#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <tchar.h>
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shlwapi.h"

#define NELEMS(a)  (sizeof(a) / sizeof((a)[0]))

static INT_PTR CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);

/** Global variables ********************************************************/
static HANDLE ghInstance;
static HWND list_hwnd;
char  *game_filename;
STARTUPINFO si;
PROCESS_INFORMATION pi;
void List(HWND hwnd,char *ptr);
BOOL is64bit;


typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

LPFN_ISWOW64PROCESS fnIsWow64Process;

BOOL IsWow64()
{
    BOOL bIsWow64 = FALSE;

    //IsWow64Process is not available on all supported versions of Windows.
    //Use GetModuleHandle to get a handle to the DLL that contains the function
    //and GetProcAddress to get a pointer to the function if available.

    fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(
        GetModuleHandle(TEXT("kernel32")),"IsWow64Process");

    if(NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
        {
            //handle error
        }
    }
    return bIsWow64;
}

unsigned long GetPID( char* szProcName )
{
	PROCESSENTRY32 pe32;
	HANDLE hHandle;
	hHandle = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	pe32.dwSize = sizeof( PROCESSENTRY32 );
	if( !Process32First( hHandle, &pe32 ) )
		return 0;
	while( Process32Next( hHandle, &pe32 ) )
	{
		if( strcmp( szProcName, pe32.szExeFile ) == 0 )
		{
			CloseHandle( hHandle );
			return pe32.th32ProcessID;
		}
	}
	CloseHandle( hHandle );
	return 0;
}

BOOL IsProcessRunning(DWORD pid)
{
	HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
	DWORD ret = WaitForSingleObject(process, 0);
	CloseHandle(process);
	return ret == WAIT_TIMEOUT;
}

HWND GetProcessWindow(DWORD procID) {
	while (1) {
		DWORD dwProcId;
		HWND hWnd = GetForegroundWindow();
		GetWindowThreadProcessId(hWnd,&dwProcId);
		if (dwProcId == procID) {
			List(list_hwnd,"* Got game window handle");
			return (hWnd);
		}
		Sleep(100);
	}
}

DWORD WINAPI Thread(LPVOID Value)
{
         HWND game_hwnd;
	char str[256];
	char classname[256];
	char game_filenamestr[255];
         DWORD DirectDrawID;
         DWORD dwType=REG_DWORD;
         DWORD dwSize=sizeof(DWORD);
	int classlen;
 	HKEY hKey;
	HKEY hSubKey;
   	LONG lResult;
	DWORD dwDisposition=0;
         static unsigned char Flags[4] = {
	0x00,0x08,0x00,0x00};
	
         while(1)
	{
		DWORD processID = GetPID(game_filename);
		if (IsProcessRunning(processID))
		{
			sprintf(str,"* Found %s.", game_filename);
			sprintf(game_filenamestr,"%s", game_filename);
			List(list_hwnd,str);
			game_hwnd = GetProcessWindow(processID);
                            GetClassName(game_hwnd,classname,255);
			sprintf(str,"* Window classname is: %s.",classname);
                            List(list_hwnd,str);
			List(list_hwnd,"* Opening registry for patching.");
			TerminateProcess(pi.hProcess,0);
 			CloseHandle( pi.hProcess );
        			CloseHandle( pi.hThread );
			 if (is64bit == TRUE){
 			lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
			"SOFTWARE\\Wow6432Node\\Microsoft\\DirectDraw\\MostRecentApplication", 0, KEY_ALL_ACCESS|KEY_WOW64_64KEY, &hKey);
		         }
			else
		         {
			lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
			"SOFTWARE\\Microsoft\\DirectDraw\\MostRecentApplication", 0, KEY_ALL_ACCESS, &hKey);
		         }

			if (lResult   != ERROR_SUCCESS)
		       	 {
				List(list_hwnd,"* Patching failed!");
				RegCloseKey( hKey);
		       	 }
			RegQueryValueEx(hKey, "ID", NULL, &dwType,(LPBYTE)& DirectDrawID, &dwSize);
                          	RegCloseKey( hKey);
			 if (is64bit == TRUE){
			lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
			"SOFTWARE\\Wow6432Node\\Microsoft\\DirectDraw\\Compatibility", 0, KEY_ALL_ACCESS|KEY_WOW64_64KEY, &hKey);
		         }
			else
		         {
			lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
			"SOFTWARE\\Microsoft\\DirectDraw\\Compatibility", 0, KEY_ALL_ACCESS, &hKey);
		         }
			lResult =RegCreateKey(hKey,classname,&hSubKey);
			if (lResult   != ERROR_SUCCESS)
		        {
				List(list_hwnd,"* Patching failed!");
				RegCloseKey(hKey);
		        }
                            RegSetValueEx(hSubKey,"Name",0,REG_SZ,(LPBYTE)game_filenamestr,(DWORD)strlen(game_filenamestr)+1);
			RegSetValueEx(hSubKey,"ID",0,REG_BINARY,(CONST BYTE*)&DirectDrawID,sizeof(DWORD));
			RegSetValueEx(hSubKey,"Flags",0,REG_BINARY,(CONST BYTE*)Flags,4);
			RegCloseKey(hSubKey);
			RegCloseKey(hKey);
			List(list_hwnd,"* Patching complete! Enjoy your game!");
			break;
		}
		Sleep(100);
	}
	return 0;
}

