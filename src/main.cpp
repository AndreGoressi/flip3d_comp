// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include <windows.h>
#include "Flip3DComp.h"
#include "PrepareForUIAccess.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    DWORD dwErr = PrepareForUIAccess();

    if (dwErr != ERROR_SUCCESS)
    {
        //...
    }

    Flip3DComp main;
    if (!main.Initialize(hInstance))
    {
        CoUninitialize();
        return 1;
    }

    ShowWindow(main.WindowHandle(), SW_SHOW);
    SetForegroundWindow(main.WindowHandle());
    UpdateWindow(main.WindowHandle());

    int result = main.Run();
    CoUninitialize();

    return result;
}
