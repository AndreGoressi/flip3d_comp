// ============================================================================
// Flip3DComp_Cards.cpp — Card building + thumbnail visual creation + DWM API
// ============================================================================
#include "Flip3DComp.h"
#include <cmath>
#include <unordered_set>

namespace {

MONITORINFO QueryPrimaryMonitor()
{
    MONITORINFO mi = { sizeof(mi) };
    HMONITOR hPrimary = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    if (hPrimary)
        GetMonitorInfoW(hPrimary, &mi);
    return mi;
}

// 2D screen anchor for non-minimized-tile layouts (extended frame or restore rect).
bool FillRestoredScreenRect(HWND h, const MONITORINFO& mi, RECT& out)
{
    bool maximized = IsZoomed(h);
    if (IsIconic(h))
    {
        WINDOWPLACEMENT wp = { sizeof(wp) };
        if (GetWindowPlacement(h, &wp) && !IsRectEmpty(&wp.rcNormalPosition))
        {
            out = wp.rcNormalPosition;
            OffsetRect(&out, mi.rcWork.left, mi.rcWork.top);
            if (wp.flags & WPF_RESTORETOMAXIMIZED)
            {
                maximized = true;
                out = mi.rcWork;
            }
            goto leave;
        }
    }
    DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &out, sizeof(out));
leave:
    if (maximized)
    {
        OffsetRect(&out, mi.rcWork.left - out.left, mi.rcWork.top - out.top);
    }
    return true;
}

} // namespace

// ============================================================================
// Flip3DComp::LoadThumbApi
// ============================================================================
bool Flip3DComp::LoadUndocApi()
{
    m_initError.clear();

    m_dwmapi = LoadLibraryW(L"dwmapi.dll");
    if (!m_dwmapi)
    {
        m_initError = L"Failed to load dwmapi.dll.";
        return false;
    }

    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (!hUser32)
    {
        m_initError = L"Failed to get user32.dll handle.";
        UnloadUndocApi();
        return false;
    }

    m_pfnCreateSharedThumbVisual = (DwmpCreateSharedThumbnailVisual_fn)
        GetProcAddress(m_dwmapi, MAKEINTRESOURCEA(147));
    
    m_pfnQueryThumbSize = (DwmpQueryWindowThumbnailSourceSize_fn)
        GetProcAddress(m_dwmapi, MAKEINTRESOURCEA(162));

    m_pfnActivateLivePreview = (DwmpActivateLivePreview_fn)
        GetProcAddress(m_dwmapi, MAKEINTRESOURCEA(113));

    m_pfnGetWindowMinimizeRect = (GetWindowMinimizeRect_fn)
        GetProcAddress(hUser32, "GetWindowMinimizeRect");

    m_pfnCreateWindowInBand = (CreateWindowInBand_fn)
        GetProcAddress(hUser32, "CreateWindowInBand"); 

    m_pfnSetWindowCompositionAttribute = (SetWindowCompositionAttribute_fn)
        GetProcAddress(hUser32, "SetWindowCompositionAttribute"); 

    if (!m_pfnCreateSharedThumbVisual)
    {
        m_initError = L"DwmpCreateSharedThumbnailVisual (dwmapi ord 147) is required.";
        UnloadUndocApi();
        return false;
    }
    if (!m_pfnQueryThumbSize)
    {
        m_initError = L"DwmpQueryWindowThumbnailSourceSize (dwmapi ord 162) is required.";
        UnloadUndocApi();
        return false;
    }
    if (!m_pfnGetWindowMinimizeRect)
    {
        m_initError = L"GetWindowMinimizeRect (user32) is required.";
        UnloadUndocApi();
        return false;
    }
    if (!m_pfnActivateLivePreview)
    {
        m_initError = L"DwmpActivateLivePreview (dwmapi ord 113) failed to load.";
        UnloadUndocApi();
        return false;
    }
    if (!m_pfnCreateWindowInBand)
    {
        m_initError = L"CreateWindowInBand failed to load.";
        UnloadUndocApi();
        return false;
    }
    if (!m_pfnSetWindowCompositionAttribute)
    {
        m_initError = L"SetWindowCompositionAttribute failed to load.";
        UnloadUndocApi();
        return false;
    }
    return true;
}
// ============================================================================
// Flip3DComp::UnloadDwmApi
// ============================================================================
void Flip3DComp::UnloadUndocApi()
{
    m_pfnCreateSharedThumbVisual        = nullptr;
    m_pfnQueryThumbSize                 = nullptr;
    m_pfnGetWindowMinimizeRect          = nullptr;
    m_pfnActivateLivePreview            = nullptr;
    m_pfnCreateWindowInBand             = nullptr;
    m_pfnSetWindowCompositionAttribute  = nullptr;

    if (m_dwmapi)
    {
        FreeLibrary(m_dwmapi);
        m_dwmapi = nullptr;
    }
}

