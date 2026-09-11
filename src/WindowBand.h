#pragma once

#include "banding.h"

namespace WindowBand
{
    bool Initialize();

    HWND CreateWindowInBand(DWORD exStyle,
                            ATOM atom,
                            LPCWSTR title,
                            DWORD style,
                            int x,
                            int y,
                            int width,
                            int height,
                            HINSTANCE instance,
                            LPVOID param,
                            DWORD band);
}