void PatchFile(HWND hwnd)
{
    DWORD  hHandle;
    OPENFILENAME ofn;
    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    char szFileName[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn); // SEE NOTE BELOW
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Executables (*.exe)\0*.exe\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "exe";

    if(GetOpenFileName(&ofn))
    {
    if( !CreateProcess( NULL,   // No module name (use command line)
       szFileName,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    ) 
    {
        List( hwnd,"* Can't load process!");
        return;
    }
        game_filename = PathFindFileName((const char*)szFileName);
        CreateThread(NULL,0,Thread,NULL,0,&hHandle);
    }
}



int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    INITCOMMONCONTROLSEX icc;
    WNDCLASSEX wcx;
    ghInstance = hInstance;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES /*|ICC_COOL_CLASSES|ICC_DATE_CLASSES|ICC_PAGESCROLLER_CLASS|ICC_USEREX_CLASSES|... */;
    InitCommonControlsEx(&icc);
    wcx.cbSize = sizeof(wcx);
    if (!GetClassInfoEx(NULL, MAKEINTRESOURCE(32770), &wcx))
        return 0;
    wcx.hInstance = hInstance;
    wcx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_ICO_MAIN));
    wcx.lpszClassName = _T("w7ddpatcClass");
    if (!RegisterClassEx(&wcx))
        return 0;
    is64bit = IsWow64();
    return DialogBox(hInstance, MAKEINTRESOURCE(DLG_MAIN), NULL, (DLGPROC)MainDlgProc);
}

static INT_PTR CALLBACK MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
            list_hwnd = hwndDlg;
	   List( hwndDlg,"-=[w7ddpatcher by mudlord]=-");
	  if (is64bit == TRUE){
				List( hwndDlg,"-=[for  Win7/Vista x64]=-");
	  }
	  else
	  {
				List( hwndDlg,"-=[for  Win7/Vista x86]=-");
	  }
	   List( hwndDlg,"* Press 'Patch' to patch a application...");
            return TRUE;
        case WM_COMMAND:
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
                /*
                 * TODO: Add more control ID's, when needed.
                 */
	       case IDOK:
	  	 PatchFile(hwndDlg);
		 return TRUE;
                case IDCANCEL:
                    EndDialog(hwndDlg, TRUE);
                    return TRUE;
            }
            break;
        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            return TRUE;
        /*
         * TODO: Add more messages, when needed.
         */
    }

    return FALSE;
}

void List(HWND hwnd,char *ptr)
{
	HWND hList = GetDlgItem(hwnd,IDC_LOG);
	SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)ptr);
}
