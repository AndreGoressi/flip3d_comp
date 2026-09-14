#include <Windows.h>
#include <dwmapi.h>

using DwmpCreateSharedMultiWindowVisual_t = HRESULT(WINAPI*)(
    HWND hwndDestination, 
    VOID* pDCompDevice, 
    VOID** ppVisual, 
    HTHUMBNAIL* phThumbnailId
);

using DwmpUpdateSharedMultiWindowVisual_t = HRESULT(WINAPI*)(
    HTHUMBNAIL hThumbnailId, 
    HWND* phwndsInclude, 
    DWORD chwndsInclude, 
    HWND* phwndsExclude, 
    DWORD chwndsExclude, 
    RECT* prcSource, 
    SIZE* pDestinationSize, 
    DWORD dwFlags
);

using DwmpQueryWindowThumbnailSourceSize_t = HRESULT(WINAPI*)(
    HWND hwndSource, 
    BOOL fSourceClientAreaOnly, 
    SIZE* pSize
);

namespace MultiWindowVisual
{
    bool Initialize();
    HRESULT Create(HWND hwndDestination, void* pDCompDevice, void** ppVisual, HTHUMBNAIL* phThumbnailId);
    HRESULT Update(HTHUMBNAIL hThumbnailId, HWND* includeArray, DWORD includeCount, HWND* excludeArray, DWORD excludeCount, RECT* prcSource, SIZE* pDestinationSize, DWORD dwFlags);
    HRESULT QuerySourceSize(HWND hwndSource, BOOL clientAreaOnly, SIZE* pSize);
}
