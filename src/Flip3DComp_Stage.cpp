// ============================================================================
// Flip3DComp_Window.cpp — Window creation + enumeration
// ============================================================================
#include "Flip3DComp.h"
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
    //
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

    MONITORINFO mi = { sizeof(mi) };
    HMONITOR hMon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    if (hMon)
        GetMonitorInfoW(hMon, &mi);

    const int x = mi.rcWork.left;
    const int y = mi.rcWork.top;
    const int w = mi.rcWork.right - mi.rcWork.left;
    int h = mi.rcWork.bottom - mi.rcWork.top;

    APPBARDATA abd = { sizeof(abd) };
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (taskbar) {
        abd.hWnd = taskbar;
        abd.cbSize = sizeof(abd);
        UINT state = SHAppBarMessage(ABM_GETSTATE, &abd);
        
        if (state & ABS_AUTOHIDE) {
            h -= 2; 
        }
    }

    SetWindowPos(m_hwnd, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW);
    //
    /*HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (taskbar)
    {
        SetWindowPos(taskbar, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        APPBARDATA abd = { sizeof(abd) };
        abd.hWnd = taskbar;
        SHAppBarMessage(ABM_ACTIVATE, &abd);
    }*/    
    
    RECT client = {};
    if (GetClientRect(m_hwnd, &client))
    {
        m_width  = std::max(1u, (UINT)(client.right  - client.left));
        m_height = std::max(1u, (UINT)(client.bottom - client.top));
    }

    UpdateMonitorRect();
}

// ============================================================================
// Flip3DComp::InitializeDCompStage
// uDWM Flip3D input window: borderless popup, topmost but do not cover the taskbar.
// ============================================================================
bool Flip3DComp::InitializeDCompStage()
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
    RegisterClassExW(&wc);
    //
    MONITORINFO mi = { sizeof(mi) };
    HMONITOR hMon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    if (hMon)
        GetMonitorInfoW(hMon, &mi);

    const int x = mi.rcWork.left;
    const int y = mi.rcWork.top;
    const int w = mi.rcWork.right - mi.rcWork.left;
    const int h = mi.rcWork.bottom - mi.rcWork.top;
    //
    m_hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | 
                             WS_EX_TOPMOST | 
                             WS_EX_TOOLWINDOW,
                             L"Flip3DCompClass",
                             L"",
                             WS_POPUP,
                             x, y, w, h,
                             nullptr, 
                             nullptr,
                             m_hInstance,
                             this);
    //
    //SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
    //

    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (taskbar) {
        // Position your window just below the taskbar in z-order
        SetWindowPos(m_hwnd, taskbar, x, y, w, h, SWP_NOACTIVATE);
    } else {
        SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
    }
    //
    
    /*HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (taskbar)
    {
        SetWindowPos(taskbar, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        APPBARDATA abd = { sizeof(abd) };
        abd.hWnd = taskbar;
        SHAppBarMessage(ABM_ACTIVATE, &abd);
    }*/
    
    if (m_hwnd)
    {
        BOOL exclude = TRUE;
        DwmSetWindowAttribute(m_hwnd, DWMWA_EXCLUDED_FROM_PEEK, &exclude, sizeof(exclude));
        WindowCompositionAttribute::EnableBlurBehind(m_hwnd);
    }
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
}