// ============================================================================
// Flip3DComp::UpdateCardGeometry
// uDWM Flip3DWindow::OnOriginalRectUpdated:
//   - flatBounds in screen pixels (per-window monitor for taskbar/minimize)
//   - NormalizeWindowSize + world mapping via shared PRIMARY rcWork (normMon*)
//   - GetMonitorToWorldTransform on primary m_rcMonitor for all cards
// ============================================================================
void Flip3DComp::UpdateCardGeometry(CardModel& c, float normMonW, float normMonH,
                                       bool selectedRestore)
{
    HWND h = c.m_hwnd;
    if (!h)
        return;

    normMonW = std::max(normMonW, 1.0f);
    normMonH = std::max(normMonH, 1.0f);

    c.m_isMinimized    = IsIconic(h) && !selectedRestore;
    c.m_isShellDesktop = (h == GetShellWindow());

    HMONITOR mon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
    if (!mon)
        mon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO mi = { sizeof(mi) };
    if (mon)
        GetMonitorInfoW(mon, &mi);
    else
        mi = QueryPrimaryMonitor();

    SIZE srcSize = {};
    if (FAILED(m_pfnQueryThumbSize(h, FALSE, &srcSize))
        || srcSize.cx < 1 || srcSize.cy < 1)
        return;

    float thumbW = (float)srcSize.cx;
    float thumbH = (float)srcSize.cy;
    const float thumbAspect = thumbH / thumbW;

    RECT flatBounds = {};

    if (c.m_isShellDesktop)
    {
        // uDWM shell: relative origin {0,0} on primary - use primary rcWork.
        MONITORINFO primaryMi = QueryPrimaryMonitor();
        flatBounds = primaryMi.rcWork;
    }
    else if (c.m_isMinimized)
    {
        // 2D minimize destination: taskbar tile position only.
        RECT minRect = {};
        if (m_pfnGetWindowMinimizeRect(h, &minRect) && !IsRectEmpty(&minRect))
            flatBounds = Math::BuildFinalMinRect(minRect, thumbAspect);
    }
    //
    else if (!FillRestoredScreenRect(h, mi, flatBounds))
    {
        flatBounds = mi.rcWork;
    }

    if (IsRectEmpty(&flatBounds))
        flatBounds = mi.rcWork;

    c.m_srcWidth  = (int)thumbW;
    c.m_srcHeight = (int)thumbH;
    //
    float maxResW = normMonW * 0.5f;
    float maxResH = normMonH * 0.5f;
    float scale = std::min(maxResW / thumbW, maxResH / thumbH);
    scale = std::min(scale, 1.0f); 

    c.m_srcWidth  = std::max(1, (int)(thumbW * scale));
    c.m_srcHeight = std::max(1, (int)(thumbH * scale));
    // -------------------------------------------------------------------------------

    // targetSize / occupancy = 3D carousel (uDWM finalSize).
    Math::WorldSizesFromThumbPixels(
        thumbW, thumbH, normMonW, normMonH,
        c.m_flatSize, c.m_targetSize, c.m_occupancy);

    c.m_aspectRatio = thumbW / thumbH;

    const float qualityScale = CardThumbnailQualityScale();
    c.m_srcWidth = std::max(1, (int)std::lround(thumbW * qualityScale));
    c.m_srcHeight = std::max(1, (int)std::lround(thumbH * qualityScale));

    // 2D flat: position from flatBounds; size from QueryThumbSize (restored pixels).
    // Exceptions: shell uses rcWork; iconic minimize uses taskbar tile dimensions.
    float flatW = thumbW;
    float flatH = thumbH;
    
    if (c.m_isShellDesktop || c.m_isMinimized)
    {
        flatW = (float)std::max(1L, flatBounds.right  - flatBounds.left);
        flatH = (float)std::max(1L, flatBounds.bottom - flatBounds.top);
    }

    c.m_flatSize = { flatW / normMonW, flatH / normMonH };

    float anchorX = (float)flatBounds.left;
    float anchorY = (float)flatBounds.top;
    if (m_rtl)
    {
        const float relX = anchorX - m_monOriginX;
        anchorX = m_monOriginX + (normMonW - (flatW + relX));
    }

    float worldX = 0.0f;
    float worldY = 0.0f;
    Math::MonitorToWorldTopLeft(
        anchorX, anchorY,
        m_monOriginX, m_monOriginY, normMonW, normMonH,
        worldX, worldY);

    c.m_originalPos = { worldX, worldY, 0.0f };
    c.m_flatPos     = { worldX, worldY, 0.0f };
}

