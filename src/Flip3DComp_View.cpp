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

    const bool isShell = (hwndTarget == GetShellWindow());

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


// ============================================================================
// Flip3DComp::HitTest3DScene
// ============================================================================
HWND Flip3DComp::HitTest3DScene(LONG screenX, LONG screenY) const
{
    if (m_cards.empty())
        return nullptr;

    const float p    = EnterProgress();
    const auto  cam  = BuildCameraMatrix(p);

    float bestNdcZ = 1e10f;
    HWND  bestHwnd = nullptr;

    struct Vertex4 { float x, y, z, w; };

    for (int ki = (int)m_cards.size() - 1; ki >= 0; --ki)
    {
        const CardModel& c = m_cards[(size_t)ki];
        if (!c.m_containerVisual)
            continue;

        const float slot = GetCardDisplaySlot(ki);
        if (slot <= -0.5f || slot >= (float)kMaxVisibleCards)
            continue;

        float t   = ComputeCarouselBezierT(slot);
        const float flatRank = ComputeFlatDepthRank(slot, p, ki);
        auto  MVP = Math::Multiply(BuildModelMatrix(c, t, p, flatRank), cam);

        float sw = (float)std::max(c.m_srcWidth,  1);
        float sh = (float)std::max(c.m_srcHeight, 1);

        auto transformVertex = [&](float px, float py) -> Vertex4 {
            float x = px * MVP.m[0][0] + py * MVP.m[1][0] + MVP.m[3][0];
            float y = px * MVP.m[0][1] + py * MVP.m[1][1] + MVP.m[3][1];
            float z = px * MVP.m[0][2] + py * MVP.m[1][2] + MVP.m[3][2];
            float w = px * MVP.m[0][3] + py * MVP.m[1][3] + MVP.m[3][3];
            return { x, y, z, w };
        };

        Vertex4 v0 = transformVertex(0.0f, 0.0f);
        Vertex4 v1 = transformVertex(sw,   0.0f);
        Vertex4 v2 = transformVertex(sw,   sh);
        Vertex4 v3 = transformVertex(0.0f, sh);

        Vec2 c0 = { v0.w != 0.0f ? v0.x / v0.w : v0.x, v0.w != 0.0f ? v0.y / v0.w : v0.y };
        Vec2 c1 = { v1.w != 0.0f ? v1.x / v1.w : v1.x, v1.w != 0.0f ? v1.y / v1.w : v1.y };
        Vec2 c2 = { v2.w != 0.0f ? v2.x / v2.w : v2.x, v2.w != 0.0f ? v2.y / v2.w : v2.y };
        Vec2 c3 = { v3.w != 0.0f ? v3.x / v3.w : v3.x, v3.w != 0.0f ? v3.y / v3.w : v3.y };

        float sx = (float)screenX;
        float sy = (float)screenY;

        auto cross = [](float x1, float y1, float x2, float y2) {
            return x1 * y2 - y1 * x2;
        };

        float d0 = cross(c1.x - c0.x, c1.y - c0.y, sx - c0.x, sy - c0.y);
        float d1 = cross(c2.x - c1.x, c2.y - c1.y, sx - c1.x, sy - c1.y);
        float d2 = cross(c3.x - c2.x, c3.y - c2.y, sx - c2.x, sy - c2.y);
        float d3 = cross(c0.x - c3.x, c0.y - c3.y, sx - c3.x, sy - c3.y);

        bool inside = (d0 >= 0 && d1 >= 0 && d2 >= 0 && d3 >= 0)
                   || (d0 <= 0 && d1 <= 0 && d2 <= 0 && d3 <= 0);

        if (!inside)
            continue;

        float pixZ = v0.z;
        float pixW = v0.w;

        auto getBarycentric = [](Vec2 p, Vec2 a, Vec2 b, Vec2 c, float& u, float& v, float& w) {
            Vec2 v0_ = { b.x - a.x, b.y - a.y };
            Vec2 v1_ = { c.x - a.x, c.y - a.y };
            Vec2 v2_ = { p.x - a.x, p.y - a.y };
            float d00 = v0_.x * v0_.x + v0_.y * v0_.y;
            float d01 = v0_.x * v1_.x + v0_.y * v1_.y;
            float d11 = v1_.x * v1_.x + v1_.y * v1_.y;
            float d20 = v2_.x * v0_.x + v2_.y * v0_.y;
            float d21 = v2_.x * v1_.x + v2_.y * v1_.y;
            float denom = d00 * d11 - d01 * d01;
            if (fabsf(denom) < 1e-6f) return false;
            v = (d11 * d20 - d01 * d21) / denom;
            w = (d00 * d21 - d01 * d20) / denom;
            u = 1.0f - v - w;
            return true;
        };

        Vec2 pt = { sx, sy };
        float u, v, w;
        if (getBarycentric(pt, c0, c1, c2, u, v, w) && u >= -1e-4f && v >= -1e-4f && w >= -1e-4f)
        {
            pixZ = u * v0.z + v * v1.z + w * v2.z;
            pixW = u * v0.w + v * v1.w + w * v2.w;
        }
        else if (getBarycentric(pt, c0, c2, c3, u, v, w) && u >= -1e-4f && v >= -1e-4f && w >= -1e-4f)
        {
            pixZ = u * v0.z + v * v2.z + w * v3.z;
            pixW = u * v0.w + v * v2.w + w * v3.w;
        }
        else
        {
            pixZ = 0.25f * (v0.z + v1.z + v2.z + v3.z);
            pixW = 0.25f * (v0.w + v1.w + v2.w + v3.w);
        }

        float ndcZ = pixW != 0.0f ? pixZ / pixW : 0.0f;

        if (ndcZ < bestNdcZ)
        {
            bestNdcZ = ndcZ;
            bestHwnd = c.m_hwnd;
        }
    }
    return bestHwnd;
}
