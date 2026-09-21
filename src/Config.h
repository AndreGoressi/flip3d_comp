// ============================================================================
// Config.h — Constants, enums, type aliases, and DWM thumbnail API definitions
// ============================================================================
// Mirrors the constant / enum organization pattern from CFlip3D.h
// ============================================================================
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <dwmapi.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi1_2.h>

// ============================================================================
// Type aliases
// ============================================================================
using Microsoft::WRL::ComPtr;
using Matrix4x4 = D2D_MATRIX_4X4_F;

// ============================================================================
// uDWM animation / camera constants
// ============================================================================
constexpr float  kEnterExitDurationSec     = 0.333f;    // uDWM binary: 0.25s
constexpr float  kExitDurationSec          = 0.333f;    // uDWM: same timeline for exit + exit-rotate

// Timeline easing: CSS cubic-bezier(0, 0, 0, 1) — fast start, soft landing
constexpr float  kTimelineBezierX1 = 0.0f;
constexpr float  kTimelineBezierY1 = 0.0f;
constexpr float  kTimelineBezierX2 = 0.0f;
constexpr float  kTimelineBezierY2 = 1.0f;

constexpr float  kScrollSmoothTimeSec      = 0.08f;    // smooth-scroll ease time constant
constexpr float  kScrollSettleEpsilon      = 0.002f;   // scrollPos≈scrollTarget threshold (browse)

// Missing from this fork's Config.h — pulled from milestprower92/flip3d_comp,
// which is where the Update.cpp/Input.cpp code referencing them originated.
constexpr float  kScrollWheelNotchFraction = 1.0f;     // carousel slots per mouse wheel notch
constexpr float  kWheelActiveTimeoutSec    = 0.15f;    // throttle repeated wheel processing (uDWM CFlip3D::OnWheel)
constexpr float  kKeyRepeatIntervalSec     = 0.135f;   // throttle repeated keydown processing (uDWM CFlip3D::OnKey)
constexpr float  kHeldKeyStartDelaySec     = 0.20f;    // delay continuous rotation so a key tap moves one card
constexpr float  kHeldKeyRotateSpeed       = 6.0f;     // carousel slots per second while a navigation key is held
constexpr float  kOpeningTabDelaySec       = 0.40f;    // delay the first automatic Tab move after opening

constexpr float  kRotateListDurationSec    = 0.175f;   // uDWM g_secRotateListDuration
constexpr float  kNearPlaneEdgeSize        = 1.15f;    // near plane half-extent
constexpr float  kNearPlaneDistance        = 1.0f;     // near plane Z distance
constexpr int    kMaxVisibleCards          = 10;       // max simultaneously visible cards
constexpr int    kMaxCards                 = 24;       // absolute max cards in carousel

enum class CardThumbnailQuality
{
    VeryLow,
    Low,
    Medium,
    High,
    Native,
};

constexpr float kRestoreRevealProgress = 0.18f;

constexpr CardThumbnailQuality kCardThumbnailQuality = CardThumbnailQuality::Medium; 
// This controls the DWM thumbnail resolution used by 3D window cards. By default Windows Vista & 7 have it set to a Medium value (0.50f)

// Pre-computed camera poses (radians)
constexpr float kCameraFinalTranslateX = -1.1f;
constexpr float kCameraFinalTranslateY =  0.35f;   // uDWM InitializeModelAndCamera
constexpr float kCameraFinalTranslateZ =  0.35f;
constexpr float kCameraFinalRotateX    =  0.08726646f;   //  5.0 deg pitch
constexpr float kCameraFinalRotateY    =  0.43633232f;   // 25.0 deg yaw
constexpr float kCameraFinalRotateZ    =  0.061086524f;  //  3.5 deg roll

// Bezier control points for carousel path
constexpr float kBezierControls[3][3] = {
    { -2.0f,   0.5f,   -2.25f },
    { -1.8f,   0.55f,  -1.25f },
    { -1.45f, -0.1f,   -0.3f  },
};

constexpr float CardThumbnailQualityScale()
{
    switch (kCardThumbnailQuality)
    {
    case CardThumbnailQuality::VeryLow:    return 0.10f;
    case CardThumbnailQuality::Low:    return 0.25f;
    case CardThumbnailQuality::Medium: return 0.50f;
    case CardThumbnailQuality::High:    return 0.75f;
    case CardThumbnailQuality::Native:   return 1.00f;
    }
    return 1.00f;
}

// Normalization Bezier coefficients (quadratic)
constexpr float kNormalizationBezier[3] = { 1.0f, 0.85f, 0.75f };

// uDWM CTopLevelWindow3D::GetFinalMinRect / flip3d BuildFinalMinRect
constexpr float kFinalMinRectWidthPercentage = 0.6f;
constexpr float kFinalMinRectLeftPercentage  = 0.3f;

// ============================================================================
// Enums
// ============================================================================

// Interpolation mode for timeline-driven animations
enum class InterpolationMode
{
    Linear      = 0,
    Cubic       = 1,  // uDWM CTimeline: smoothstep t²(3-2t)
    CubicBezier = 2,  // CSS cubic-bezier (Config.h kTimelineBezier*)
};

