#include "pDwmpQueryThumbnailType.h"

static DwmpQueryThumbnailType_t g_pDwmpQueryThumbnailType = nullptr;
static bool g_isInitialized = false;

bool ThumbQuery::Initialize()
{
    if (g_isInitialized)
        return g_pDwmpQueryThumbnailType != nullptr;

    HMODULE dwmapiModule = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dwmapiModule)
    {
        // Ordinal 114, queries thumbnail internal state (e.g. BitmapPending) in dwmapi.dll
        g_pDwmpQueryThumbnailType = reinterpret_cast<DwmpQueryThumbnailType_t>(
            GetProcAddress(dwmapiModule, reinterpret_cast<LPCSTR>(114))
        );
    }

    g_isInitialized = true;
    return g_pDwmpQueryThumbnailType != nullptr;
}

bool ThumbQuery::GetType(HTHUMBNAIL hThumbnailId, ThumbnailType* outType)
{
    if (!Initialize() || !g_pDwmpQueryThumbnailType || !hThumbnailId || !outType)
        return false;

    return SUCCEEDED(g_pDwmpQueryThumbnailType(hThumbnailId, outType));
}
