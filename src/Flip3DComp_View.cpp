// ============================================================================
// Flip3DComp_View.cpp — View state: Exit, Select, HitTest3DScene
// ============================================================================
#include "Flip3DComp.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

// ============================================================================
// Flip3DComp::ExitView
// ============================================================================
void Flip3DComp::ExitView(bool commitScroll, float exitDurationSec)
{
    if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate)
        return;

    if (m_pfnActivateLivePreview)
    {
        m_pfnActivateLivePreview(FALSE, m_hwnd, nullptr, static_cast<UINT>(PeekTypes::Desktop), nullptr);
    }
    
    if (commitScroll)
        CommitCarouselScroll();

    m_lastPaintOrder.clear();
    m_state = ViewState::Exit;
    NotifyAccessibilityEvent(EVENT_SYSTEM_DIALOGEND);
    m_animEnter.Restart(EnterProgress(), 0.0f, exitDurationSec,
                        InterpolationMode::Linear);
}

// ============================================================================
// Flip3DComp::BeginExitView — uDWM CFlip3D::BeginExitView (parallel flatten)
// ============================================================================
void Flip3DComp::BeginExitView()
{
    if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate)
        return;

    m_lastPaintOrder.clear();
    NotifyAccessibilityEvent(EVENT_SYSTEM_DIALOGEND);
    m_animEnter.Restart(EnterProgress(), 0.0f, kExitDurationSec,
                        InterpolationMode::Linear);
    m_state = ViewState::Exit;
}

// ============================================================================
// Flip3DComp::SelectFront
// ============================================================================
void Flip3DComp::SelectFront()
{
    if (m_cards.empty())
        return;

    int   bestIdx  = 0;
    float bestSlot = 1e9f;
    for (int i = 0; i < (int)m_cards.size(); ++i)
    {
        const float slot = GetCardDisplaySlot(i);
        if (slot < -0.5f)
            continue;
        if (slot < bestSlot)
        {
            bestSlot = slot;
            bestIdx  = i;
        }
    }
    SelectWindow(m_cards[(size_t)bestIdx].m_hwnd);
}

// ============================================================================
// Flip3DComp::SelectWindow — uDWM: BeginExitView then ExitRepeatedRotate
// ============================================================================
void Flip3DComp::SelectWindow(HWND hwndTarget)
{
    if (!hwndTarget || !IsWindow(hwndTarget))
        return;

    //const bool isShell = (hwndTarget == GetShellWindow());

    /*if (isShell)
    {
        if (HWND shellTray = FindWindowW(L"Shell_TrayWnd", nullptr))
            PostMessageW(shellTray, 0x579, 1, 0);
    }*/
    /*else*/ if (!IsWindowEnabled(hwndTarget))
    {
        SwitchToThisWindow(GetLastActivePopup(GetAncestor(hwndTarget, GA_ROOTOWNER)), TRUE);
    }
    else
    {
        SwitchToThisWindow(hwndTarget, TRUE);
    }

    const int selIdx = FindCardIndex(hwndTarget);
    if (selIdx < 0)
    {
        ExitView();
        return;
    }

    auto& card = m_cards[(size_t)selIdx];
    if (card.m_isMinimized)
    {
        if (card.m_hThumb)
        {
            DwmUnregisterThumbnail(card.m_hThumb);
            card.m_hThumb = nullptr;
        }
        DwmInvalidateIconicBitmaps(hwndTarget);
        ShowWindowAsync(hwndTarget, SW_SHOWNOACTIVATE);
        //
        if (m_hwnd && hwndTarget)
        {
            DwmRegisterThumbnail(m_hwnd, hwndTarget, &card.m_hThumb);
        }
        UpdateCardGeometry(card, m_monW, m_monH, true);
    }

    m_selectedHwnd = hwndTarget;
    m_lastPaintOrder.clear();

    FreezeCarouselVisuals();
    BeginExitView();

    const int selIdxAfter = FindCardIndex(hwndTarget);
    if (selIdxAfter > 0)
    {
        m_rRepeatedRotateRate = -(kExitDurationSec / (float)selIdxAfter);
        m_state = ViewState::ExitRepeatedRotate;
        TickRepeatedRotate();
    }
}
