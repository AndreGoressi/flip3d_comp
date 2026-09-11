// ============================================================================
// Flip3DComp_Input.cpp — Keyboard, mouse, and wheel input handling
// ============================================================================
#include "Flip3DComp.h"

// ============================================================================
// Flip3DComp::OnWheel
// Modern smooth scroll: each WHEEL_DELTA notch nudges the scroll target by one
// slot. Wheel down (delta < 0) scrolls front→back; wheel up scrolls back→front.
// ============================================================================
bool Flip3DComp::OnWheel(int wheelDelta)
{
    if (wheelDelta == 0 ||
        m_state == ViewState::Exit ||
        m_state == ViewState::ExitRepeatedRotate)
        return false;

    m_scrollTarget -= (float)wheelDelta / (float)WHEEL_DELTA;
    return true;
}

// ============================================================================
// Flip3DComp::OnKey
// ============================================================================
bool Flip3DComp::OnKey(bool down, UINT vkCode, LPARAM lParam)
{
    if (!down ||
        m_state == ViewState::Exit ||
        m_state == ViewState::ExitRepeatedRotate)
        return false;

    if (m_state == ViewState::Exit ||
        m_state == ViewState::ExitRepeatedRotate)
        return false;

    switch (vkCode)
    {
    case VK_ESCAPE:
        ExitView();
        return true;

    case VK_TAB:
    {
        const int direction = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? -1 : 1;
        RotateBy(direction);
        return true;
    }

    case VK_UP:
        RotateBy(-1);
        return true;

    case VK_DOWN:
        RotateBy(1);
        return true;

    case VK_LEFT:
        RotateBy(m_rtl ? 1 : -1);
        return true;

    case VK_RIGHT:
        RotateBy(m_rtl ? -1 : 1);
        return true;

    case VK_HOME:
        RotateToWindow(m_originalFrontHwnd);
        return true;

    case VK_F5:
        ReplayEnterAnimation();
        return true;

    case VK_RETURN:
    case VK_SPACE:
        SelectFront();
        return true;
    }

    return false;
}

// ============================================================================
// Flip3DComp::OnMouse
// ============================================================================
bool Flip3DComp::OnMouse(LONG x, LONG y, bool pressed)
{
    if (!pressed ||
        m_state == ViewState::Exit ||
        m_state == ViewState::ExitRepeatedRotate)
        return false;

    HWND hit = HitTest3DScene(x, y);
    if (hit)
        SelectWindow(hit);
    else
        ExitView();

    return true;
}
