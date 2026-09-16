// ============================================================================
// Flip3DComp_ShellHook.cpp — dynamic card list via RegisterShellHookWindow
// ============================================================================
#include "Flip3DComp.h"
#include <algorithm>

// ============================================================================
// Flip3DComp::QualifiesForView
// ============================================================================
bool Flip3DComp::QualifiesForView(HWND hwnd) const
{
    if (!hwnd || hwnd == m_hwnd || hwnd == GetDesktopWindow())
        return false;

    if (!IsWindowVisible(hwnd))
        return false;

    if (m_selectedHwnd && hwnd == m_selectedHwnd)
        return true;

    const LONG_PTR style   = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    if ((style & WS_CHILD) != 0)
        return false;

    const bool isShellDesktop = (hwnd == GetShellWindow());

    if (!isShellDesktop
        && (exStyle & 0x08000080) != 0
        && (exStyle & WS_EX_APPWINDOW) == 0)
        return false;

    if (!isShellDesktop && (exStyle & WS_EX_TOOLWINDOW) != 0)
        return false;

    if (!isShellDesktop
        && (exStyle & WS_EX_APPWINDOW) == 0
        && GetWindow(hwnd, GW_OWNER) != nullptr)
        return false;

    if (IsNeverHiddenWindow(hwnd))
        return false;

    if (!isShellDesktop)
    {
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
            && cloaked != 0)
            return false;
    }
    return true;
}

// ============================================================================
bool Flip3DComp::IsFlip3DViewActive() const
{
    return m_state != ViewState::Inactive;
}

bool Flip3DComp::IsNeverHiddenWindow(HWND hwnd) const
{
    if (!hwnd)
        return true;

    if (hwnd == m_hwnd)
        return true;

    wchar_t cls[64] = {};
    if (!GetClassNameW(hwnd, cls, 63))
        return false;

    return !_wcsicmp(cls, L"Shell_TrayWnd")
    || !_wcsicmp(cls, L"Shell_SecondaryTrayWnd")
        || !_wcsicmp(cls, L"WorkerW");
}

// ============================================================================
void Flip3DComp::EnterFlip3DWindowMode()
{
    if (!m_hwnd || m_shellHookRegistered)
        return;

    if (!m_wmShellHook)
        m_wmShellHook = RegisterWindowMessageW(L"SHELLHOOK");

    if (RegisterShellHookWindow(m_hwnd))
        m_shellHookRegistered = true;
}

void Flip3DComp::LeaveFlip3DWindowMode()
{
    if (m_hwnd && m_shellHookRegistered)
    {
        DeregisterShellHookWindow(m_hwnd);
        m_shellHookRegistered = false;
    }
}

// ============================================================================
void Flip3DComp::OnShellHookMessage(WPARAM wParam, LPARAM lParam)
{
    if (!IsFlip3DViewActive())
        return;

    const UINT code = (UINT)(wParam & ~HSHELL_HIGHBIT);
    const HWND hwnd = (HWND)lParam;

    switch (code)
    {
    case HSHELL_WINDOWCREATED:
    case HSHELL_WINDOWACTIVATED:
    case HSHELL_WINDOWREPLACED:
    case HSHELL_RUDEAPPACTIVATED:
        if (hwnd)
            OnWindowShowHide(hwnd);
        break;

    case HSHELL_WINDOWDESTROYED:
        if (hwnd)
        {
            const int idx = FindCardIndex(hwnd);
            if (idx >= 0)
                RemoveCardAt((size_t)idx);
        }
        break;

    default:
        break;
    }
}

// ============================================================================
void Flip3DComp::OnWindowShowHide(HWND hwnd)
{
    if (!IsFlip3DViewActive() || !hwnd || !IsWindow(hwnd))
        return;

    if (hwnd == m_hwnd || IsNeverHiddenWindow(hwnd))
        return;

    const bool qualifies = QualifiesForView(hwnd);
    const int  cardIdx   = FindCardIndex(hwnd);

    if (qualifies)
    {
        if (cardIdx < 0)
            AddCardForWindow(hwnd);
    }
    else if (cardIdx >= 0)
    {
        RemoveCardAt((size_t)cardIdx);
    }
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
/*HRESULT Flip3DComp::CreateCardVisual(CardModel& card)
{
    if (!m_dcompDevice || !m_sceneVisual || !card.m_hwnd)
        return E_INVALIDARG;

    DWM_THUMBNAIL_PROPERTIES tp = {};
    tp.dwFlags   = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION
                 | DWM_TNP_ENABLE3D | DWM_TNP_FORCECVI;
    tp.fVisible  = TRUE;
    tp.rcDestination = { 0, 0, card.m_srcWidth, card.m_srcHeight };

    void* pv = nullptr;
    HRESULT hr = m_pfnCreateSharedThumbVisual(
        m_hwnd,
        card.m_hwnd,
        DWM_TNF_DWMWINDOW,
        &tp,
        m_dcompDevice.Get(),
        &pv,
        &card.m_hThumb);

    if (FAILED(hr) || !pv)
        return FAILED(hr) ? hr : E_FAIL;

    ComPtr<IDCompositionVisual> thumbBase;
    thumbBase.Attach((IDCompositionVisual*)pv);
    hr = thumbBase.As(&card.m_visual);
    if (FAILED(hr))
        return hr;

    ComPtr<IDCompositionVisual2> container;
    hr = m_dcompDevice->CreateVisual(&container);
    if (FAILED(hr))
        return hr;

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

    card.m_visual->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);
    card.m_visual->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);

    hr = container->AddVisual(card.m_visual.Get(), FALSE, nullptr);
    if (FAILED(hr))
        return hr;

    hr = container.As(&card.m_containerVisual);
    if (FAILED(hr))
        return hr;

    hr = m_sceneVisual->AddVisual(container.Get(), TRUE, nullptr);
    return hr;
}*/
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
    // 2. SPECIAL: If this is the Desktop card, also render any active snap groups on top of it!
    if (card.m_isShellDesktop)
    {
        MONITORINFO primaryMi = QueryPrimaryMonitor();
        std::vector<HWND> allHwnds = EnumerateWindows();
        std::vector<std::vector<HWND>> activeGroups = DetectActiveSnapGroups(allHwnds, primaryMi.rcWork);

        for (const auto& group : activeGroups)
        {
            for (HWND groupHwnd : group)
            {
                // Get true window bounds to calculate relative position on the desktop card
                RECT rcWin = {};
                if (FAILED(DwmGetWindowAttribute(groupHwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rcWin, sizeof(rcWin))))
                {
                    GetWindowRect(groupHwnd, &rcWin);
                }

                // Map screen coordinates relative to primary work area, then scale to card dimensions
                float scaleX = card.m_srcWidth / m_monW;
                float scaleY = card.m_srcHeight / m_monH;
                //
                int relX = (int)((rcWin.left - m_monOriginX) * scaleX);
                int relY = (int)((rcWin.top - m_monOriginY) * scaleY);
                int relW = (int)((rcWin.right - rcWin.left) * scaleX);
                int relH = (int)((rcWin.bottom - rcWin.top) * scaleY);
                //
                HTHUMBNAIL subThumb = nullptr;
                DWM_THUMBNAIL_PROPERTIES subTp = {};
                subTp.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_ENABLE3D | DWM_TNP_FORCECVI;
                subTp.fVisible = TRUE;
                subTp.rcDestination = { 0, 0, relW, relH };
                //
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
        //
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
