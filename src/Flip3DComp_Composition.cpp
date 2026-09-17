// ============================================================================
// Flip3DComp_Composition.cpp — DirectComposition device init, desktop wash
// ============================================================================

#include "Flip3DComp.h"

#include <algorithm>

#include <cmath>

#include <cwchar>

#include <vector>

namespace {

  struct EnumMonitorsContext {
    std::vector < MONITORINFO > monitors;
  };

  BOOL CALLBACK EnumMonitorsProc(HMONITOR hMon, HDC, LPRECT, LPARAM lParam) {
    auto * ctx = reinterpret_cast < EnumMonitorsContext * > (lParam);

    MONITORINFO mi = {
      sizeof(mi)
    };

    if (GetMonitorInfoW(hMon, & mi))
      ctx -> monitors.push_back(mi);

    return TRUE;
  }

  std::vector < MONITORINFO > EnumerateMonitors() {
    EnumMonitorsContext ctx;

    EnumDisplayMonitors(
      nullptr,
      nullptr,
      EnumMonitorsProc,
      (LPARAM) & ctx);

    return ctx.monitors;
  }

} // namespace

// ============================================================================
// Gaussian blur configuration
//
// DirectComposition applies the effect to the entire subtree rooted at
// shellContainer. The DWM shell thumbnail is therefore rasterized into an
// implicit off-screen surface and that surface is passed through the blur.
//
// Higher values = stronger / wider blur.
// ============================================================================

namespace {

  constexpr float kDesktopBlurStandardDeviation = 20.0 f;

}

// ============================================================================
// Shared wash surface
//
// Kept for compatibility with the existing composition structure.
//
// IMPORTANT:
// This surface is intentionally NOT assigned as content to washVisual below.
// An opaque wash surface would cover the blurred desktop thumbnail.
//
// The actual desktop appearance now comes from:
//
//     DWM shell thumbnail
//             |
//             v
//     shellContainer
//             |
//             v
//     GaussianBlurEffect
//             |
//             v
//         rootVisual
//
// ============================================================================

namespace {

  HRESULT CreateSharedWashSurface(
    ID3D11Device * d3d,
    IDCompositionDesktopDevice * dcomp,
    ComPtr < IDCompositionSurface > & outSurface) {
    if (!d3d || !dcomp)
      return E_INVALIDARG;

    ComPtr < IDCompositionSurfaceFactory > sf;

    HRESULT hr = dcomp -> CreateSurfaceFactory(
      d3d, &
      sf);

    if (FAILED(hr))
      return hr;

    ComPtr < IDCompositionSurface > bg;

    hr = sf -> CreateSurface(
      1,
      1,
      DXGI_FORMAT_B8G8R8A8_UNORM,
      DXGI_ALPHA_MODE_IGNORE, &
      bg);

    if (FAILED(hr))
      return hr;

    ComPtr < IDXGISurface > dxgiSurf;

    POINT offset = {};

    hr = bg -> BeginDraw(
      nullptr,
      IID_PPV_ARGS( & dxgiSurf), &
      offset);

    if (SUCCEEDED(hr)) {
      ComPtr < ID3D11Texture2D > tex;
      ComPtr < ID3D11RenderTargetView > rtv;

      if (SUCCEEDED(dxgiSurf.As( & tex)) &&
        SUCCEEDED(
          d3d -> CreateRenderTargetView(
            tex.Get(),
            nullptr, &
            rtv))) {
        ComPtr < ID3D11DeviceContext > ctx;

        d3d -> GetImmediateContext( & ctx);

        const float wash[4] = {
          0.04 f,
          0.05 f,
          0.08 f,
          1.0 f
        };

        ctx -> ClearRenderTargetView(
          rtv.Get(),
          wash);
      }

      bg -> EndDraw();
    }

    outSurface = std::move(bg);

    return outSurface ? S_OK : E_FAIL;
  }

} // namespace

// ============================================================================
// Flip3DComp::InitComposition
// ============================================================================

