// ============================================================================
// main.cpp — Flip3D (DComp) entry point with UIAccess Token Magic
// ============================================================================
#include "Flip3DComp.h"
#include "PrepareForUIAccess.h"
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
        MessageBoxW(nullptr,
            main.InitErrorMessage(),
            L"Flip3D (DComp)", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
    int exitCode = main.Run();
    CoUninitialize();
    return exitCode;
}
