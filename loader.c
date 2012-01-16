#define DEBUG 0
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <wincon.h>
#include <commctrl.h>
#include <ctype.h>
#include <tchar.h>
#include <stdio.h>
#include "resource.h"
#include "dwconsole.h"

#define PBS_MARQUEE 0x08
#define PBM_SETMARQUEE (WM_USER + 10)
#define PBM_SETSTATE 0x0410
#define PBST_ERROR 0x02

#define IDT_TIMER1 1001

LRESULT CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
void striparg(LPWSTR *cmdline);
LPCTSTR loadResString(UINT id);

DWORD exitCode;
HANDLE hChildProc;
BOOL silentFlag;
HMODULE hMod;


LPCTSTR MSG_LOADING = TEXT("Please wait while the application is being installed.\nThis might take a while...");
LPCTSTR MSG_SUCCESS = TEXT("Application installed succesfully.");
LPCTSTR MSG_FAILURE = TEXT("An error occured during installation of the application.\nPlease contact the Service Desk and ask for assistance.");

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	exitCode = ERROR_SUCCESS;

	int argc;
	LPWSTR cmdline = GetCommandLineW();
	LPWSTR *argv = CommandLineToArgvW(cmdline, &argc);
	if (argc < 2) {
		MessageBox(NULL, TEXT("Usage: loader.exe [/s] cmdline"), TEXT("Usage"), MB_OK | MB_ICONINFORMATION);
		return ERROR_INVALID_PARAMETER;
	}
	striparg(&cmdline);
	if (argv[1][0] == '/') {
		if (wcslen(argv[1]) == 2 && tolower(argv[1][1]) == 's') {
			silentFlag = TRUE;
			striparg(&cmdline);
		}
	}

	STARTUPINFOW startInfo;
	PROCESS_INFORMATION procInfo;
	ZeroMemory(&startInfo, sizeof(startInfo));
	startInfo.cb = sizeof(startInfo);
	ZeroMemory(&procInfo, sizeof(procInfo));

	if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &startInfo, &procInfo)) {
		exitCode = GetLastError();
		MessageBox(NULL, strerror(exitCode), NULL, MB_ICONERROR);
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

				//DEBUG
				SetDlgItemText(hwndDlg, IDC_MESSAGE, loadResString(IDS_LOADING));
			}

			SetTimer(hwndDlg, IDT_TIMER1, 500, (TIMERPROC)NULL);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					if (hChildProc == 0) EndDialog(hwndDlg, wParam);
					return TRUE;
					break;
#if DEBUG
				case IDCANCEL:
					EndDialog(hwndDlg, wParam);
					return TRUE;
					break;
#endif
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

							if (exitCode == ERROR_SUCCESS) {
								//SetDlgItemText(hwndDlg, IDC_MESSAGE, loadResString(IDS_SUCCESS));
							} else {
								SendMessage(hwndProg, PBM_SETSTATE, PBST_ERROR, 0);
								//SetDlgItemText(hwndDlg, IDC_MESSAGE, loadResString(IDS_FAILURE));
							}

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

LPCTSTR loadResString(UINT id) {
	if (!hMod) hMod = GetModuleHandle(NULL);
	
	LPCTSTR result = NULL;
	LoadString(hMod, id, (LPTSTR)&result, 0);

	AttachConsole(ATTACH_PARENT_PROCESS);
	ReplaceHandle(STD_OUTPUT_HANDLE);
	fprintf(stdout, "test");
	FreeConsole();

	MessageBox(NULL, strerror(GetLastError()), NULL, MB_OK);
	return result;
}


void striparg(LPWSTR *cmdline) {
	while (isspace(**cmdline)) (*cmdline)++;

	BOOL quoted = FALSE;
	BOOL escape = FALSE;

	while (**cmdline && (!isspace(**cmdline) || quoted)) {
		if (quoted && **cmdline == '\\') {
			escape = !escape;
		} else {
			if (**cmdline == '\"') {
				if (!quoted) {
					quoted = TRUE;
				} else if (!escape) {
					quoted = FALSE;
				}
			}
			escape = FALSE;
		}
		(*cmdline)++;
	}

	while (isspace(**cmdline)) (*cmdline)++;
}