HRESULT Flip3DComp::InitComposition() {
  if (!m_d3dDevice)
    return E_FAIL;

  ComPtr < IDXGIDevice > dxgi;

  HRESULT hr = m_d3dDevice.As( & dxgi);

  if (FAILED(hr))
    return hr;

  // ------------------------------------------------------------------------
  // Create DirectComposition device
  // ------------------------------------------------------------------------

  hr = DCompositionCreateDevice2(
    dxgi.Get(),
    IID_PPV_ARGS( & m_dcompDevice));

  if (FAILED(hr))
    return hr;

  // ------------------------------------------------------------------------
  // Create composition target
  // ------------------------------------------------------------------------

  hr = m_dcompDevice -> CreateTargetForHwnd(
    m_hwnd,
    TRUE, &
    m_dcompTarget);

  if (FAILED(hr))
    return hr;

  // ------------------------------------------------------------------------
  // Root visual
  // ------------------------------------------------------------------------

  ComPtr < IDCompositionVisual2 > root;

  hr = m_dcompDevice -> CreateVisual( &
    root);

  if (FAILED(hr))
    return hr;

  m_rootVisual = root;

  hr = m_dcompTarget -> SetRoot(
    root.Get());

  if (FAILED(hr))
    return hr;

  // ------------------------------------------------------------------------
  // Scene visual
  //
  // All Flip3D cards are placed under this visual.
  // The desktop/backdrop visuals are inserted before it.
  // ------------------------------------------------------------------------

  ComPtr < IDCompositionVisual2 > sceneBase;

  hr = m_dcompDevice -> CreateVisual( &
    sceneBase);

  if (FAILED(hr))
    return hr;

  hr = sceneBase.As( & m_sceneVisual);

  if (FAILED(hr))
    return hr;

  m_sceneVisual -> SetDepthMode(
    DCOMPOSITION_DEPTH_MODE_TREE);

  // ------------------------------------------------------------------------
  // Attach scene to root
  // ------------------------------------------------------------------------

  ComPtr < IDCompositionVisual > rootBase;

  hr = root.As( & rootBase);

  if (FAILED(hr))
    return hr;

  hr = rootBase -> AddVisual(
    m_sceneVisual.Get(),
    FALSE,
    nullptr);

  if (FAILED(hr))
    return hr;

  return m_dcompDevice -> Commit();
}

// ============================================================================
// Flip3DComp::DestroyMonitorBackdrops
// ============================================================================

void Flip3DComp::DestroyMonitorBackdrops() {
  if (!m_rootVisual) {
    m_monitorBackdrops.clear();
    return;
  }

  ComPtr < IDCompositionVisual > rootBase;

  if (SUCCEEDED(m_rootVisual.As( & rootBase))) {
    for (auto & mon: m_monitorBackdrops) {
      // ----------------------------------------------------------------
      // Remove shell thumbnail container
      // ----------------------------------------------------------------

      if (mon.shellContainer) {
        rootBase -> RemoveVisual(
          mon.shellContainer.Get());
      }

      // ----------------------------------------------------------------
      // Remove wash visual
      // ----------------------------------------------------------------

      if (mon.washVisual) {
        rootBase -> RemoveVisual(
          mon.washVisual.Get());
      }

      // ----------------------------------------------------------------
      // Unregister DWM thumbnail
      // ----------------------------------------------------------------

      if (mon.hShellThumb) {
        DwmUnregisterThumbnail(
          mon.hShellThumb);

        mon.hShellThumb = nullptr;
      }
    }
  }

  m_monitorBackdrops.clear();
}

// ============================================================================
// Flip3DComp::UpdateBackdropLayout
//
// Per-monitor shell thumbnail.
//
// The shell thumbnail is positioned over the monitor work area and then
// blurred by the Gaussian effect attached to shellContainer.
//
// Client coordinates match the virtual-desktop-sized Flip3D window.
// ============================================================================

