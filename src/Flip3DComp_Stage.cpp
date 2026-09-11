// ============================================================================
// Flip3DComp_Window.cpp — Window creation + enumeration
// ============================================================================
#include "Flip3DComp.h"
#include "banding.h"
#include "WindowBand.h"
#include "WindowCompositionAttribute.h"
//
#include <algorithm>
#include <vector>
#include <Windows.h>

struct Flip3DComp::EnumContext
{
    Flip3DComp*    app;
    std::vector<HWND> hwnds;
};

// ============================================================================
// Flip3DComp::EnumWindowsProc
// ============================================================================
BOOL CALLBACK Flip3DComp::EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto* ctx = (EnumContext*)lParam;

    if (!hwnd || !ctx->app || hwnd == ctx->app->WindowHandle())
        return TRUE;
    if (ctx->hwnds.size() >= (size_t)kMaxCards)
        return FALSE;
    if (!ctx->app->QualifiesForView(hwnd))
        return TRUE;

    ctx->hwnds.push_back(hwnd);
    return TRUE;
}

// ============================================================================
// Flip3DComp::EnumerateWindows
// ============================================================================
std::vector<HWND> Flip3DComp::EnumerateWindows()
{
    EnumContext ctx = { this };
    EnumWindows(EnumWindowsProc, (LPARAM)&ctx);

    HWND shell = GetShellWindow();
    if (shell && shell != m_hwnd && QualifiesForView(shell))
    {
        auto it = std::find(ctx.hwnds.begin(), ctx.hwnds.end(), shell);
        if (it == ctx.hwnds.end() && ctx.hwnds.size() < (size_t)kMaxCards)
            ctx.hwnds.push_back(shell);
    }

    return ctx.hwnds;
}

// ============================================================================
// Flip3DComp::ApplyFullscreenLayout
// uDWM EnableInputHooksHelper: WS_POPUP covering m_rcVirtualScreen.
// ============================================================================
void Flip3DComp::ApplyFullscreenLayout()
{
    if (!m_hwnd)
        return;

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    SetWindowPos(m_hwnd,
                 nullptr,
                 x,
                 y,
                 w,
                 h,
                 SWP_SHOWWINDOW);

    SetWindowPos(m_hwnd, HWND_BOTTOM, x, y, w, h, SWP_NOACTIVATE | SWP_FRAMECHANGED);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);

    RECT client = {};

    if (GetClientRect(m_hwnd, &client))
    {
        m_width = std::max(1u,
                          (UINT)(client.right - 
                           client.left));

        m_height = std::max(1u,
                           (UINT)(client.bottom - 
                                  client.top));
    }

    UpdateMonitorRect();
}

// ============================================================================
// Flip3DComp::CreateAppWindow
// uDWM Flip3D input window: borderless popup, topmost, full virtual desktop.
// ============================================================================
/*bool Flip3DComp::InitializeDCompStage()
{
    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_HREDRAW | CS_VREDRAW,
        &Flip3DComp::WndProc,
        0, 0,
        m_hInstance,
        nullptr,
        LoadCursorW(nullptr, IDC_ARROW),
        nullptr, nullptr,
        L"Flip3DCompClass",
        nullptr,
    };
    ATOM atom = RegisterClassExW(&wc);
    if (!atom)
    {
        return false;
    }

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    m_hwnd = WindowBand::CreateWindowInBand(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW,
        atom,
        L"Flip3DCompClass",
        WS_POPUP | WS_VISIBLE,
        x,
        y,
        w,
        h,
        m_hInstance,
        this,
        ZBID_DESKTOP
    );
    //

    BOOL exclude = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_EXCLUDED_FROM_PEEK, &exclude, sizeof(exclude));
    WindowCompositionAttribute::EnableAcrylic(m_hwnd);;
    //}
    //
    if (!m_hwnd)
        return false;

    m_rtl = (GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;

    RECT client = {};
    if (GetClientRect(m_hwnd, &client))
    {
        m_width  = std::max(1u, (UINT)(client.right  - client.left));
        m_height = std::max(1u, (UINT)(client.bottom - client.top));
    }

    return true;
}*/

bool Flip3DCompApp::InitializeDCompStage()
{
    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_HREDRAW | CS_VREDRAW,
        &Flip3DCompApp::WndProc,
        0, 0,
        m_hInstance,
        nullptr,
        LoadCursorW(nullptr, IDC_ARROW),
        nullptr, nullptr,
        L"Flip3DCompClass",
        nullptr,
    };
    ATOM atom = RegisterClassExW(&wc);
    if (!atom)
    {
        return false;
    }

    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    //
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (hTaskbar && IsWindowVisible(hTaskbar))
    {
        RECT rcTaskbar = {};
        if (GetWindowRect(hTaskbar, &rcTaskbar))
        {
            // bottom docked?
            if (rcTaskbar.top <= y + h && rcTaskbar.bottom >= y + h)
            {
                h -= (rcTaskbar.bottom - rcTaskbar.top);
            }
            // top docked?
            else if (rcTaskbar.top <= y && rcTaskbar.bottom >= y)
            {
                int tbHeight = rcTaskbar.bottom - rcTaskbar.top;
                y += tbHeight;
                h -= tbHeight;
            }
            // left docked?
            else if (rcTaskbar.left <= x && rcTaskbar.right >= x)
            {
                int tbWidth = rcTaskbar.right - rcTaskbar.left;
                x += tbWidth;
                w -= tbWidth;
            }
            // right docked?
            else if (rcTaskbar.left <= x + w && rcTaskbar.right >= x + w)
            {
                w -= (rcTaskbar.right - rcTaskbar.left);
            }
        }
    }

    m_hwnd = WindowBand::CreateWindowInBand(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW,
        atom,
        L"Flip3DCompClass",
        WS_POPUP | WS_VISIBLE,
        x,
        y,
        w,
        h,
        m_hInstance,
        this,
        ZBID_DESKTOP
    );

    if (!m_hwnd)
        return false;

    BOOL exclude = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_EXCLUDED_FROM_PEEK, &exclude, sizeof(exclude));
    WindowCompositionAttribute::EnableAcrylic(m_hwnd);

    m_rtl = (GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;

    RECT client = {};
    if (GetClientRect(m_hwnd, &client))
    {
        m_width  = std::max(1u, (UINT)(client.right  - client.left));
        m_height = std::max(1u, (UINT)(client.bottom - client.top));
    }

    return true;
}


