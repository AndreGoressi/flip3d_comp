#include "MultiWindowVisual.h"

static DwmpCreateSharedMultiWindowVisual_t g_pCreateSharedMultiWindowVisual = nullptr;
static DwmpUpdateSharedMultiWindowVisual_t g_pUpdateSharedMultiWindowVisual = nullptr;
static DwmpQueryWindowThumbnailSourceSize_t g_pQueryWindowThumbnailSourceSize = nullptr;
static bool g_isInitialized = false;

bool MultiWindowVisual::Initialize()
{
    if (g_isInitialized)
        return g_pCreateSharedMultiWindowVisual != nullptr;

    HMODULE dwmapiModule = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dwmapiModule)
    {
        // Ordinal 163: Shared Multi-Window Visual (ab Windows 10/11 Build 20xxx+)[cite: 6]
        g_pCreateSharedMultiWindowVisual = reinterpret_cast<DwmpCreateSharedMultiWindowVisual_t>(
            GetProcAddress(dwmapiModule, MAKEINTRESOURCEA(163))
        );
        
        // Ordinal 164: Update Shared Multi-Window Visual[cite: 6]
        g_pUpdateSharedMultiWindowVisual = reinterpret_cast<DwmpUpdateSharedMultiWindowVisual_t>(
            GetProcAddress(dwmapiModule, MAKEINTRESOURCEA(164))
        );

        // Ordinal 162: Query Window Thumbnail Source Size[cite: 6]
        g_pQueryWindowThumbnailSourceSize = reinterpret_cast<DwmpQueryWindowThumbnailSourceSize_t>(
            GetProcAddress(dwmapiModule, MAKEINTRESOURCEA(162))
        );
    }

    g_isInitialized = true;
    return g_pCreateSharedMultiWindowVisual != nullptr;
}

HRESULT MultiWindowVisual::Create(HWND hwndDestination, void* pDCompDevice, void** ppVisual, HTHUMBNAIL* phThumbnailId)
{
    if (!Initialize() || !g_pCreateSharedMultiWindowVisual)
        return E_FAIL;

    return g_pCreateSharedMultiWindowVisual(hwndDestination, pDCompDevice, ppVisual, phThumbnailId);
}

HRESULT MultiWindowVisual::Update(HTHUMBNAIL hThumbnailId, HWND* includeArray, DWORD includeCount, HWND* excludeArray, DWORD excludeCount, RECT* prcSource, SIZE* pDestinationSize, DWORD dwFlags)
{
    if (!Initialize() || !g_pUpdateSharedMultiWindowVisual)
        return E_FAIL;

    return g_pUpdateSharedMultiWindowVisual(hThumbnailId, includeArray, includeCount, excludeArray, excludeCount, prcSource, pDestinationSize, dwFlags);
}

HRESULT MultiWindowVisual::QuerySourceSize(HWND hwndSource, BOOL clientAreaOnly, SIZE* pSize)
{
    if (!Initialize() || !g_pQueryWindowThumbnailSourceSize)
        return E_FAIL;

    return g_pQueryWindowThumbnailSourceSize(hwndSource, clientAreaOnly, pSize);
}