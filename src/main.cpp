// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include "Flip3DComp.h"
#include "PrepareForUIAccess.h"
//
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
    DWORD dwErr = PrepareForUIAccess();
    if (ERROR_SUCCESS != dwErr)
    {
        //...
    }
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    Flip3DComp main;
    if (!main.Initialize(hInstance))
    {
        CoUninitialize();
        return 1;
    }    
    ShowWindow(main.WindowHandle(), SW_SHOW);
    UpdateWindow(main.WindowHandle());
    SetTopmost(main.WindowHandle(), TRUE);
    SetForegroundWindow(main.WindowHandle());
    CoUninitialize();
    return main.Run();
}
