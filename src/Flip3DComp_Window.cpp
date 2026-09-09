// ============================================================================
// Flip3DComp_Window.cpp — Window creation + enumeration
// ============================================================================
#include "Flip3DComp.h"

#include <algorithm>

struct Flip3DCompApp::EnumContext
{
    Flip3DCompApp*    app;
    std::vector<HWND> hwnds;
};

// ============================================================================
// Flip3DCompApp::EnumWindowsProc
// ============================================================================
BOOL CALLBACK Flip3DCompApp::EnumWindowsProc(HWND hwnd, LPARAM lParam)
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
// Flip3DCompApp::EnumerateWindows
// ============================================================================
std::vector<HWND> Flip3DCompApp::EnumerateWindows()
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
// Flip3DCompApp::ApplyFullscreenLayout
// uDWM EnableInputHooksHelper: WS_POPUP covering m_rcVirtualScreen.
// ============================================================================
void Flip3DCompApp::ApplyFullscreenLayout()
{
    if (!m_hwnd)
        return;

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);

    RECT client = {};
    if (GetClientRect(m_hwnd, &client))
    {
        m_width  = std::max(1u, (UINT)(client.right  - client.left));
        m_height = std::max(1u, (UINT)(client.bottom - client.top));
    }

    UpdateMonitorRect();
}

// ============================================================================
// Flip3DCompApp::CreateAppWindow
// uDWM Flip3D input window: borderless popup, topmost, full virtual desktop.
// ============================================================================
/*bool Flip3DCompApp::CreateAppWindow()
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
    RegisterClassExW(&wc);

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    m_hwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"Flip3DCompClass",
        L"",
        WS_POPUP,
        x, y, w, h,
        nullptr, nullptr,
        m_hInstance,
        this);

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

bool Flip3DCompApp::CreateAppWindow()
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
    RegisterClassExW(&wc);

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    m_hwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"Flip3DCompClass",
        L"",
        WS_POPUP,
        x, y, w, h,
        nullptr, nullptr,
        m_hInstance,
        this);

    if (!m_hwnd)
        return false;

    MONITORINFO mi = { sizeof(mi) };
    HMONITOR hPrimary = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfoW(hPrimary, &mi);

    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    UINT appBarState = static_cast<UINT>(SHAppBarMessage(ABM_GETSTATE, &abd));
    bool taskbarAutoHide = (appBarState & ABS_AUTOHIDE) != 0;

    int _x, _y, _w, _h;

    if (hTaskbar && !taskbarAutoHide)
    {
        ShowWindow(hTaskbar, SW_SHOW);

        _x = mi.rcWork.left;
        _y = mi.rcWork.top;
        _w = mi.rcWork.right - mi.rcWork.left;
        _h = mi.rcWork.bottom - mi.rcWork.top;
    }
    else
    {
        _x = mi.rcMonitor.left;
        _y = mi.rcMonitor.top;
        _w = mi.rcMonitor.right - mi.rcMonitor.left;
        _h = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }
    
    DwmFlush();

    m_rtl = (GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;

    RECT client = {};
    if (GetClientRect(m_hwnd, &client))
    {
        m_width  = std::max(1u, (UINT)(client.right  - client.left));
        m_height = std::max(1u, (UINT)(client.bottom - client.top));
    }

    return true;
}
