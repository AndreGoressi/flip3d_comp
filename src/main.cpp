// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include <windows.h>
#include "PrepareForUIAccess.h"
#include "Flip3DComp.h"

int main()
{
    DWORD dwErr = PrepareForUIAccess();
    if (ERROR_SUCCESS != dwErr)
    {
        //...
    }

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    Flip3DComp main;

    if (!main.Initialize(hInstance))
    {
        MessageBoxW(nullptr, main.InitErrorMessage(), L"Flip3D (DComp)", MB_OK | MB_ICONERROR);
        return 1;
    }
    ShowWindow(main.WindowHandle(), SW_SHOW);
    SetForegroundWindow(main.WindowHandle());
    UpdateWindow(main.WindowHandle());

    return main.Run();
}
