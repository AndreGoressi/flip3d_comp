#include "WindowBand.h"

static CreateWindowInBand_t g_CreateWindowInBand = nullptr;

bool WindowBand::Initialize()
{
    if (g_CreateWindowInBand)
        return true;

    auto user32 = GetModuleHandleW(L"user32.dll");

    if (!user32)
        return false;

    g_CreateWindowInBand =
        reinterpret_cast<CreateWindowInBand_t>(
            GetProcAddress(
                user32,
                "CreateWindowInBand"));

    return g_CreateWindowInBand != nullptr;
}

HWND WindowBand::CreateWindowInBand(DWORD exStyle,
                                    ATOM atom,
                                    LPCWSTR title,
                                    DWORD style,
                                    int x,
                                    int y,
                                    int width,
                                    int height,
                                    HINSTANCE instance,
                                    LPVOID param,
                                    DWORD band)
{
    if (!Initialize())
        return nullptr;

    return g_CreateWindowInBand(exStyle,
                                atom,
                                title,
                                style,
                                x,
                                y,
                                width,
                                height,
                                nullptr,
                                nullptr,
                                instance,
                                param,
                                band);
}
