#include "WindowCompositionAttribute.h"

static SetWindowCompositionAttribute_t g_SetWindowCompositionAttribute = nullptr;

bool WindowCompositionAttribute::Initialize()
{
    if (g_SetWindowCompositionAttribute)
        return true;

    auto user32 = GetModuleHandleW(L"user32.dll");
    if (!user32)
        return false;

    g_SetWindowCompositionAttribute =
        reinterpret_cast<SetWindowCompositionAttribute_t>(
            GetProcAddress(
                user32,
                "SetWindowCompositionAttribute"));

    return g_SetWindowCompositionAttribute != nullptr;
}

bool WindowCompositionAttribute::EnableAcrylic(HWND hwnd, DWORD gradientColor)
{
    if (!Initialize())
        return false;

    ACCENT_POLICY accent = {};
    accent.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    accent.AccentFlags = 0;
    accent.GradientColor = 0xD9101010; /*0x73190F0F/*gradientColor;*/

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &accent;
    data.cbData = sizeof(accent);

    return g_SetWindowCompositionAttribute(hwnd, &data) != FALSE;
}
