// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include "Flip3DComp.h"
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    Flip3DComp main;
    if (!main.Initialize(hInstance))
    {
        CoUninitialize();
        return 1;
    }
    ShowWindow(app.WindowHandle(), SW_SHOW);
    SetForegroundWindow(app.WindowHandle());
    UpdateWindow(app.WindowHandle());
    //
    int result = main.Run();
    CoUninitialize();
    return result;
}