// Decoupled display-slot phase during smooth-scroll list wrap (uDWM cycle card).
enum class CarouselWrapPhase
{
    None          = 0,
    ExitingFront  = 1,  // slot → [-1, 0]   front fade-out (forward scroll)
    EnteringBack  = 2,  // opacity slot span→listSlot; position stays on list slot
    EnteringFront = 3,  // slot → [-1, 0]   front fade-in (backward scroll)
};

// Flip3D view state machine (uDWM CFlip3D::Flip3DState ordering preserved)
enum class ViewState
{
    Inactive                  = 0,  // not visible
    Enter                     = 1,  // animating in
    Exit                      = 2,  // animating out (stationary carousel)
    Interactive               = 3,  // fully visible, accepting input
    ExitRepeatedRotate        = 4,  // exit flatten + discrete list rotation in parallel
};

enum class PeekTypes : long
{
    /// <summary>
    /// This flag is here only for completeness and is not used
    /// </summary>
    NotUsed = 0,

    /// <summary>
    /// Denotes that the Peek API is to operate on the desktop
    /// </summary>
    Desktop = 1,

    /// <summary>
    /// Denotes that the Peek API is to operate on a window.
    /// </summary>
    Window = 3
};

enum ACCENT_STATE
{
    ACCENT_DISABLED                     = 0,
    ACCENT_ENABLE_GRADIENT              = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT   = 2,
    ACCENT_ENABLE_BLURBEHIND            = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND     = 4,
    ACCENT_ENABLE_HOSTBACKDROP          = 5,
};

struct ACCENT_POLICY
{
    ACCENT_STATE AccentState;
    DWORD        AccentFlags;
    DWORD        GradientColor;   // Format 0xAABBGGRR
    DWORD        AnimationId;
};

enum WINDOWCOMPOSITIONATTRIB
{
    WCA_ACCENT_POLICY = 19
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID                   pvData;
    SIZE_T                  cbData;
};

enum ZBID
{
    ZBID_DEFAULT = 0,
    ZBID_DESKTOP = 1,
    ZBID_UIACCESS = 2,

    ZBID_IMMERSIVE_IHM = 3,
    ZBID_IMMERSIVE_NOTIFICATION = 4,
    ZBID_IMMERSIVE_APPCHROME = 5,
    ZBID_IMMERSIVE_MOGO = 6,
    ZBID_IMMERSIVE_EDGY = 7,
    ZBID_IMMERSIVE_INACTIVEMOBODY = 8,
    ZBID_IMMERSIVE_INACTIVEDOCK = 9,
    ZBID_IMMERSIVE_ACTIVEMOBODY = 10,
    ZBID_IMMERSIVE_ACTIVEDOCK = 11,
    ZBID_IMMERSIVE_BACKGROUND = 12,
    ZBID_IMMERSIVE_SEARCH = 13,

    ZBID_GENUINE_WINDOWS = 14,
    ZBID_IMMERSIVE_RESTRICTED = 15,

    ZBID_SYSTEM_TOOLS = 16,

    ZBID_LOCK = 17,
    ZBID_ABOVELOCK_UX = 18
};

// ============================================================================
// Private DWM thumbnail API (dwmapi.dll ordinal exports)
// ============================================================================
constexpr DWORD DWM_TNP_FORCECVI         = 0x40000000;
constexpr DWORD DWM_TNP_DISABLEFORCECVI  = 0x80000000;
constexpr DWORD DWM_TNP_ENABLE3D         = 0x04000000;

// DwmpCreateSharedThumbnailVisual dwThumbnailFlags (see dwthumbnailflags-reference.md)
constexpr DWORD DWM_TNF_DWMWINDOW       = 0x2;   // fDwmWindow → PostMessage(0x327) on size change

// Posted to hwndDestination when a DWM_TNF_DWMWINDOW thumbnail source resizes.
constexpr UINT WM_DWMTHUMBNAILSOURCESIZECHANGED = 0x327;

using DwmpCreateSharedThumbnailVisual_fn = HRESULT (WINAPI *)(
    HWND hwndDestination, HWND hwndSource, DWORD dwThumbnailFlags,
    DWM_THUMBNAIL_PROPERTIES* pThumbnailProperties,
    void* pDCompDevice, void** ppVisual, HTHUMBNAIL* phThumbnailId);

using DwmpQueryWindowThumbnailSourceSize_fn = HRESULT (WINAPI *)(
    HWND hwndSource, BOOL fSourceClientAreaOnly, SIZE* pSize);

using GetWindowMinimizeRect_fn = BOOL (WINAPI *)(HWND, LPRECT);

using DwmpUpdateDesktopThumbnail_fn = HRESULT (WINAPI *)(
    HWND hwnd, LPCRECT rcDest, LPCRECT rcSrc, BYTE opacity, DWORD dwFlags);
//new
using DwmpActivateLivePreview_fn = HRESULT(WINAPI*)(
    BOOL fEnable, HWND hPeekWindow, HWND hTopmostWindow, UINT peekType, void* reserved);

using SetWindowCompositionAttribute_fn = BOOL(WINAPI*)(
    HWND hWnd, void* pData);

using CreateWindowInBand_fn = HWND(WINAPI*)(
    DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, 
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam, DWORD dwBand);

using NtUserEnableIAMAccess_fn = BOOL(WINAPI*)(
    ULONG64 key, BOOL enable);
    
