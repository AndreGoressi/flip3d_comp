// ============================================================================
// Flip3DComp_ShellHook.cpp — dynamic card list via RegisterShellHookWindow
// ============================================================================
#include "Flip3DComp.h"
#include <algorithm>
#include <vector>
//
bool Flip3DComp::QualifiesForView(HWND hwnd) const
{
    if (!hwnd || hwnd == m_hwnd || hwnd == GetDesktopWindow())
        return false;

    if (!IsWindowVisible(hwnd))
        return false;

    if (m_selectedHwnd && hwnd == m_selectedHwnd)
        return true;

    const LONG_PTR style   = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    if ((style & WS_CHILD) != 0)
        return false;

    const bool isShellDesktop = (hwnd == GetShellWindow());

    if (!isShellDesktop
        && (exStyle & 0x08000080) != 0
        && (exStyle & WS_EX_APPWINDOW) == 0)
        return false;

    if (!isShellDesktop && (exStyle & WS_EX_TOOLWINDOW) != 0)
        return false;

    if (!isShellDesktop
        && (exStyle & WS_EX_APPWINDOW) == 0
        && GetWindow(hwnd, GW_OWNER) != nullptr)
        return false;

    if (IsNeverHiddenWindow(hwnd))
        return false;

    if (!isShellDesktop)
    {
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
            && cloaked != 0)
            return false;
    }
    return true;
}

// ============================================================================
bool Flip3DComp::IsFlip3DViewActive() const
{
    return m_state != ViewState::Inactive;
}

bool Flip3DComp::IsAlwaysOnTop(HWND hwnd) {
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    return (exStyle & WS_EX_TOPMOST) != 0;
}

struct StrippedAotWindowState
{
    LONG_PTR exStyle;
    LONG_PTR originalExStyle = 0;
};

std::unordered_map<HWND, StrippedAotWindowState> Flip3DComp::m_strippedAotWindows;
void Flip3DComp::EnterInteractionOverride()
{
    for (HWND h : EnumerateWindows())
    {
        if (!h || h == m_hwnd || !IsAlwaysOnTop(h))
            continue;

        LONG_PTR exStyle = GetWindowLongPtr(h, GWL_EXSTYLE);
        if (exStyle & WS_EX_TRANSPARENT)
            continue; 

        m_strippedAotWindows[h] = { exStyle };

        SetWindowLongPtr(h, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
        SetWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}
// ============================================================================
// Flip3DComp::LeaveInteractionOverride
// ============================================================================
void Flip3DComp::LeaveInteractionOverride()
{
    for (auto& [hwnd, state] : m_strippedAotWindows)
    {
        if (IsWindow(hwnd))
        {
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, state.originalExStyle);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }
    m_strippedAotWindows.clear();
}
// ============================================================================

bool Flip3DComp::IsNeverHiddenWindow(HWND hwnd) const
{
    if (!hwnd)
        return true;

    if (hwnd == m_hwnd)
        return true;

    if (IsSystemFlyoutProcess(hwnd))
        return true;

    wchar_t cls[64] = {};
    if (!GetClassNameW(hwnd, cls, 63))
        return false;

    return !_wcsicmp(cls, L"Shell_TrayWnd")
        || !_wcsicmp(cls, L"Shell_SecondaryTrayWnd")
        || !_wcsicmp(cls, L"WorkerW")
        || !_wcsicmp(cls, L"NotifyIconOverflowWindow")
        || !_wcsicmp(cls, L"TrayNotifyWnd")
        || !_wcsicmp(cls, L"WindhawkCorner"); 
}

bool Flip3DComp::IsSystemFlyoutProcess(HWND hwnd) const
{
    if (!hwnd)
        return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0)
        return false;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess)
        return false;

    wchar_t path[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    bool found = false;

    if (QueryFullProcessImageNameW(hProcess, 0, path, &size))
    {
        wchar_t* exeName = wcsrchr(path, L'\\');
        exeName = exeName ? exeName + 1 : path;
        found = _wcsicmp(exeName, L"StartMenuExperienceHost.exe") == 0 ||
                _wcsicmp(exeName, L"SearchHost.exe") == 0 ||
                _wcsicmp(exeName, L"SearchUI.exe") == 0 ||
                _wcsicmp(exeName, L"ShellExperienceHost.exe") == 0 ||
                _wcsicmp(exeName, L"TextInputHost.exe") == 0 ||
                _wcsicmp(exeName, L"InputApp.exe") == 0 ||
                _wcsicmp(exeName, L"Widgets.exe") == 0 ||
                _wcsicmp(exeName, L"TabTip.exe") == 0 ||
                _wcsicmp(exeName, L"GameBar.exe") == 0;
    }
    CloseHandle(hProcess);
    return found;
}

// ============================================================================
void Flip3DComp::EnterFlip3DWindowMode()
{
    if (!m_hwnd || m_shellHookRegistered)
        return;

    if (!m_wmShellHook)
        m_wmShellHook = RegisterWindowMessageW(L"SHELLHOOK");

    if (RegisterShellHookWindow(m_hwnd))
        m_shellHookRegistered = true;
}

void Flip3DComp::LeaveFlip3DWindowMode()
{
    if (m_hwnd && m_shellHookRegistered)
    {
        DeregisterShellHookWindow(m_hwnd);
        m_shellHookRegistered = false;
    }
}

// ============================================================================
void Flip3DComp::OnShellHookMessage(WPARAM wParam, LPARAM lParam)
{
    if (!IsFlip3DViewActive())
        return;

    const UINT code = (UINT)(wParam & ~HSHELL_HIGHBIT);
    const HWND hwnd = (HWND)lParam;

    switch (code)
    {
    case HSHELL_WINDOWCREATED:
    case HSHELL_WINDOWACTIVATED:
    case HSHELL_WINDOWREPLACED:
    case HSHELL_RUDEAPPACTIVATED:
        if (hwnd)
            OnWindowShowHide(hwnd);
        break;

    case HSHELL_WINDOWDESTROYED:
        if (hwnd)
        {
            const int idx = FindCardIndex(hwnd);
            if (idx >= 0)
                RemoveCardAt((size_t)idx);
        }
        break;

    default:
        break;
    }
}

// ============================================================================
void Flip3DComp::OnWindowShowHide(HWND hwnd)
{
    if (!IsFlip3DViewActive() || !hwnd || !IsWindow(hwnd))
        return;

    if (hwnd == m_hwnd || IsNeverHiddenWindow(hwnd))
        return;

    const bool qualifies = QualifiesForView(hwnd);
    const int  cardIdx   = FindCardIndex(hwnd);

    if (qualifies)
    {
        if (cardIdx < 0)
            AddCardForWindow(hwnd);
    }
    else if (cardIdx >= 0)
    {
        RemoveCardAt((size_t)cardIdx);
    }
}

