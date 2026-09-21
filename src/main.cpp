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
        //...
    }
    ShowWindow(main.WindowHandle(), SW_SHOW);
    UpdateWindow(main.WindowHandle());
    int exitCode = main.Run();
    CoUninitialize();
    return exitCode;
}
