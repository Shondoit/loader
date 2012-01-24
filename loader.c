#define _WIN32_WINNT 0x0501
#define __MSVCRT_VERSION__ 0x0700

#include <windows.h>
#include <commctrl.h>
#include <ctype.h>
#include <tchar.h>
#include <shlwapi.h>
#include "resource.h"

#define PBS_MARQUEE 0x08
#define PBM_SETMARQUEE (WM_USER + 10)
#define PBM_SETSTATE 0x0410
#define PBST_ERROR 0x02

#define IDT_TIMER1 1001

#ifdef _UNICODE
#define _tcserror _wcserror
#define __targv __wargv
#else
#define _tcserror strerror
#define __targv __argv
#endif

void showUsage();
LRESULT CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

DWORD exitCode;
HANDLE hChildProc;
BOOL silentFlag;
BOOL uninstallFlag;
int estTime;

LPCTSTR MSG_USAGE = TEXT("[/s] [/t=min] [/u]");

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	exitCode = ERROR_SUCCESS;

	LPTSTR cmdline = GetCommandLine();
	LPSTR *argv = __argv;
	int argc = __argc;
	if (argc < 2) {
		showUsage();
		return ERROR_INVALID_PARAMETER;
	}
	cmdline = PathGetArgs(cmdline);
	int index = 1;
	BOOL switchMatch;
	while (index < argc) {
		switchMatch = FALSE;
		LPSTR arg = argv[index];
		if (strlen(arg) >= 2) {
			switch (arg[0]) {
				case '-':
				case '/':
					switch (tolower(arg[1])) {
						case 's':
							if (strlen(arg) == 2) {
								switchMatch = TRUE;
								silentFlag = TRUE;
							}
							break;
						case 'u':
							if (strlen(arg) == 2) {
								switchMatch = TRUE;
								uninstallFlag = TRUE;
							}
							break;
						case 't':
							if (strlen(arg) > 3 && arg[2] == '=') {
								switchMatch = TRUE;
								estTime = atoi(arg + 3);
							}
							break;
					}
					break;
			}
		}
		if (switchMatch) {
			cmdline = PathGetArgs(cmdline);
			index++;
		} else {
			index = argc;
		}
	}

	STARTUPINFO startInfo;
	PROCESS_INFORMATION procInfo;
	ZeroMemory(&startInfo, sizeof(startInfo));
	startInfo.cb = sizeof(startInfo);
	ZeroMemory(&procInfo, sizeof(procInfo));

	if (!CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &startInfo, &procInfo)) {
		exitCode = GetLastError();
		MessageBox(NULL, _tcserror(exitCode), NULL, MB_ICONERROR);
		return exitCode;
	}
	hChildProc = procInfo.hProcess;

	DialogBox(hInstance, MAKEINTRESOURCE(IDD_LOADER), 0, (DLGPROC)DlgProc);

	CloseHandle(procInfo.hProcess);
	CloseHandle(procInfo.hThread);

	return exitCode;
}

void showUsage() {
	LPTSTR moduleName = (LPTSTR)malloc(MAX_PATH * sizeof(TCHAR));
	GetModuleFileName(NULL, moduleName, MAX_PATH);
	PathStripPath(moduleName);

	LPTSTR message = (LPTSTR)malloc(MAX_STRING_LENGTH * sizeof(TCHAR));
	LoadString(NULL, IDS_USAGE, message, MAX_STRING_LENGTH);

	DWORD_PTR messageArguments[] = { (DWORD_PTR)moduleName, (DWORD_PTR)MSG_USAGE };

	HLOCAL usageMessage = NULL;
	DWORD formatFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING |  FORMAT_MESSAGE_ARGUMENT_ARRAY;
	FormatMessage(formatFlags, message, IDS_USAGE, 0, (LPTSTR)&usageMessage, 0, (va_list*)messageArguments);

	MessageBox(NULL, (LPTSTR)usageMessage, NULL, MB_OK | MB_ICONINFORMATION);

	LocalFree(usageMessage);
	free(moduleName);
	free(message);
}