// ============================================================================
// Flip3DComp::UpdateMonitorRect
// uDWM UpdateMonitorRect: normMon from primary rcWork; SetSize uses work-area
// pixels. The input window covers the virtual desktop, but the 3D viewport must
// match rcWork (size + origin), not the full client rect.
// ============================================================================
void Flip3DComp::UpdateMonitorRect()
{
    HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    if (!hMon)
        return;

    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoW(hMon, &mi))
        return;

    const int vx = GetSystemMetrics(SM_CXSCREEN);
    const int vy = GetSystemMetrics(SM_CYSCREEN);
    m_viewX = (float)(mi.rcWork.left - vx);
    m_viewY = (float)(mi.rcWork.top  - vy);

    const float newMonW      = (float)std::max(1L, mi.rcWork.right  - mi.rcWork.left);
    const float newMonH      = (float)std::max(1L, mi.rcWork.bottom - mi.rcWork.top);
    const float newOriginX   = (float)mi.rcWork.left;
    const float newOriginY   = (float)mi.rcWork.top;

    const bool layoutChanged =
        newMonW    != m_monW        ||
        newMonH    != m_monH        ||
        newOriginX != m_monOriginX ||
        newOriginY != m_monOriginY;

    m_monW       = newMonW;
    m_monH       = newMonH;
    m_monOriginX = newOriginX;
    m_monOriginY = newOriginY;
    
    if (layoutChanged)
    {
        for (auto& card : m_cards)
            UpdateCardGeometry(card, m_monW, m_monH);
    }
}

bool Flip3DComp::AddCardForWindow(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return false;
    // Check if a card for this window already exists
    if (FindCardIndex(hwnd) >= 0)
        return true;

    CardModel c;
    c.m_hwnd = hwnd;
    c.m_isGroup = false;
    c.m_initialCarouselIndex = (int)m_cards.size();
    
    UpdateCardGeometry(c, m_monW, m_monH);

    if (SUCCEEDED(CreateCardVisual(c)))
    {
        m_cards.push_back(std::move(c));
        return true;
    }

    return false;
}

void Flip3DComp::BuildCards()
{
    m_cards.clear();

    if (!m_d3dDevice)
    {
        D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            &fl, 1, D3D11_SDK_VERSION,
            &m_d3dDevice, nullptr, nullptr);

        if (FAILED(hr))
        {
            D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                &fl, 1, D3D11_SDK_VERSION,
                &m_d3dDevice, nullptr, nullptr);
        }
    }
    auto hwnds = EnumerateWindows();
    MONITORINFO primaryMi = QueryPrimaryMonitor();
    m_monW       = (float)std::max(1L, primaryMi.rcWork.right  - primaryMi.rcWork.left);
    m_monH       = (float)std::max(1L, primaryMi.rcWork.bottom - primaryMi.rcWork.top);
    m_monOriginX = (float)primaryMi.rcWork.left;
    m_monOriginY = (float)primaryMi.rcWork.top;

    int carouselIndex = 0;
    for (auto h : hwnds)
    {
        CardModel c;
        c.m_hwnd                 = h;
        c.m_isGroup             =  false;
        c.m_initialCarouselIndex = carouselIndex++;
        UpdateCardGeometry(c, m_monW, m_monH);
        m_cards.push_back(std::move(c));
    }
    m_originalFrontHwnd = m_cards.empty() ? nullptr : m_cards[0].m_hwnd;
}

