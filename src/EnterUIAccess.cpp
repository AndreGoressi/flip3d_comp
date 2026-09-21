#include <windows.h>
#include <tchar.h>
#include "PrepareForUIAccess.h"
#include "resource.h"

static HINSTANCE g_hInstance = NULL;
static HWND g_hdlg = NULL;
static BOOL g_fHasUIAccess;
static BOOL g_fAlwaysTop = TRUE;

static void SetTopmostStatus(BOOL fAlwaysTop)
{
	if (!g_hdlg) return;
	DWORD dwFlags, dwExStyle;
	HWND hwndIns;

	dwFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
	hwndIns = fAlwaysTop ? HWND_TOPMOST : HWND_NOTOPMOST;
	SetWindowPos(g_hdlg, hwndIns, 0, 0, 0, 0, dwFlags);

	dwExStyle = (DWORD)GetWindowLongPtr(g_hdlg, GWL_EXSTYLE);
	g_fAlwaysTop = dwExStyle & WS_EX_TOPMOST;
}

static INT_PTR CALLBACK DialogProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM lParam){
    switch (uMsg){
    case WM_COMMAND:
		{
			UINT id = LOWORD(wParam), code = HIWORD(wParam);
			switch (id){
			case IDC_MAIN_TOP:
				SetTopmostStatus(g_fAlwaysTop);
				break;
            }
        }
		return 0;

    case WM_INITDIALOG:
		g_hdlg = hdlg;
		SetTopmostStatus(g_fAlwaysTop);
        return TRUE;
    }
    return FALSE;
}

static int InitInstance(HINSTANCE hInstance)
{
	DWORD dwErr;
	INT_PTR iResult;

	dbgstart();

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	dwErr = PrepareForUIAccess();
	if (ERROR_SUCCESS != dwErr)
		dbg("UIAccess error: 0x%08X\n", dwErr);
	g_fHasUIAccess = ERROR_SUCCESS == dwErr;

	g_hInstance = hInstance;

	iResult = DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DialogProc);
	g_hdlg = NULL;

	CoUninitialize();

	dbgend();

	return (int)iResult;
}

int APIENTRY _tWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR lpCmdLine,
	_In_ int nCmdShow
)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);
	return InitInstance(hInstance);
}
