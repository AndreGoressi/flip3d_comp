// ============================================================================
// Flip3DComp_Window.cpp — Window creation + enumeration
// ============================================================================
#include "Flip3DComp.h"
#include <algorithm>
#include <vector>
#include <Windows.h>
#include <psapi.h>

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
    HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY);
    if (hMon)
        GetMonitorInfoW(hMon, &mi);

    const int x = mi.rcWork.left;
    const int y = mi.rcWork.top;
    const int w = mi.rcWork.right - mi.rcWork.left;
    const int h = mi.rcWork.bottom - mi.rcWork.top;
    //
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);

    RECT client = {};
    if (GetClientRect(m_hwnd, &client))
    {
        m_width  = std::max(1u, (UINT)(client.right  - client.left));
        m_height = std::max(1u, (UINT)(client.bottom - client.top));
    }
    UpdateMonitorRect();
}

BOOL Flip3DComp::SetWindowBand(HWND hWnd, HWND hwndInsertAfter, DWORD dwBand)
{
	if (g_iam_key)
	{
		m_NtUserEnableIAMAccess(g_iam_key, TRUE);
		const auto callResult = m_SetWindowBand(hWnd, hwndInsertAfter, dwBand);
		lSet = GetLastError();
		m_NtUserEnableIAMAccess(g_iam_key, FALSE);
		return callResult;
	}
	return FALSE;
}

bool Flip3DComp::SetTopmost(HWND hwnd, bool topmost)
{
    SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	
    const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    const bool actuallyTopmost = (exStyle & WS_EX_TOPMOST) != 0;
    return actuallyTopmost == topmost;
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
        LoadCursor(nullptr, IDC_ARROW),
        nullptr, nullptr,
        L"Flip3DCompClass",
        nullptr,
    };
    ATOM res = RegisterClassExW(&wc);
    if (!res) {
        DWORD dwError = GetLastError();
    }
    //
    MONITORINFO mi = { sizeof(mi) };
    HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY);
    if (hMon)
        GetMonitorInfoW(hMon, &mi);

    const int x = mi.rcWork.left;
    const int y = mi.rcWork.top;
    const int w = mi.rcWork.right - mi.rcWork.left;
    const int h = mi.rcWork.bottom - mi.rcWork.top;
	//
    m_hwnd = m_pfnCreateWindowInBand(WS_EX_NOREDIRECTIONBITMAP | 
                                     WS_EX_TOPMOST |
                                     WS_EX_TOOLWINDOW,
                                     (LPCWSTR)res, L"",                                          
                                     0x80000000,                                     
                                     x, y, w, h,                                   
                                     nullptr,                                      
                                     nullptr,                                      
                                     m_hInstance,                                  
                                     this,                                         
                                     ZBID_DESKTOP                                 
    );
    if (!m_hwnd)
        return false;
	//
	SetWindowBand(m_hwnd, nullptr, ZBID_SYSTEM_TOOLS);
	SetTopmost(m_hwnd, TRUE);
	//
	m_rtl = (GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
	//
    BOOL exclude = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_EXCLUDED_FROM_PEEK, &exclude, sizeof(exclude));
    //
    ACCENT_POLICY accent = {};
    accent.AccentState =  ACCENT_ENABLE_ACRYLICBLURBEHIND;
    accent.AccentFlags = 2;
    accent.GradientColor = 0x73190F0F; /*gradientColor*/
    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &accent;
    data.cbData = sizeof(accent);    
    m_pfnSetWindowCompositionAttribute(m_hwnd, &data);
    //
    RECT client = {};
    if (GetClientRect(m_hwnd, &client))
    {
        m_width  = std::max(1u, (UINT)(client.right  - client.left));
        m_height = std::max(1u, (UINT)(client.bottom - client.top));
    }
    return true;
}
