#include <Windows.h>
#include <tchar.h>
#include "svcmgr.h"

HINSTANCE hInst = NULL;

const TCHAR* serviceName = _T("RheostaticControl2");			/* 서비스 이름 */
const TCHAR* displayName = _T("Rheostatic Control Train 2");	/* 표시 이름 */
const TCHAR* description = _T("이 서비스를 실행하면 개조저항을 기분좋게 탈 수 있습니다.");
const TCHAR* serviceGroup = _T("seoulmetro");					/* 서비스 그룹 (Windows 10 미만에서 동일 프로세스에서 실행) */
const BOOL protected = FALSE;									/* 서비스 중지 / 시작 유형 변경 금지 여부 */
const BOOL interactive = TRUE;									/* 서비스가 UI/창을 표시할 수 있는지 여부(Windows 10 1709부터 작동 안 함) */

__declspec(dllexport) HRESULT WINAPI DllRegisterServer(void)
{
	if(InstallService(serviceName, displayName, description, SERVICE_DEMAND_START, serviceGroup, interactive, FALSE, protected))
		return S_OK;
	else
		return E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DllUnregisterServer(void)
{
	if(UninstallService(serviceName, serviceGroup))
		return S_OK;
	else
		return E_FAIL;
}

__declspec(dllexport) BOOL WINAPI DllMain(hInstDLL, fdwReason, lpvReserved)
    HINSTANCE hInstDLL;
    DWORD fdwReason;
    LPVOID lpvReserved;
{
    switch(fdwReason) {
        case DLL_PROCESS_ATTACH: {
            hInst = hInstDLL;
		}
    }
    return TRUE;
}
