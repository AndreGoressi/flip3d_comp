// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include "PrepareForUIAccess.h"
#include "Flip3DComp.h"

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    DWORD dwErr = PrepareForUIAccess();
    if (ERROR_SUCCESS != dwErr)
    {
        //...
    }
    Flip3DComp main;

    if (!main.Initialize(hInstance))
    {
        return 1;
    }
    ShowWindow(main.WindowHandle(), SW_SHOW);
    SetForegroundWindow(main.WindowHandle());
    UpdateWindow(main.WindowHandle());

    return main.Run();
}
