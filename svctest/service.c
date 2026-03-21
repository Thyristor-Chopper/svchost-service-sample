#include <Windows.h>

const BOOL stoppable = TRUE;  /* 서비스 중지 가능 여부 */

SERVICE_STATUS status = { 0 };
SERVICE_STATUS_HANDLE statusHandle = NULL;
HANDLE stopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ControlHandler(ctrl)
	DWORD ctrl;
{
	switch(ctrl) {
		/* 서비스 중지 응답 */
		case SERVICE_CONTROL_STOP: {
			if(status.dwCurrentState == SERVICE_RUNNING) {
				status.dwCurrentState = SERVICE_STOP_PENDING;
				SetServiceStatus(statusHandle, &status);
				SetEvent(stopEvent);
			}
		}
	}
}

DWORD WINAPI WorkerThread(param)
	LPVOID param;
{
	do {
		/* 1분마다 여기 있는 코드 실행 */
		MessageBox(NULL, TEXT("안녕하세요"), TEXT("알림"), MB_OK | MB_ICONINFORMATION);
	} while(WaitForSingleObject(stopEvent, 60000) == WAIT_TIMEOUT);

	return ERROR_SUCCESS;
}

__declspec(dllexport) VOID WINAPI ServiceMain(argc, argv)
	DWORD argc;
	LPTSTR* argv;
{
	HANDLE hThread = NULL;

	statusHandle = RegisterServiceCtrlHandler(argv[0], ControlHandler);
	if(!statusHandle) return;

	status.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
	status.dwServiceSpecificExitCode = 0;

	if(stoppable)
		status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

	/* 서비스 시작 중 */
	status.dwCurrentState = SERVICE_START_PENDING;
	SetServiceStatus(statusHandle, &status);

	/* 서비스 중지 이벤트 */
	stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	/* 서비스 실행 */
	status.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(statusHandle, &status);

	/* 서비스가 실행할 작업의 쓰레드 생성 */
	hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);

	/* 서비스 종료 시까지 대기 */
	WaitForSingleObject(stopEvent, INFINITE);

	/* 서비스 중지 */
	status.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(statusHandle, &status);
}
