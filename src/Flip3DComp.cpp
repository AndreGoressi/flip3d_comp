// ============================================================================
// Flip3DComp.cpp — Application lifecycle: Initialize, Run, WndProc, HandleMessage
// ============================================================================
#include "Flip3DComp.h"
#include "Flip3DAccessible.h"

#include <windowsx.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// ============================================================================
// Flip3DComp::Initialize
// ============================================================================
bool Flip3DComp::Initialize(HINSTANCE hInstance)
{
    m_hInstance = hInstance;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!LoadUndocApi())
        return false;
    //
    BuildCards();
    //
    if (!InitializeDCompStage())
    {
        if (m_initError.empty())
            m_initError = L"Failed to create the Flip3D input window.";
        return false;
    }
    UpdateMonitorRect();

    if (m_pfnActivateLivePreview)
    {
        m_pfnActivateLivePreview(TRUE, m_hwnd, nullptr, static_cast<UINT>(PeekTypes::Desktop), nullptr);
    }

    if (FAILED(InitComposition()))
    {
        if (m_initError.empty())
            m_initError = L"Failed to initialize DirectComposition.";
        return false;
    }

    if (FAILED(CreateCardVisuals()))
    {
        if (m_initError.empty())
            m_initError = L"Failed to create DWM thumbnail visuals.";
        return false;
    }
    
    m_state = ViewState::Enter;
    m_animEnter.Restart(0.0f, 1.0f, kEnterExitDurationSec);
    m_prevFrame = std::chrono::steady_clock::now();

    EnterFlip3DWindowMode();
    InitAccessibility();  
    Update(0.0f);
    
    return true;
}

bool IsMonitorHorizontal(HMONITOR hMon)
{
    MONITORINFOEX mi = { sizeof(mi) };
    if (!GetMonitorInfoW(hMon, (MONITORINFO*)&mi))
        return true; 

    DEVMODE dm = { sizeof(dm) };
    dm.dmSize = sizeof(dm);

    if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm))
    {
        if (dm.dmDisplayOrientation == DMDO_90 || dm.dmDisplayOrientation == DMDO_270)
        {
            return false;
        }
    }
    return true; 
}

HMONITOR Flip3DComp::GetTargetMonitor() const
{
    if (m_hwnd && IsWindow(m_hwnd))
    {
        HMONITOR hMonWindow = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONULL);
        if (hMonWindow && IsMonitorHorizontal(hMonWindow))
            return hMonWindow;
    }
    POINT ptCursor;
    if (GetCursorPos(&ptCursor))
    {
        HMONITOR hMonCursor = MonitorFromPoint(ptCursor, MONITOR_DEFAULTTONULL);
        if (hMonCursor && IsMonitorHorizontal(hMonCursor))
            return hMonCursor;
    }
    HMONITOR hPrimary = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    if (IsMonitorHorizontal(hPrimary))
    {
        return hPrimary;
    }
    HMONITOR hValidHorizontal = nullptr;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMon, HDC, LPRECT, LPARAM lParam) -> BOOL {
        auto* pResult = reinterpret_cast<HMONITOR*>(lParam);
        if (IsMonitorHorizontal(hMon))
        {
            *pResult = hMon;
            return FALSE; 
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&hValidHorizontal));

    if (hValidHorizontal)
        return hValidHorizontal;
    //
    return hPrimary;
}
// ============================================================================
// Flip3DComp::Run
// ============================================================================
int Flip3DComp::Run()
{
    MSG msg = {};

    while (msg.message != WM_QUIT)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (msg.message == WM_QUIT)
            break;

        if (!m_minimized)
        {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - m_prevFrame).count();
            m_prevFrame = now;
            Update(std::min(dt, 0.05f));
        }
        else
        {
            WaitMessage();
            m_prevFrame = std::chrono::steady_clock::now();
        }
    }
    UnloadUndocApi();
    return (int)msg.wParam;
}

// ============================================================================
// Flip3DComp::WndProc — window procedure
// ============================================================================
LRESULT CALLBACK Flip3DComp::WndProc(HWND hwnd, UINT msg,
                                         WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        auto* self = (Flip3DComp*)((CREATESTRUCTW*)lParam)->lpCreateParams;
        if (self)
        {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
            self->m_hwnd = hwnd;
        }
    }

    auto* self = (Flip3DComp*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    return self
        ? self->HandleMessage(msg, wParam, lParam)
        : DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Flip3DComp::HandleMessage
// ============================================================================
LRESULT Flip3DComp::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            m_minimized = true;
            return 0;
        }
        m_minimized = false;
        m_width     = std::max(1u, (UINT)LOWORD(lParam));
        m_height    = std::max(1u, (UINT)HIWORD(lParam));
        UpdateMonitorRect();
        return 0;

    case WM_DISPLAYCHANGE:
        ApplyFullscreenLayout();
        UpdateMonitorRect();
        return 0;

    case WM_DWMTHUMBNAILSOURCESIZECHANGED:
        // Each registered thumbnail posts 0x327 independently; coalesce to one
        // refresh per frame in Update() (wParam = adapter LUID low part).
        m_thumbnailsDirty = true;
        return 0;

    case WM_MOUSEWHEEL:
        OnWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    case WM_MOUSEMOVE:
        m_hitHwnd = HitTest3DScene(
            (LONG)(short)LOWORD(lParam),
            (LONG)(short)HIWORD(lParam));
        SetCursor(LoadCursorW(nullptr, m_hitHwnd ? IDC_HAND : IDC_ARROW));
        return 0;

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, m_hitHwnd ? IDC_HAND : IDC_ARROW));
        return TRUE;

    case WM_LBUTTONDOWN:
        OnMouse((LONG)(short)LOWORD(lParam),
                (LONG)(short)HIWORD(lParam), true);
        return 0;

    case WM_KEYDOWN:
        if (OnKey(true, (UINT)wParam, lParam))
            return 0;
        break;
        
    // new
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            if (m_state != ViewState::Exit && m_state != ViewState::ExitRepeatedRotate)
            {
                ExitView();
            }
        }
        return 0;
    //close_if_focus_lost

    case WM_CLOSE:
        if (m_state == ViewState::Exit ||
            m_state == ViewState::ExitRepeatedRotate)
        {
            DestroyWindow(m_hwnd);
        }
        else
        {
            ExitView();
        }
        return 0;

    case WM_GETOBJECT:
    {
        if ((DWORD)lParam != OBJID_CLIENT && (DWORD)lParam != 0)
            break;

        IAccessible* pAccessible = GetAccessibleObject();
        if (!pAccessible)
            break;

        return LresultFromObject(IID_IAccessible, wParam, pAccessible);
    }

    case WM_DESTROY:
        //
        ShutdownAccessibility();
        LeaveFlip3DWindowMode();
        PostQuitMessage(0);
        return 0;
    }

    if (m_wmShellHook && msg == m_wmShellHook)
    {
        OnShellHookMessage(wParam, lParam);
        return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}
