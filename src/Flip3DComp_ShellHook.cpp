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

std::vector<HWND> Flip3DComp::s_strippedTopmostWindows;
BOOL CALLBACK Flip3DComp::RemoveTopmostCallback(HWND hwnd, LPARAM lParam)
{
    auto* pThis = reinterpret_cast<Flip3DComp*>(lParam);
    if (!pThis || pThis->IsNeverHiddenWindow(hwnd))
        return TRUE;

    if (IsWindowVisible(hwnd) && !IsIconic(hwnd))
    {
        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if (exStyle & WS_EX_TOPMOST)
        {
            if (s_strippedTopmostWindows.empty()) {
                s_strippedTopmostWindows.reserve(8);
            }

            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, (exStyle & ~WS_EX_TOPMOST) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
            s_strippedTopmostWindows.push_back(hwnd);
        }
    }
    return TRUE;
}

void Flip3DComp::StripCompetingTopmost()
{
    s_strippedTopmostWindows.clear();
    EnumWindows(RemoveTopmostCallback, reinterpret_cast<LPARAM>(this));
}

void Flip3DComp::RestoreCompetingTopmost()
{
    for (HWND hwnd : s_strippedTopmostWindows)
    {
        if (IsWindow(hwnd))
        {
            LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, (exStyle | WS_EX_TOPMOST) & ~WS_EX_LAYERED & ~WS_EX_TRANSPARENT);
        }
    }
    s_strippedTopmostWindows.clear();
}

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

