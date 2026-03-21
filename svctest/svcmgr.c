#include "svcmgr.h"

#define MAX_SERVICE_NAME_LENGTH 32
#define MAX_SERVICE_GROUP_LENGTH 32

VOID ProtectService(SC_HANDLE hService);
VOID UnprotectService(SC_HANDLE hService);

BOOL InstallService(name, displayName, description, startType, group, interactive, autoStart, protected)
	TCHAR* name;			/* 서비스 이름 */
	TCHAR* displayName;		/* 표시 이름 */
	TCHAR* description;		/* 서비스 설명 */
	DWORD startType;        /* 시작 유형 */
	TCHAR* group;			/* 그룹 이름 */
	BOOL interactive;		/* 대화형 서비스 여부 */
	BOOL autoStart;			/* 서비스 자동 시작 여부 */
	BOOL protected;		/* 서비스 보호 여부 */
{
	BOOL ret = FALSE, failed = FALSE;

	HKEY hKey = NULL;

	SC_HANDLE hSCM = NULL, hService = NULL;
	SERVICE_DESCRIPTION sd;

	TCHAR path[37 + MAX_SERVICE_GROUP_LENGTH + 1];
	TCHAR key[45 + MAX_SERVICE_NAME_LENGTH + 1];

	size_t nameSize = _tcslen(name);
	
	/* 오버플로우 방지 */
	if(nameSize > MAX_SERVICE_NAME_LENGTH) return FALSE;
	if(_tcslen(group) > MAX_SERVICE_GROUP_LENGTH) return FALSE;

	/* 서비스 제어 관리자 열기 */
	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE | SC_MANAGER_CONNECT);
	if(!hSCM) return FALSE;

	/* 동일 이름의 서비스가 있으면 먼저 지우기 (덮어쓰기) */
	UninstallService(name, group);

	/* 서비스 설치 */
	_stprintf(path, _T("%%SystemRoot%%\\System32\\svchost.exe -k %s"), group);  /* 이미 위에서 수동으로 길이 검사를 했으므로 _s는 없어도 됨 */
	hService = CreateService(hSCM, name, displayName, SERVICE_ALL_ACCESS, SERVICE_WIN32_SHARE_PROCESS | (interactive ? SERVICE_INTERACTIVE_PROCESS : 0), startType, SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL);
	if(!hService) goto cleanup;
	_stprintf(key, _T("SYSTEM\\CurrentControlSet\\Services\\%s\\Parameters"), name);
	if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, key, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
		TCHAR modulePath[MAX_PATH];
		HMODULE hMod = NULL;
		if(GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR) InstallService, &hMod)) {
			if(GetModuleFileName(hMod, modulePath, MAX_PATH)) {
				RegSetValueEx(hKey, _T("ServiceDll"), 0, REG_EXPAND_SZ, (BYTE*) modulePath, (DWORD) (_tcslen(modulePath) + 1) * sizeof(TCHAR));
			} else failed = TRUE;
		} else failed = TRUE;
		RegCloseKey(hKey);
	} else failed = TRUE;
	if(failed) goto cleanup;

	/* 서비스를 그룹에 등록 */
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost"), 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
		DWORD size = 0, type = 0;
		TCHAR *buf = NULL, *p;
		BOOL exists = FALSE;
		LSTATUS queryRet;

		queryRet = RegQueryValueEx(hKey, group, NULL, &type, NULL, &size);
		if(queryRet == ERROR_SUCCESS) {
			buf = (TCHAR*) malloc(size + ((nameSize + 1) * sizeof(TCHAR)));
			if(!buf) {
				failed = TRUE;
			} else if(RegQueryValueEx(hKey, group, NULL, &type, (BYTE*) buf, &size) == ERROR_SUCCESS) {
				p = buf;
				while(*p) {
					if(_tcsicmp(p, name) == 0) {
						exists = TRUE;
						break;
					}
					p += _tcslen(p) + 1;
				}
				if(!exists) {
					_tcscpy(p, name);
					p += nameSize + 1;
					*p = _T('\0');
					size = (DWORD) ((BYTE*) p - (BYTE*) buf) + sizeof(TCHAR);
					if(RegSetValueEx(hKey, group, 0, REG_MULTI_SZ, (BYTE*) buf, size) != ERROR_SUCCESS) failed = TRUE;
				}
				free(buf);
			} else failed = TRUE;
		} else if(queryRet == ERROR_FILE_NOT_FOUND) {
			TCHAR* data = (TCHAR*) malloc((nameSize + 2) * sizeof(TCHAR));
			if(data) {
				p = data;
				_tcscpy(p, name);
				p += nameSize + 1;
				*p = _T('\0');
				size = (DWORD) ((BYTE*) p - (BYTE*) data) + sizeof(TCHAR);
				if(RegSetValueEx(hKey, group, 0, REG_MULTI_SZ, (BYTE*) data, size) != ERROR_SUCCESS) failed = TRUE;
				free(data);
			} else failed = TRUE;
		} else failed = TRUE;
		RegCloseKey(hKey);
	} else failed = TRUE;
	if(failed) goto cleanup;

	/* 서비스 설명 지정 */
	sd.lpDescription = description;
	ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &sd);

	if(autoStart) StartService(hService, 0, NULL);
	if(protected) ProtectService(hService);
	ret = TRUE;
	
