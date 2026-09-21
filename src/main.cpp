// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include "Flip3DComp.h"
#include "PrepareForUIAccess.h"
//
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
    SetForegroundWindow(main.WindowHandle());
    UpdateWindow(main.WindowHandle());
    CoUninitialize();
    return main.Run();
}