void Flip3DComp::UpdateBackdropLayout() {
  const int vx =
    GetSystemMetrics(SM_XVIRTUALSCREEN);

  const int vy =
    GetSystemMetrics(SM_YVIRTUALSCREEN);

  HWND shell = GetShellWindow();

  RECT shellWnd = {};

  if (shell)
    GetWindowRect(
      shell, &
      shellWnd);

  for (auto & mon: m_monitorBackdrops) {
    // --------------------------------------------------------------------
    // Monitor wash geometry
    //
    // The wash visual is currently intentionally empty.
    // Keep its geometry because the existing MonitorBackdrop structure
    // still uses it and this preserves the original layout architecture.
    // --------------------------------------------------------------------

    const LONG washW =
      mon.rcMonitor.right -
      mon.rcMonitor.left;

    const LONG washH =
      mon.rcMonitor.bottom -
      mon.rcMonitor.top;

    const float washX =
      (float)(mon.rcMonitor.left - vx);

    const float washY =
      (float)(mon.rcMonitor.top - vy);

    if (mon.washVisual) {
      ComPtr < IDCompositionVisual2 > wash2;

      if (SUCCEEDED(
          mon.washVisual.As( & wash2))) {
        const D2D_MATRIX_3X2_F xform = {
          (float) std::max(washW, 1 L),
          0. f,

          0. f,
          (float) std::max(washH, 1 L),

          washX,
          washY,
        };

        wash2 -> SetTransform(
          xform);
      }
    }

    // --------------------------------------------------------------------
    // Shell thumbnail geometry
    // --------------------------------------------------------------------

    const LONG shellW =
      mon.rcWork.right -
      mon.rcWork.left;

    const LONG shellH =
      mon.rcWork.bottom -
      mon.rcWork.top;

    const float shellX =
      (float)(mon.rcWork.left - vx);

    const float shellY =
      (float)(mon.rcWork.top - vy);

    if (mon.shellContainer) {
      ComPtr < IDCompositionVisual2 > shell2;

      if (SUCCEEDED(
          mon.shellContainer.As( & shell2))) {
        const D2D_MATRIX_3X2_F xform = {
          1. f,
          0. f,

          0. f,
          1. f,

          shellX,
          shellY,
        };

        shell2 -> SetTransform(
          xform);
      }
    }

    // --------------------------------------------------------------------
    // Update DWM thumbnail source/destination
    // --------------------------------------------------------------------

    if (mon.hShellThumb &&
      shellW > 0 &&
      shellH > 0) {
      RECT rcSource = mon.rcWork;

      OffsetRect( &
        rcSource,
        -shellWnd.left,
        -shellWnd.top);

      DWM_THUMBNAIL_PROPERTIES tp = {};

      tp.dwFlags =
        DWM_TNP_VISIBLE |
        DWM_TNP_RECTDESTINATION |
        DWM_TNP_RECTSOURCE |
        DWM_TNP_DISABLEFORCECVI;

      tp.fVisible = TRUE;

      tp.rcSource =
        rcSource;

      tp.rcDestination = {
        0,
        0,
        shellW,
        shellH
      };

      DwmUpdateThumbnailProperties(
        mon.hShellThumb, &
        tp);
    }
  }

  if (m_dcompDevice)
    m_dcompDevice -> Commit();
}

// ============================================================================
// Flip3DComp::RebuildMonitorBackdropsIfNeeded
// ============================================================================

