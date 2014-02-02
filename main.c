/*
w7ddpatcher - enable Windows 7 DirectDraw palette compatibility fix for a given
program.

Certain games such as Starcraft 1, Warcraft 2 BNE, Age of Empires, Diablo 1,
and others suffer from a "rainbow glitch", when some other program modifies
global Windows color palette while the game is running, causing some objects to
be rendered with wrong colors. Most often this program is Explorer.exe, so
killing it before starting the game usually fixes the glitch, but it is
annoying to do.

Windows 7 (and maybe Vista) has a registry setting for enabling backward
compatible DirectDraw behaviour for a given application which fixes this
glitch, but setting it up manually is bothersome because it requires collecting
certain technical data about the application to let Windows identify it.

This program prompts for an application executable, launches it, collects said
data, closes it, and creates the registry key enabling the fix.

This requires administrator privileges, of course.

Originally written by Mudlord from
http://www.sevenforums.com/gaming/2981-starcraft-fix-holy-cow-22.html, but the
archive contained the C source only, so in order to compile it myself I had to
create a VS project, convert it to a console application (VS Express doesn't
even include a win32api GUI resource editor), throw away all now-unnecessary
GUI stuff, and improve error reporting a bit. Now it compiles with VS 2013
Express for Windows Desktop.

*/

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include "shlwapi.h"

BOOL is64bit;

BOOL IsWow64()
{
	typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process;

	BOOL bIsWow64 = FALSE;

	//IsWow64Process is not available on all supported versions of Windows.
	//Use GetModuleHandle to get a handle to the DLL that contains the function
	//and GetProcAddress to get a pointer to the function if available.

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
		GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if (NULL != fnIsWow64Process)
	{
		if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		{
			//handle error
		}
	}
	return bIsWow64;
}

void LogLastError(const char * operation_description)
{
	LPTSTR error_str = 0;
	DWORD err = GetLastError();
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		err,
		0,
		(LPTSTR)&error_str,
		0,
		NULL) == 0)
	{
		printf("Failed to FormatMessage for error %u\n", err);
	}
	else
	{
		printf("Failed to %s: error %u: %s\n", operation_description, err, error_str);
		LocalFree(error_str);
	}
}

unsigned long GetPID(const char* szProcName)
{
	PROCESSENTRY32 pe32;
	HANDLE hHandle;
	hHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hHandle, &pe32))
		return 0;
	while (Process32Next(hHandle, &pe32))
	{
		if (strcmp(szProcName, pe32.szExeFile) == 0)
		{
			CloseHandle(hHandle);
			return pe32.th32ProcessID;
		}
	}
	CloseHandle(hHandle);
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
		GetWindowThreadProcessId(hWnd, &dwProcId);
		if (dwProcId == procID) {
			printf("Got game window handle.\n");
			return (hWnd);
		}
		Sleep(100);
	}
}