// ============================================================================
int Flip3DComp::FindCardIndex(HWND hwnd) const
{
    if (!hwnd)
        return -1;

    for (int i = 0; i < (int)m_cards.size(); ++i)
    {
        if (m_cards[(size_t)i].m_hwnd == hwnd)
            return i;
    }
    return -1;
}

// ============================================================================
// Flip3DComp::CreateCardVisuals
// ============================================================================
HRESULT Flip3DComp::CreateCardVisual(CardModel& card)
{
    if (!m_dcompDevice || !m_sceneVisual)
        return E_INVALIDARG;

    if (!card.m_isGroup && !card.m_hwnd && !card.m_isShellDesktop)
        return E_INVALIDARG;

    if (card.m_isGroup && card.m_groupHwnds.empty())
        return E_INVALIDARG;

    // Create container visual for the card
    ComPtr<IDCompositionVisual2> container;
    HRESULT hr = m_dcompDevice->CreateVisual(&container);
    if (FAILED(hr))
        return hr;

    // 1. Render the primary source (either the single window or the desktop background)
    HWND primarySourceHwnd = card.m_isGroup ? nullptr : card.m_hwnd;
    if (primarySourceHwnd)
    {
        DWM_THUMBNAIL_PROPERTIES tp = {};
        tp.dwFlags     = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_ENABLE3D | DWM_TNP_FORCECVI;
        tp.fVisible    = TRUE;
        tp.rcDestination = { 0, 0, card.m_srcWidth, card.m_srcHeight };

        void* pv = nullptr;
        hr = m_pfnCreateSharedThumbVisual(
            m_hwnd, primarySourceHwnd, DWM_TNF_DWMWINDOW, &tp,
            m_dcompDevice.Get(), &pv, &card.m_hThumb);

        if (SUCCEEDED(hr) && pv)
        {
            ComPtr<IDCompositionVisual> thumbBase;
            thumbBase.Attach((IDCompositionVisual*)pv);
            if (SUCCEEDED(thumbBase.As(&card.m_visual)))
            {
                card.m_visual->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);
                card.m_visual->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
                container->AddVisual(card.m_visual.Get(), FALSE, nullptr);
            }
        }
    }
    hr = container.As(&card.m_containerVisual);
    if (FAILED(hr))
        return hr;
    //
    if (card.m_isShellDesktop)
    {
        MONITORINFO primaryMi = QueryPrimaryMonitor();
        std::vector<HWND> allHwnds = EnumerateWindows();
        std::vector<std::vector<HWND>> activeGroups = DetectActiveSnapGroups(allHwnds, primaryMi.rcWork);
        
        for (auto group : activeGroups)
        {
            if (group.empty())
                continue;
            
            for (HWND groupHwnd : group)
            {
                RECT rcWin = {};
                bool isVis = IsWindowVisible(groupHwnd);
                if (isVis)
                {
                    WINDOWPLACEMENT wp = { sizeof(wp) };
                    if (GetWindowPlacement(groupHwnd, &wp))
                        rcWin = wp.rcNormalPosition;
                }
                else if (FAILED(DwmGetWindowAttribute(groupHwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rcWin, sizeof(rcWin))))
                {
                    GetWindowRect(groupHwnd, &rcWin);
                }

                if (rcWin.right <= rcWin.left || rcWin.bottom <= rcWin.top)
                    continue;

                float scaleX = card.m_srcWidth / m_monW;
                float scaleY = card.m_srcHeight / m_monH;

                float screenX = (float)(rcWin.left - m_monOriginX);
                float screenY = (float)(rcWin.top - m_monOriginY);
                float screenW = (float)(rcWin.right - rcWin.left);
                float screenH = (float)(rcWin.bottom - rcWin.top);

                float gutter = 160.0f;
                bool touchesLeft   = (screenX <= 5.0f);
                bool touchesRight  = (abs((screenX + screenW) - m_monW) <= 5.0f);
                bool touchesTop    = (screenY <= 5.0f);
                bool touchesBottom = (abs((screenY + screenH) - m_monH) <= 5.0f);

                float adjustedX = screenX + (touchesLeft ? gutter : gutter * 0.5f);
                float adjustedY = screenY + (touchesTop ? gutter : gutter * 0.5f);
                float adjustedW = screenW - ((touchesLeft ? gutter : gutter * 0.5f) + (touchesRight ? gutter : gutter * 0.5f));
                float adjustedH = screenH - ((touchesTop ? gutter : gutter * 0.5f) + (touchesBottom ? gutter : gutter * 0.5f));

                int relX = (int)(adjustedX * scaleX);
                int relY = (int)(adjustedY * scaleY);
                int relW = (int)(adjustedW * scaleX);
                int relH = (int)(adjustedH * scaleY);
                //
                HTHUMBNAIL subThumb = nullptr;
                DWM_THUMBNAIL_PROPERTIES subTp = {};
                subTp.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_ENABLE3D;
                
                if (isMin)
                    subTp.dwFlags |= DWM_TNP_FORCECVI;
                
                subTp.fVisible = TRUE;
                subTp.rcDestination = { 0, 0, relW, relH };

                void* subPv = nullptr;
                if (SUCCEEDED(m_pfnCreateSharedThumbVisual(m_hwnd, groupHwnd, DWM_TNF_DWMWINDOW, &subTp, m_dcompDevice.Get(), &subPv, &subThumb)))
                {
                    ComPtr<IDCompositionVisual> subThumbBase;
                    subThumbBase.Attach((IDCompositionVisual*)subPv);
                    
                    ComPtr<IDCompositionVisual3> subVisual;
                    if (SUCCEEDED(subThumbBase.As(&subVisual)))
                    {
                        subVisual->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);
                        subVisual->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
                        subVisual->SetOffsetX((float)relX);
                        subVisual->SetOffsetY((float)relY);
                        container->AddVisual(subVisual.Get(), FALSE, nullptr);
                    }
                }
            }
        }
    }
    // Apply global rounded corner clipping to the container visual
    ComPtr<IDCompositionRectangleClip> clip;
    if (SUCCEEDED(m_dcompDevice->CreateRectangleClip(&clip)))
    {
        float radius = 12.f / 2.f;
        clip->SetLeft(0.f);
        clip->SetTop(0.f);
        clip->SetRight((float)card.m_srcWidth);
        clip->SetBottom((float)card.m_srcHeight);
        clip->SetTopLeftRadiusX(radius);
        clip->SetTopLeftRadiusY(radius);
        clip->SetTopRightRadiusX(radius);
        clip->SetTopRightRadiusY(radius);
        clip->SetBottomLeftRadiusX(radius);
        clip->SetBottomLeftRadiusY(radius);
        clip->SetBottomRightRadiusX(radius);
        clip->SetBottomRightRadiusY(radius);
        container->SetClip(clip.Get());
    }
    container->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);
    container->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);

    hr = container.As(&card.m_containerVisual);
    if (FAILED(hr))
        return hr;

    hr = m_sceneVisual->AddVisual(container.Get(), TRUE, nullptr);
    return hr;
}