cleanup:
	if(failed) DeleteService(hService);
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);
	return ret;
}

BOOL UninstallService(name, group)
    TCHAR* name;	/* 서비스 이름 */
	TCHAR* group;	/* 서비스 그룹 */
{
	SC_HANDLE hSCM = NULL, hService = NULL;
	SERVICE_STATUS_PROCESS ssp;
	SERVICE_STATUS status;
	DWORD bytesNeeded;
	BOOL ret = FALSE;

	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if(!hSCM) return FALSE;
	
	/* 서비스 잠금 해제 */
	hService = OpenService(hSCM, name, WRITE_DAC | SERVICE_QUERY_CONFIG);
	if(!hService) goto cleanup;
	UnprotectService(hService);
	CloseServiceHandle(hService);

	/* 서비스 중지 및 삭제 */
	hService = OpenService(hSCM, name, SERVICE_STOP | DELETE);
	if(!hService) goto cleanup;
	if(ControlService(hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS) &ssp))
        while(QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE) &ssp, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
            if(ssp.dwCurrentState == SERVICE_STOPPED) break;
            Sleep(100);
        }
	ControlService(hService, SERVICE_CONTROL_STOP, &status);
	if(DeleteService(hService)) ret = TRUE;
	CloseServiceHandle(hService);

	if(ret) {
		/* 그룹 등록 해제 */
		HKEY hKey;
		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost"), 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
			DWORD size = 0, type = 0;
			TCHAR *buf = NULL, *nbuf = NULL, *p, *np;
			size_t pl;
			BOOL exists = FALSE;

			if(RegQueryValueEx(hKey, group, NULL, &type, NULL, &size) == ERROR_SUCCESS) {
				buf = (TCHAR*) malloc(size), nbuf = (TCHAR*) malloc(size);
				if(buf && nbuf && RegQueryValueEx(hKey, group, NULL, &type, (BYTE*) buf, &size) == ERROR_SUCCESS) {
					memset(nbuf, 0, size);
					p = buf, np = nbuf;
					while(*p) {
						pl = _tcslen(p);
						if(_tcsicmp(p, name) == 0) {
							exists = TRUE;
						} else {
							_tcscpy(np, p);
							np += pl + 1;
						}
						p += pl + 1;
					}
					if(exists) {
						*np = _T('\0');  /* 이미 memset으로 널로 만들어서 굳이 필요는 없지만... */
						size = (DWORD) ((BYTE*) np - (BYTE*) nbuf) + sizeof(TCHAR);
						RegSetValueEx(hKey, group, 0, REG_MULTI_SZ, (BYTE*) nbuf, size);
					}
				}
				if(buf) free(buf);
				if(nbuf) free(nbuf);
			}
			RegCloseKey(hKey);
		}
	}

cleanup:
	CloseServiceHandle(hSCM);
	return ret;
}

VOID ProtectService(hService)
	SC_HANDLE hService;		/* 잠글 서비스의 핸들 */
{
	/* const TCHAR* sddl = _T("D:(D;;0x00022;;;WD)"); <- services.msc에 안 나타남 */
	const TCHAR* sddl = _T("D:(D;;DCWP;;;WD)(A;;GA;;;BA)(A;;CCLCSWLOCRRC;;;WD)");  /* (A;;GA;;;BA)는 서비스 제거 시 필요 */
	PSECURITY_DESCRIPTOR pSD = NULL;
	if(!ConvertStringSecurityDescriptorToSecurityDescriptor(sddl, SDDL_REVISION_1, &pSD, NULL)) return;  /* 실패 */
	SetServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, pSD);
	LocalFree(pSD);
}

VOID UnprotectService(hService)
	SC_HANDLE hService;		/* 잠금 해제할 서비스의 핸들 */
{
	const TCHAR* sddl = _T("D:(A;;GA;;;WD)");
	PSECURITY_DESCRIPTOR pSD = NULL;
	if(!ConvertStringSecurityDescriptorToSecurityDescriptor(sddl, SDDL_REVISION_1, &pSD, NULL)) return;  /* 실패 */
	SetServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, pSD);
	LocalFree(pSD);
}
