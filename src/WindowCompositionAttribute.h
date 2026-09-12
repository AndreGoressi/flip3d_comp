#pragma once

#include <Windows.h>

enum ACCENT_STATE
{
    ACCENT_DISABLED                     = 0,
    ACCENT_ENABLE_GRADIENT              = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT   = 2,
    ACCENT_ENABLE_BLURBEHIND            = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND     = 4,
    ACCENT_ENABLE_HOSTBACKDROP          = 5,
};

struct ACCENT_POLICY
{
    ACCENT_STATE AccentState;
    DWORD        AccentFlags;
    DWORD        GradientColor;   // Format 0xAABBGGRR
    DWORD        AnimationId;
};

enum WINDOWCOMPOSITIONATTRIB
{
    WCA_ACCENT_POLICY = 19
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID                   pvData;
    SIZE_T                  cbData;
};

using SetWindowCompositionAttribute_t = BOOL (WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

namespace WindowCompositionAttribute
{
    bool Initialize();
    bool EnableBlurBehind(HWND hwnd);
}
