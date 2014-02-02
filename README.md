w7ddpatcher
===========

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