void WaitForGame(const char * game_filename, PROCESS_INFORMATION * pi)
{
#define CHECK(cmd, error_msg) do { LONG lResult = cmd; \
	if (lResult != ERROR_SUCCESS) { LogLastError(error_msg); return; } \
} while (0)

	HWND game_hwnd;
	char classname[256];
	DWORD directDrawID;
	char recentAppName[256];
	DWORD size, expected_size;
	HKEY hKey;
	HKEY hSubKey;
	DWORD dwDisposition = 0;
	static unsigned char Flags[4] = {
		0x00, 0x08, 0x00, 0x00 };

	printf("Waiting for the game to start...\n");

	while (1)
	{
		DWORD processID = GetPID(game_filename);
		if (IsProcessRunning(processID))
		{
			printf("Found %s.\n", game_filename);
			game_hwnd = GetProcessWindow(processID);
			GetClassName(game_hwnd, classname, 255);
			printf("Window classname is: %s.\n", classname);
			printf("Opening registry for patching.\n");
			TerminateProcess(pi->hProcess, 0);
			CloseHandle(pi->hProcess);
			CloseHandle(pi->hThread);
			if (is64bit){
				CHECK(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					"SOFTWARE\\Wow6432Node\\Microsoft\\DirectDraw\\MostRecentApplication", 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey),
					"open DirectDraw\\MostRecentApplication");
			}
			else
			{
				CHECK(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					"SOFTWARE\\Microsoft\\DirectDraw\\MostRecentApplication", 0, KEY_ALL_ACCESS, &hKey),
					"open DirectDraw\\MostRecentApplication");
			}

			
			size = expected_size = sizeof(directDrawID);
			CHECK(RegQueryValueEx(hKey, "ID", NULL, NULL, (LPBYTE)& directDrawID, &size),
				"query DirectDraw\\MostRecentApplication\\ID");
			if (size != expected_size)
			{
				printf("error: DirectDraw\\MostRecentApplication\\ID has wrong size: %u (expected %u)\n",
					size, expected_size);
				return;
			}

			// just to be sure, check that the recent application is in fact the one we are interested in.
			size = expected_size = sizeof(recentAppName) - 1;
			CHECK(RegQueryValueEx(hKey, "Name", NULL, NULL, (LPBYTE)recentAppName, &size),
				"query DirectDraw\\MostRecentApplication\\Name");
			if (size > expected_size)
			{
				printf("error: DirectDraw\\MostRecentApplication\\ID has wrong size: %u (expected %u or less)\n",
					size, expected_size);
				return;
			}
			recentAppName[size] = '\0';
			if (strcmp(recentAppName, game_filename))
			{
				printf("error: DirectDraw\\MostRecentApplication\\Name doesn't match the game file name: '%s' != '%s'\n",
					recentAppName, game_filename);
			}


			RegCloseKey(hKey);
			if (is64bit == TRUE){
				CHECK(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					"SOFTWARE\\Wow6432Node\\Microsoft\\DirectDraw\\Compatibility", 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey),
					"open DirectDraw\\Compatibility");

			}
			else
			{
				CHECK(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					"SOFTWARE\\Microsoft\\DirectDraw\\Compatibility", 0, KEY_ALL_ACCESS, &hKey),
					"open DirectDraw\\Compatibility");
			}
			CHECK(RegCreateKey(hKey, classname, &hSubKey),
				"create DirectDraw\\Compatibility subkey for window classname");

			CHECK(RegSetValueEx(hSubKey, "Name", 0, REG_SZ, (LPBYTE)game_filename, (DWORD)strlen(game_filename) + 1),
				"set Name");
			CHECK(RegSetValueEx(hSubKey, "ID", 0, REG_BINARY, (CONST BYTE*)&directDrawID, sizeof(directDrawID)),
				"set ID");
			CHECK(RegSetValueEx(hSubKey, "Flags", 0, REG_BINARY, (CONST BYTE*)Flags, 4),
				"set Flags");

			RegCloseKey(hSubKey);
			RegCloseKey(hKey);
			printf("Patching complete! Enjoy your game!\n");
			return;
		}
		Sleep(100);
	}
}

void main()
{
	is64bit = IsWow64();
	printf("-=[w7ddpatcher by mudlord]=-\n");
	printf("-=[for  Win7/Vista %s]=-\n", is64bit ? "x64" : "x86");

	OPENFILENAME ofn = {.lStructSize = sizeof(ofn)};
	STARTUPINFO si = {.cb = sizeof(si)};
	PROCESS_INFORMATION pi = {0};

	char szFileName[MAX_PATH] = "";

	ofn.hwndOwner = NULL;
	ofn.lpstrFilter = "Executables (*.exe)\0*.exe\0";
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = "exe";

	printf("Waiting for executable file name...\n");
	if (GetOpenFileName(&ofn))
	{
		if (!CreateProcess(NULL,   // No module name (use command line)
			szFileName,        // Command line
			NULL,           // Process handle not inheritable
			NULL,           // Thread handle not inheritable
			FALSE,          // Set handle inheritance to FALSE
			0,              // No creation flags
			NULL,           // Use parent's environment block
			NULL,           // Use parent's starting directory 
			&si,            // Pointer to STARTUPINFO structure
			&pi)           // Pointer to PROCESS_INFORMATION structure
			)
		{
			printf("Can't load process!\n");
			return;
		}
		WaitForGame(PathFindFileName(szFileName), &pi);
	}
	else
	{
		printf("Cancelled\n");
	}

	int ch;
	printf("\nPress any key to exit...\n");
	ch = _getch();
}