bool Flip3DComp::RebuildMonitorBackdropsIfNeeded() {
  // ------------------------------------------------------------------------
  // Enumerate current monitor layout
  // ------------------------------------------------------------------------

  const std::vector < MONITORINFO > monitors =
    EnumerateMonitors();

  if (monitors.empty())
    return false;

  // ------------------------------------------------------------------------
  // Determine whether the existing backdrop layout can be reused
  // ------------------------------------------------------------------------

  bool layoutSame =
    monitors.size() ==
    m_monitorBackdrops.size();

  if (layoutSame) {
    for (size_t i = 0; i < monitors.size();
      ++i) {
      const MONITORINFO & a =
        monitors[i];

      const MonitorBackdrop & b =
        m_monitorBackdrops[i];

      if (a.rcMonitor.left !=
        b.rcMonitor.left ||
        a.rcMonitor.top !=
        b.rcMonitor.top ||
        a.rcMonitor.right !=
        b.rcMonitor.right ||
        a.rcMonitor.bottom !=
        b.rcMonitor.bottom ||
        a.rcWork.left !=
        b.rcWork.left ||
        a.rcWork.top !=
        b.rcWork.top ||
        a.rcWork.right !=
        b.rcWork.right ||
        a.rcWork.bottom !=
        b.rcWork.bottom) {
        layoutSame = false;
        break;
      }
    }
  }

  // ------------------------------------------------------------------------
  // Existing layout is still valid
  // ------------------------------------------------------------------------

  if (layoutSame) {
    UpdateBackdropLayout();
    return false;
  }

  // ------------------------------------------------------------------------
  // Rebuild everything
  // ------------------------------------------------------------------------

  DestroyMonitorBackdrops();

  // ------------------------------------------------------------------------
  // Create the legacy wash surface if necessary.
  //
  // The surface remains available for compatibility with the existing
  // class state, but is intentionally NOT attached to washVisual below.
  // ------------------------------------------------------------------------

  if (!m_washSurface &&
    m_d3dDevice &&
    m_dcompDevice) {
    CreateSharedWashSurface(
      m_d3dDevice.Get(),
      m_dcompDevice.Get(),
      m_washSurface);
  }

  // ------------------------------------------------------------------------
  // Get shell window
  // ------------------------------------------------------------------------

  HWND shell =
    GetShellWindow();

  if (!shell ||
    !m_dcompDevice ||
    !m_rootVisual) {
    return false;
  }

  // ------------------------------------------------------------------------
  // Root visual interface
  // ------------------------------------------------------------------------

  ComPtr < IDCompositionVisual > rootBase;

  if (FAILED(
      m_rootVisual.As( & rootBase))) {
    return false;
  }

  // ------------------------------------------------------------------------
  // Shell window rectangle
  // ------------------------------------------------------------------------

  RECT shellWnd = {};

  GetWindowRect(
    shell, &
    shellWnd);

  // ------------------------------------------------------------------------
  // Build one backdrop per monitor
  // ------------------------------------------------------------------------

  for (const MONITORINFO & mi: monitors) {
    MonitorBackdrop mon = {};

    mon.rcMonitor = mi.rcMonitor;
    mon.rcWork = mi.rcWork;

    const LONG shellW =
      mon.rcWork.right -
      mon.rcWork.left;

    const LONG shellH =
      mon.rcWork.bottom -
      mon.rcWork.top;

    if (shellW <= 0 ||
      shellH <= 0) {
      continue;
    }

    // --------------------------------------------------------------------
    // Calculate the source rectangle inside the shell window
    // --------------------------------------------------------------------

    RECT rcSource =
      mon.rcWork;

    OffsetRect( &
      rcSource,
      -shellWnd.left,
      -shellWnd.top);

    // --------------------------------------------------------------------
    // DWM thumbnail properties
    //
    // This describes which part of the desktop shell window should be
    // supplied to the DirectComposition thumbnail visual.
    // --------------------------------------------------------------------

    DWM_THUMBNAIL_PROPERTIES tp = {};

    tp.dwFlags =
      DWM_TNP_VISIBLE |
      DWM_TNP_RECTDESTINATION |
      DWM_TNP_RECTSOURCE |
      DWM_TNP_DISABLEFORCECVI;

    tp.fVisible = TRUE;

    tp.rcSource =
      rcSource;

    tp.rcDestination =
      mon.rcWork;

    // --------------------------------------------------------------------
    // Create the private DWM shared thumbnail visual
    // --------------------------------------------------------------------

    void * pv = nullptr;

    HRESULT hr =
      m_pfnCreateSharedThumbVisual(
        m_hwnd,
        shell,
        DWM_TNF_DWMWINDOW, &
        tp,
        m_dcompDevice.Get(), &
        pv, &
        mon.hShellThumb);

    if (FAILED(hr) ||
      !pv) {
      continue;
    }

    // --------------------------------------------------------------------
    // Take ownership of the returned DirectComposition visual
    // --------------------------------------------------------------------

    ComPtr < IDCompositionVisual > thumbBase;

    thumbBase.Attach(
      (IDCompositionVisual * ) pv);

    hr = thumbBase.As( &
      mon.shellThumb);

    if (FAILED(hr)) {
      if (mon.hShellThumb) {
        DwmUnregisterThumbnail(
          mon.hShellThumb);

        mon.hShellThumb = nullptr;
      }

      continue;
    }

    // --------------------------------------------------------------------
    // Create a container around the DWM thumbnail.
    //
    // IMPORTANT:
    //
    // The Gaussian blur is applied to THIS container. DirectComposition
    // therefore creates an implicit off-screen surface for this subtree:
    //
    //     shellContainer
    //          |
    //          +-- thumbBase
    //
    // and feeds that surface into the Gaussian blur.
    // --------------------------------------------------------------------

    ComPtr < IDCompositionVisual2 > shellContainer;

    hr =
      m_dcompDevice -> CreateVisual( &
        shellContainer);

    if (FAILED(hr)) {
      if (mon.hShellThumb) {
        DwmUnregisterThumbnail(
          mon.hShellThumb);

        mon.hShellThumb = nullptr;
      }

      continue;
    }

    hr =
      shellContainer.As( &
        mon.shellContainer);

    if (FAILED(hr)) {
      if (mon.hShellThumb) {
        DwmUnregisterThumbnail(
          mon.hShellThumb);

        mon.hShellThumb = nullptr;
      }

      continue;
    }

    // --------------------------------------------------------------------
    // Put the DWM thumbnail below the blur container
    // --------------------------------------------------------------------

    hr =
      shellContainer -> AddVisual(
        thumbBase.Get(),
        FALSE,
        nullptr);

    if (FAILED(hr)) {
      if (mon.hShellThumb) {
        DwmUnregisterThumbnail(
          mon.hShellThumb);

        mon.hShellThumb = nullptr;
      }

      continue;
    }

    // --------------------------------------------------------------------
    // Create Gaussian blur
    //
    // IDCompositionVisual::SetEffect applies the effect to the complete
    // subtree rooted at shellContainer.
    //
    // SetInput(0, nullptr, 0) tells the effect to use the implicit
    // off-screen surface generated from that visual subtree.
    // --------------------------------------------------------------------

    bool blurOk = false;

    ComPtr < IDCompositionDevice3 > dcompDevice3;

    if (SUCCEEDED(
        m_dcompDevice.As( & dcompDevice3))) {
      ComPtr < IDCompositionGaussianBlurEffect >
        blurEffect;

      if (SUCCEEDED(
          dcompDevice3 -> CreateGaussianBlurEffect( &
            blurEffect))) {
        HRESULT bhr =
          blurEffect -> SetInput(
            0,
            nullptr,
            0);

        // ------------------------------------------------------------
        // Gaussian standard deviation
        //
        // 20.0 gives a clearly visible desktop blur while keeping
        // the effect reasonably controlled.
        // ------------------------------------------------------------

        if (SUCCEEDED(bhr)) {
          bhr =
            blurEffect -> SetStandardDeviation(
              kDesktopBlurStandardDeviation);
        }

        // ------------------------------------------------------------
        // HARD border prevents the effect from sampling an undefined
        // transparent/extended border around the thumbnail.
        // ------------------------------------------------------------

        if (SUCCEEDED(bhr)) {
          bhr =
            blurEffect -> SetBorderMode(
              D2D1_BORDER_MODE_HARD);
        }

        // ------------------------------------------------------------
        // Apply the Gaussian blur to the entire shellContainer
        // subtree.
        // ------------------------------------------------------------

        if (SUCCEEDED(bhr)) {
          bhr =
            shellContainer -> SetEffect(
              blurEffect.Get());
        }

        blurOk =
          SUCCEEDED(bhr);
      }
    }

    // --------------------------------------------------------------------
    // If Gaussian blur creation failed, remove any partially installed
    // effect so that the desktop thumbnail still works normally.
    // --------------------------------------------------------------------

    if (!blurOk) {
      shellContainer -> SetEffect(
        nullptr);
    }

    // --------------------------------------------------------------------
    // Wash visual
    //
    // IMPORTANT:
    //
    // Do NOT attach m_washSurface here.
    //
    // The existing wash surface is opaque because it was created with
    // DXGI_ALPHA_MODE_IGNORE and cleared with alpha = 1.0.
    //
    // Attaching it would therefore simply draw a dark rectangle over the
    // blurred shell thumbnail.
    //
    // Keeping the visual empty preserves the existing composition tree
    // without covering the Gaussian-blurred desktop.
    // --------------------------------------------------------------------

    ComPtr < IDCompositionVisual2 > washVis;

    hr =
      m_dcompDevice -> CreateVisual( &
        washVis);

    if (FAILED(hr)) {
      if (mon.hShellThumb) {
        DwmUnregisterThumbnail(
          mon.hShellThumb);

        mon.hShellThumb = nullptr;
      }

      continue;
    }

    hr =
      washVis -> SetContent(
        nullptr);

    if (FAILED(hr)) {
      if (mon.hShellThumb) {
        DwmUnregisterThumbnail(
          mon.hShellThumb);

        mon.hShellThumb = nullptr;
      }

      continue;
    }

    hr =
      washVis.As( &
        mon.washVisual);

    if (FAILED(hr)) {
      if (mon.hShellThumb) {
        DwmUnregisterThumbnail(
          mon.hShellThumb);

        mon.hShellThumb = nullptr;
      }

      continue;
    }

    // --------------------------------------------------------------------
    // Insert backdrop visuals BEFORE the Flip3D scene.
    //
    // Result:
    //
    //     root
    //       |
    //       +-- blurred desktop
    //       |
    //       +-- empty wash visual
    //       |
    //       +-- sceneVisual
    //             |
    //             +-- Flip3D cards
    //
    // The cards therefore remain sharp and are rendered above the blur.
    // --------------------------------------------------------------------

    hr =
      rootBase -> AddVisual(
        mon.shellContainer.Get(),
        FALSE,
        m_sceneVisual.Get());

    if (FAILED(hr)) {
      if (mon.hShellThumb) {
        DwmUnregisterThumbnail(
          mon.hShellThumb);

        mon.hShellThumb = nullptr;
      }

      continue;
    }

    hr =
      rootBase -> AddVisual(
        mon.washVisual.Get(),
        FALSE,
        m_sceneVisual.Get());

    if (FAILED(hr)) {
      rootBase -> RemoveVisual(
        mon.shellContainer.Get());

      if (mon.hShellThumb) {
        DwmUnregisterThumbnail(
          mon.hShellThumb);

        mon.hShellThumb = nullptr;
      }

      continue;
    }

    // --------------------------------------------------------------------
    // Full opacity for the shell container.
    //
    // The Gaussian effect itself does not change the opacity of the
    // resulting image.
    // --------------------------------------------------------------------

    mon.shellContainer -> SetOpacity(
      1.0 f);

    m_monitorBackdrops.push_back(
      std::move(mon));
  }

  // ------------------------------------------------------------------------
  // Apply initial monitor geometry
  // ------------------------------------------------------------------------
  UpdateBackdropLayout();

  return true;
}

// ============================================================================
// Flip3DComp::CreateShellBackdrop
// ============================================================================
HRESULT Flip3DComp::CreateShellBackdrop() {
  if (!RebuildMonitorBackdropsIfNeeded()) {
    if (m_monitorBackdrops.empty())
      return E_FAIL;
  }

  return S_OK;
}
