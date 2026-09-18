// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include "Flip3DComp.h"
#include "PrepareForUIAccess.h" 

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nShowCmd)
{
    //Force UIAccess to run immediately at startup (restarts the EXE invisibly if necessary)[cite: 3]
    DWORD dwErr = PrepareForUIAccess();
    if (ERROR_SUCCESS != dwErr)
    {
        //#if defined(_DEBUG)
        //#endif
    }

    Flip3DComp main;

    if (!main.Initialize(hInstance))
    {
        MessageBoxW(nullptr,
            main.InitErrorMessage(),
            L"Flip3D (DComp)", MB_OK | MB_ICONERROR);
        return 1;
    }
    ShowWindow(main.WindowHandle(), SW_SHOW);
    SetForegroundWindow(main.WindowHandle());
    UpdateWindow(main.WindowHandle());

    return main.Run();
}
