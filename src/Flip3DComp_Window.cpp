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

enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_INVALID_STATE = 5
};

struct ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    unsigned int GradientColor;
    int AnimationId;
};

enum WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    void* pvData;
    size_t cbData;
};

typedef BOOL (WINAPI* SetWindowCompositionAttribute_t)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
bool DrawAcrylic(HWND hwnd)
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return false;

    auto SetWCA = reinterpret_cast<SetWindowCompositionAttribute_t>(
        GetProcAddress(user32, "SetWindowCompositionAttribute"));
    if (!SetWCA) return false;

    ACCENT_POLICY accent = {};
    accent.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    accent.AccentFlags = 0;
    accent.GradientColor = 0x73190F0F; 

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &accent;
    data.cbData = sizeof(accent);

    return SetWCA(hwnd, &data) != FALSE;
}

// ============================================================================
// Flip3DCompApp::CreateAppWindow
// uDWM Flip3D input window: borderless popup, topmost, full virtual desktop.
// ============================================================================
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
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST |WS_EX_TOOLWINDOW,
        L"Flip3DCompClass",
        L"",
        WS_POPUP,
        x, y, w, h,
        nullptr, 
        nullptr,
        m_hInstance,
        this);

    if (!m_hwnd)
        return false;
    
    BOOL exclude = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_EXCLUDED_FROM_PEEK, &exclude, sizeof(exclude));
    DrawAcrylic(m_hwnd);

    m_rtl = (GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;

    RECT client = {};
    if (GetClientRect(m_hwnd, &client))
    {
        m_width  = std::max(1u, (UINT)(client.right  - client.left));
        m_height = std::max(1u, (UINT)(client.bottom - client.top));
    }
    
    return m_hwnd != nullptr;
}
