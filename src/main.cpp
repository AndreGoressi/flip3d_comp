// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include "Flip3DComp.h"
bool SetTopmost(HWND hwnd, bool topmost)
{
    SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    const bool actuallyTopmost = (exStyle & WS_EX_TOPMOST) != 0;
    return actuallyTopmost == topmost;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    Flip3DComp main;
    if (!main.Initialize(hInstance))
    {
        CoUninitialize();
        return 1;
    }
    HWND hwnd = main.WindowHandle();
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetTopmost(hwnd, TRUE);
    SetForegroundWindow(hwnd);
    int result = main.Run();
    CoUninitialize();
    return result;
}
