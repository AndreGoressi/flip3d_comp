#include "LivePreview.h"

static DwmpActivateLivePreview_t g_pDwmpActivateLivePreview = nullptr;
static BOOL g_aeroPeekActive = FALSE;
static bool g_isInitialized = false;

bool LivePreview::Initialize()
{
    if (g_isInitialized)
        return g_pDwmpActivateLivePreview != nullptr;

    HMODULE dwmapiModule = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dwmapiModule)
    {
        // Ordinal 113 is the native Aero Peek / Live Preview in dwmapi.dll
        g_pDwmpActivateLivePreview = reinterpret_cast<DwmpActivateLivePreview_t>(
            GetProcAddress(dwmapiModule, reinterpret_cast<LPCSTR>(113))
        );
    }
    g_isInitialized = true;
    return g_pDwmpActivateLivePreview != nullptr;
}

void LivePreview::Activate(BOOL enable, HWND hPeekWindow, HWND hTopmostWindow, PeekType peekType)
{
    if (!Initialize())
        return;

    if (g_aeroPeekActive != enable)
    {
        g_pDwmpActivateLivePreview(enable, hPeekWindow, hTopmostWindow, (UINT)peekType, nullptr);
        g_aeroPeekActive = enable;
    }
}
