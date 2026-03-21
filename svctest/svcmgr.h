#include <Windows.h>
#include <tchar.h>
#include <Sddl.h>

BOOL InstallService(TCHAR* name, TCHAR* displayName, TCHAR* description, DWORD startType, TCHAR* group, BOOL interactive, BOOL autoStart, BOOL protected);
BOOL UninstallService(TCHAR* name, TCHAR* group);
