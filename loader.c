#define DEBUG 0
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
#else
#define _tcserror strerror
#endif

LRESULT CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

DWORD exitCode;
HANDLE hChildProc;
BOOL silentFlag;

LPCTSTR MSG_USAGE = TEXT("loader.exe [/s] commandline");

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	exitCode = ERROR_SUCCESS;

	LPTSTR cmdline = GetCommandLine();
	LPTSTR *argv = __targv;
	int argc = __argc;
	if (argc < 2) {
		TCHAR message[256 * sizeof(TCHAR)];
		LoadString(GetModuleHandle(NULL), IDS_USAGE, message, sizeof(message));
		TCHAR usageMessage[_tcslen(MSG_USAGE) + _tcslen(message) + 3];
		_tcsncpy(usageMessage, message, sizeof(usageMessage));
		_tcsncat(usageMessage, TEXT(": "), sizeof(usageMessage));
		_tcsncat(usageMessage, MSG_USAGE, sizeof(usageMessage));
		MessageBox(NULL, usageMessage, message, MB_OK | MB_ICONINFORMATION);
		return ERROR_INVALID_PARAMETER;
	}
	cmdline = PathGetArgs(cmdline);
	if (argv[1][0] == _T('/')) {
		if (_totlower(argv[1][1]) == _T('s')) {
			silentFlag = TRUE;
			cmdline = PathGetArgs(cmdline);
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

				TCHAR dialogText[256 * sizeof(TCHAR)];
				LoadString(GetModuleHandle(NULL), IDS_INSTALL, dialogText, sizeof(dialogText));
				SetWindowText(hwndDlg, dialogText);

				LoadString(GetModuleHandle(NULL), IDS_CLOSE, dialogText, sizeof(dialogText));
				SetDlgItemText(hwndDlg, IDC_OK, dialogText);

				LoadString(GetModuleHandle(NULL), IDS_LOADING, dialogText, sizeof(dialogText));
				SetDlgItemText(hwndDlg, IDC_MESSAGE, dialogText);
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

							TCHAR dialogText[256 * sizeof(TCHAR)];
							if (exitCode == ERROR_SUCCESS) {
								LoadString(GetModuleHandle(NULL), IDS_SUCCESS, dialogText, sizeof(dialogText));
							} else {
								LoadString(GetModuleHandle(NULL), IDS_FAILURE, dialogText, sizeof(dialogText));
								SendMessage(hwndProg, PBM_SETSTATE, PBST_ERROR, 0);
							}
							SetDlgItemText(hwndDlg, IDC_MESSAGE, dialogText);

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
