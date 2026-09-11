#pragma once

#include <Windows.h>

enum class PeekType : long
{
    NotUsed = 0,
    Desktop = 1,
    Window  = 3
};

using DwmpActivateLivePreview_t = HRESULT(WINAPI*)(BOOL peekOn, 
                                                   HWND hPeekWindow, 
                                                   HWND hTopmostWindow, 
                                                   UINT peekType, 
                                                   LPVOID param5
);

namespace LivePreview
{
    bool Initialize();
    void Activate(BOOL enable, HWND hPeekWindow, HWND hTopmostWindow, PeekType peekType = PeekType::Desktop);
}