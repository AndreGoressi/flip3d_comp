// ============================================================================
// Flip3DComp_Window.cpp — Window creation + enumeration
// ============================================================================
#include "Flip3DComp.h"
#include "banding.h"
#include <algorithm>
#include "WindowBand.h"
#include <vector>
#include <Windows.h>




#include <fstream>
#include <iomanip>

#include <Psapi.h>

#pragma comment(lib, "Psapi.lib")


BOOL CALLBACK DumpBandsEnumProc(
    HWND hwnd,
    LPARAM lParam)
{
    auto* file =
        reinterpret_cast<std::wofstream*>(lParam);

    using GetWindowBand_t =
        BOOL(WINAPI*)(HWND, PDWORD);

    static auto pGetWindowBand =
        reinterpret_cast<GetWindowBand_t>(
            GetProcAddress(
                GetModuleHandleW(L"user32.dll"),
                "GetWindowBand"));

    DWORD band = 0;

    if (pGetWindowBand)
    {
        pGetWindowBand(hwnd, &band);
    }

    wchar_t title[512] = {};
    wchar_t cls[256] = {};

    GetWindowTextW(
        hwnd,
        title,
        _countof(title));

    GetClassNameW(
        hwnd,
        cls,
        _countof(cls));

    DWORD pid = 0;

    GetWindowThreadProcessId(
        hwnd,
        &pid);

    wchar_t processName[MAX_PATH] = {};

    HANDLE hProcess =
        OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION |
            PROCESS_VM_READ,
            FALSE,
            pid);

    if (hProcess)
    {
        GetModuleBaseNameW(
            hProcess,
            nullptr,
            processName,
            MAX_PATH);

        CloseHandle(hProcess);
    }

    (*file)
        << L"Band    : "
        << band
        << L"\r\n"

        << L"Class   : "
        << cls
        << L"\r\n"

        << L"Title   : "
        << title
        << L"\r\n"

        << L"Process : "
        << processName
        << L"\r\n"

        << L"PID     : "
        << pid
        << L"\r\n"

        << L"HWND    : 0x"
        << std::hex
        << (UINT_PTR)hwnd
        << std::dec
        << L"\r\n"

        << L"------------------------------------------"
        << L"\r\n";

    return TRUE;
}

void Flip3DCompApp::DumpWindowBands()
{
    std::wofstream file(
        L"WindowBands.txt",
        std::ios::trunc);

    if (!file.is_open())
        return;

    file
        << L"==== Window Band Dump ===="
        << L"\r\n\r\n";

    EnumWindows(
        DumpBandsEnumProc,
        reinterpret_cast<LPARAM>(&file));

    file.close();
}


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
/*void Flip3DCompApp::ApplyFullscreenLayout()
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
}*/
void Flip3DCompApp::ApplyFullscreenLayout()
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
    ATOM atom = RegisterClassExW(&wc);
    if (!atom)
    {
        return false;
    }

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    m_hwnd = WindowBand::CreateBandWindow(
        WS_EX_NOREDIRECTIONBITMAP |
        WS_EX_TOOLWINDOW,
        atom,
        L"Flip3DCompClass",
        WS_POPUP,
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

    if (m_hwnd)
    {
        static bool dumped = false;
    
        if (!dumped)
        {
            DumpWindowBands();
            dumped = true;
        }
    }


    m_rtl = (GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;

    RECT client = {};
    if (GetClientRect(m_hwnd, &client))
    {
        m_width  = std::max(1u, (UINT)(client.right  - client.left));
        m_height = std::max(1u, (UINT)(client.bottom - client.top));
    }

    return true;
}


