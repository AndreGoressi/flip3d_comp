// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include "Flip3DComp.h"
#include "PrepareForUIAccess.h"
//
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
        LONG_PTR exStyle = GetWindowLongPtr(main.WindowHandle(), GWL_EXSTYLE);
        bool isTopmost = (exStyle & WS_EX_TOPMOST) != 0;
        CoUninitialize();
        return isTopmost ? 0 : 1;
    }
    ShowWindow(main.WindowHandle(), SW_SHOW);
    UpdateWindow(main.WindowHandle());
    int exitCode = main.Run();
    CoUninitialize();
    return exitCode;
}

