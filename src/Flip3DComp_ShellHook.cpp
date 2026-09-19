// ============================================================================
// Flip3DComp_ShellHook.cpp — dynamic card list via RegisterShellHookWindow
// ============================================================================
#include "Flip3DComp.h"
#include <algorithm>
// ============================================================================
// Flip3DComp::QualifiesForView
// ============================================================================
namespace {

Flip3DComp* s_instance = nullptr;
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && s_instance)
    {
        const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        if (wParam == WM_MOUSEWHEEL)
        {
            const int delta = GET_WHEEL_DELTA_WPARAM(info->mouseData);
            s_instance->OnWheel(delta);
            return 1; 
        }
        else if (wParam == WM_LBUTTONDOWN || wParam == WM_LBUTTONUP)
        {
            bool pressed = (wParam == WM_LBUTTONDOWN);
            s_instance->OnMouse(info->pt.x, info->pt.y, pressed);
            // return 1; 
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

} // namespace

// ============================================================================
// Flip3DComp::InstallMouseWheelHook
// ============================================================================
void Flip3DComp::InitializeMouseWheelHook()
{
    if (m_mouseHook)
        return;

    s_instance = this;
    m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);
}

// ============================================================================
// Flip3DComp::RemoveMouseWheelHook
// ============================================================================
void Flip3DComp::RemoveMouseWheelHook()
{
    if (m_mouseHook)
    {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
    if (s_instance == this)
        s_instance = nullptr;
}

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
        || !_wcsicmp(cls, L"WorkerW");
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