// ============================================================================
HRESULT Flip3DComp::CreateCardVisuals()
{
    if (!m_dcompDevice || !m_sceneVisual)
        return E_FAIL;

    for (auto& card : m_cards)
    {
        // Skip if already initialized, but allow cards that either have a single hwnd OR are a group with hwnds
        if (card.m_containerVisual)
            continue;

        if (!card.m_isGroup && !card.m_hwnd)
            continue;

        if (card.m_isGroup && card.m_groupHwnds.empty())
            continue;

        if (FAILED(CreateCardVisual(card)))
            continue;
    }
    return S_OK;
}

void Flip3DComp::RemoveCardAt(size_t index)
{
    if (index >= m_cards.size())
        return;

    CardModel& card = m_cards[index];

    if (card.m_containerVisual && m_sceneVisual)
    {
        ComPtr<IDCompositionVisual> sceneBase;
        if (SUCCEEDED(m_sceneVisual.As(&sceneBase)))
            sceneBase->RemoveVisual(card.m_containerVisual.Get());
    }

    if (card.m_hThumb)
    {
        DwmUnregisterThumbnail(card.m_hThumb);
        card.m_hThumb = nullptr;
    }

    card.m_visual.Reset();
    card.m_containerVisual.Reset();

    m_cards.erase(m_cards.begin() + (ptrdiff_t)index);

    if (m_dcompDevice)
        m_dcompDevice->Commit();
}

