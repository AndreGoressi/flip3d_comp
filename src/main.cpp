// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include "Flip3DComp.h"
#include "PrepareForUIAccess.h"
//

bool SetAlwaysOnTopAsync(HWND hwnd, bool topmost)
{
    if (!hwnd) 
        return false;
    
    SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
                 
    const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    return ((exStyle & WS_EX_TOPMOST) != 0) == topmost;
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

    DWORD dwErr = PrepareForUIAccess();
    if (ERROR_SUCCESS != dwErr)
    {
        //...
    }
    ShowWindow(main.WindowHandle(), SW_SHOW);
    SetAlwaysOnTopAsync(main.WindowHandle(), true);
    UpdateWindow(main.WindowHandle());
    int exitCode = main.Run();
    CoUninitialize();
    return exitCode;
}
