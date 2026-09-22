// ============================================================================
// main.cpp — Flip3D (DComp) entry point
// ============================================================================
#include "Flip3DComp.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nShowCmd)
{
    Flip3DCompApp app;

    if (!app.Initialize(hInstance))
    {
        MessageBoxW(nullptr,
            app.InitErrorMessage(),
            L"Flip3D (DComp)", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(app.WindowHandle(), SW_SHOW);
    SetForegroundWindow(app.WindowHandle());
    UpdateWindow(app.WindowHandle());

    return app.Run();
}