// ============================================================================
// Flip3DComp::UpdateCardThumbnailDest
// Sync DWM thumbnail rcDestination to the current source pixel size.
// ============================================================================
void Flip3DComp::UpdateCardThumbnailDest(CardModel& card)
{
    if (!card.m_hThumb || card.m_srcWidth <= 0 || card.m_srcHeight <= 0)
        return;

    DWM_THUMBNAIL_PROPERTIES tp = {};
    tp.dwFlags   = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION
                 | DWM_TNP_ENABLE3D | DWM_TNP_FORCECVI;
    tp.fVisible  = TRUE;
    tp.rcDestination = { 0, 0, card.m_srcWidth, card.m_srcHeight };
    DwmUpdateThumbnailProperties(card.m_hThumb, &tp);
}

// ============================================================================
// Flip3DComp::OnThumbnailSourceSizeChanged
// uDWM CThumbnailVisual::OnSizeChanged posts WM 0x327 to hwndDestination when
// DWM_TNF_DWMWINDOW is set and the live preview bitmap changes size. Every
// card thumbnail posts independently, so the WndProc only sets m_thumbnailsDirty
// and this runs once per frame, touching cards whose queried source size differs.
// ============================================================================
void Flip3DComp::OnThumbnailSourceSizeChanged()
{
    m_thumbnailsDirty = false;

    bool anyChange = false;
    for (auto& card : m_cards)
    {
        if (!card.m_hwnd)
            continue;

        SIZE querySize = {};
        if (FAILED(m_pfnQueryThumbSize(card.m_hwnd, FALSE, &querySize)))
            continue;

        const int queryW = (int)std::max(0L, querySize.cx);
        const int queryH = (int)std::max(0L, querySize.cy);
        if (queryW == card.m_srcWidth && queryH == card.m_srcHeight)
            continue;

        const bool selectedRestore = card.m_hwnd == m_selectedHwnd;
        UpdateCardGeometry(card, m_monW, m_monH, selectedRestore);
        UpdateCardThumbnailDest(card);
        anyChange = true;
    }
    if (anyChange && m_dcompDevice)
        m_dcompDevice->Commit();
}
 
std::vector<std::vector<HWND>> Flip3DComp::DetectActiveSnapGroups(const std::vector<HWND>& hwnds, const RECT& rcWork)
{
    std::vector<std::vector<HWND>> groups;
    auto getSafeRect = [](HWND hwnd) {
        RECT rc = {};
        if (IsIconic(hwnd))
        {
            WINDOWPLACEMENT wp = { sizeof(wp) };
            if (GetWindowPlacement(hwnd, &wp))
            {
                rc = wp.rcNormalPosition;
            }
        }
        else
        {
            if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(rc))))
            {
                GetWindowRect(hwnd, &rc);
            }
        }
        return rc;
    };

    for (size_t i = 0; i < hwnds.size(); ++i)
    {
        if (!IsWindow(hwnds[i]) || hwnds[i] == GetShellWindow() || !IsWindowVisible(hwnds[i]) && !IsIconic(hwnds[i]))
            continue;

        RECT rc1 = getSafeRect(hwnds[i]);
        if (rc1.right <= rc1.left || rc1.bottom <= rc1.top)
            continue;

        for (size_t j = i + 1; j < hwnds.size(); ++j)
        {
            if (!IsWindow(hwnds[j]) || hwnds[j] == GetShellWindow() || !IsWindowVisible(hwnds[j]) && !IsIconic(hwnds[j]))
                continue;

            RECT rc2 = getSafeRect(hwnds[j]);
            if (rc2.right <= rc2.left || rc2.bottom <= rc2.top)
                continue;

            // Check horizontal adjacency (side-by-side snap)
            bool touchingHorizontally = (abs(rc1.right - rc2.left) <= 8 || abs(rc2.right - rc1.left) <= 8);
            bool verticalOverlap = (rc1.top < rc2.bottom && rc1.bottom > rc2.top);

            if (touchingHorizontally && verticalOverlap)
            {
                groups.push_back({ hwnds[i], hwnds[j] });
            }
            // Check vertical adjacency (stacked snap)
            else
            {
                bool touchingVertically = (abs(rc1.bottom - rc2.top) <= 8 || abs(rc2.bottom - rc1.top) <= 8);
                bool horizontalOverlap = (rc1.left < rc2.right && rc1.right > rc2.left);

                if (touchingVertically && horizontalOverlap)
                {
                    groups.push_back({ hwnds[i], hwnds[j] });
                }
            }
        }
    }
    return groups;
}