LRESULT CALLBACK DlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_INITDIALOG:
			{
				RECT rectOwner, rectDlg, rectDiff;

				GetWindowRect(GetDesktopWindow(), &rectOwner);
				GetWindowRect(hwndDlg, &rectDlg);
				CopyRect(&rectDiff, &rectOwner);

				OffsetRect(&rectDlg, -rectDlg.left, -rectDlg.top);
				OffsetRect(&rectDiff, -rectDiff.left, -rectDiff.top);
				OffsetRect(&rectDiff, -rectDlg.right, -rectDlg.bottom);

				SetWindowPos(hwndDlg, HWND_TOP, rectOwner.left + (rectDiff.right / 2), rectOwner.top + (rectDiff.bottom / 2), 0, 0, SWP_NOSIZE);


				HWND hwndProg = GetDlgItem(hwndDlg, IDC_PROGRESS);
				DWORD dwStyle = GetWindowLong(hwndProg, GWL_STYLE);
				SetWindowLong(hwndProg, GWL_STYLE, dwStyle | PBS_MARQUEE);
				SendMessage(hwndProg, PBM_SETMARQUEE, TRUE, 70 /* = scroll speed */);

				HWND hwndOK = GetDlgItem(hwndDlg, IDC_OK);
				if (silentFlag) {
					RECT rectProg, rectOK;
					GetWindowRect(hwndProg, &rectProg);
					GetWindowRect(hwndOK, &rectOK);

					POINT posProg;
					posProg.x = rectProg.left;
					posProg.y = rectProg.top;
					ScreenToClient(hwndDlg, &posProg);

					MoveWindow(hwndProg, posProg.x, posProg.y, rectOK.right - rectProg.left, rectProg.bottom - rectProg.top, TRUE);

					ShowWindow(hwndOK, SW_HIDE);
				} else {
					EnableWindow(hwndOK, FALSE);
				}

				LPTSTR dialogText = (LPTSTR)malloc(MAX_STRING_LENGTH * sizeof(TCHAR));
				UINT titleID = (uninstallFlag ? IDS_UNINSTALL : IDS_INSTALL);
				LoadString(GetModuleHandle(NULL), titleID, dialogText, MAX_STRING_LENGTH);
				SetWindowText(hwndDlg, dialogText);

				LoadString(GetModuleHandle(NULL), IDS_CLOSE, dialogText, MAX_STRING_LENGTH);
				SetDlgItemText(hwndDlg, IDC_OK, dialogText);

				LPTSTR message = (LPTSTR)malloc((2 * MAX_STRING_LENGTH + 10) * sizeof(TCHAR));
				UINT loadingID = (uninstallFlag ? IDS_LOADING_U : IDS_LOADING);
				LoadString(GetModuleHandle(NULL), loadingID, message, MAX_STRING_LENGTH);

				loadingID = (estTime) ? IDS_TIMESPEC : IDS_TIMEUNDEF;
				LoadString(GetModuleHandle(NULL), loadingID, dialogText, MAX_STRING_LENGTH);

				LPTSTR time = (LPTSTR)malloc(9 * sizeof(TCHAR));
				_itot(estTime, time, 10);
				DWORD_PTR messageArguments[] = { (DWORD_PTR)time };
				HLOCAL formattedString = NULL;
				DWORD formatFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING |  FORMAT_MESSAGE_ARGUMENT_ARRAY;
				FormatMessage(formatFlags, dialogText, loadingID, 0, (LPTSTR)&formattedString, 0, (va_list*)messageArguments);
				free(time);

				_tcsncat(message, _T("\n"), 2 * MAX_STRING_LENGTH + 10);
				_tcsncat(message, formattedString, 2 * MAX_STRING_LENGTH + 10);

				SetDlgItemText(hwndDlg, IDC_MESSAGE, message);

				LocalFree(formattedString);
				free(dialogText);
				free(message);
			}

			SetTimer(hwndDlg, IDT_TIMER1, 500, (TIMERPROC)NULL);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
				case IDCANCEL:
					if (hChildProc == 0) {
						EndDialog(hwndDlg, wParam);
						return TRUE;
					}
					break;
			}
			break;
		case WM_TIMER:
			switch (wParam) {
				case IDT_TIMER1:
					GetExitCodeProcess(hChildProc, &exitCode);
					if (exitCode != STILL_ACTIVE) {
						KillTimer(hwndDlg, IDT_TIMER1);

						hChildProc = 0;

						if (silentFlag) {
							EndDialog(hwndDlg, 0);
						} else {
							FlashWindow(hwndDlg, TRUE);

							HWND hwndProg;
							DWORD dwStyle;
							hwndProg = GetDlgItem(hwndDlg, IDC_PROGRESS);

							SendMessage(hwndProg, PBM_SETMARQUEE, FALSE, 0);
							dwStyle = GetWindowLong(hwndProg, GWL_STYLE);
							SetWindowLong(hwndProg, GWL_STYLE, dwStyle & ~PBS_MARQUEE);
							SendMessage(hwndProg, PBM_SETPOS, 100, 0);

							LPTSTR dialogText = (LPTSTR)malloc(MAX_STRING_LENGTH * sizeof(TCHAR));
							if (exitCode == ERROR_SUCCESS) {
								LoadString(GetModuleHandle(NULL), IDS_SUCCESS, dialogText, MAX_STRING_LENGTH);
							} else {
								LoadString(GetModuleHandle(NULL), IDS_FAILURE, dialogText, MAX_STRING_LENGTH);
								SendMessage(hwndProg, PBM_SETSTATE, PBST_ERROR, 0);
							}
							SetDlgItemText(hwndDlg, IDC_MESSAGE, dialogText);
							free(dialogText);

							HWND hwndOK = GetDlgItem(hwndDlg, IDC_OK);
							EnableWindow(hwndOK, TRUE);
						}
					}
					break;
			}
			break;
	}

	return FALSE;
}
