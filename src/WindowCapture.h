#pragma once

// ============================================================================
// WindowCapture — captures a single HWND's content via Windows.Graphics.Capture
// into an ID3D11ShaderResourceView, for use as a real GPU-rasterized, MSAA'd
// textured quad instead of a raw DirectComposition thumbnail visual.
//
// Ported from ALTaleX531/flip3d's WindowCapture, adapted to reuse
// flip3d_comp's own already-loaded DWM ordinal function pointers
// (m_pfnCreateSharedThumbVisual / m_pfnQueryThumbSize) instead of loading a
// second, separate copy — avoids redefining the private DWM_TNP_* constants
// that Config.h already declares.
// ============================================================================
#include "Config.h"

// WinRT interop headers for Windows.Graphics.Capture + Composition
#include <roapi.h>
#include <winstring.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <windows.ui.composition.h>          // ABI::Windows::UI::Composition::IVisual
#include <windows.ui.composition.interop.h>  // ICompositorInterop

class WindowCapture
{
public:
    WindowCapture() = default;
    ~WindowCapture() { Release(); }

    WindowCapture(WindowCapture&& other) noexcept;
    WindowCapture& operator=(WindowCapture&& other) noexcept;

    WindowCapture(const WindowCapture&) = delete;
    WindowCapture& operator=(const WindowCapture&) = delete;

    // hwndCapture: window whose content to capture.
    // hwndDestination: our own top-level window (needed by the DWM thumbnail API).
    // device: the app's own D3D11 device (the captured texture lives on this device).
    // pfnCreateThumb/pfnQuerySize: flip3d_comp's already-loaded DWM ordinal function
    // pointers (m_pfnCreateSharedThumbVisual / m_pfnQueryThumbSize) — reused here
    // purely as a bridge to obtain a Windows.Graphics.Capture item, not to show
    // a visible DirectComposition thumbnail.
    HRESULT Initialize(HWND hwndCapture, HWND hwndDestination, ID3D11Device* device,
                        DwmpCreateSharedThumbnailVisual_fn pfnCreateThumb,
                        DwmpQueryWindowThumbnailSourceSize_fn pfnQuerySize);
    void Release();

    bool IsValid() const { return m_srv != nullptr; }
    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
    UINT GetWidth()  const { return m_width; }
    UINT GetHeight() const { return m_height; }

    // Poll for a new captured frame (call from render thread, once per frame).
    void PollFrame();

private:
    HRESULT InitViaThumbnail(HWND hwndCapture, HWND hwndDestination,
                              DwmpCreateSharedThumbnailVisual_fn pfnCreateThumb,
                              DwmpQueryWindowThumbnailSourceSize_fn pfnQuerySize);

    HRESULT CreateTextureAndSRV(UINT width, UINT height);
    HRESULT StartWGCSession(
        ABI::Windows::Graphics::Capture::IGraphicsCaptureItem* captureItem,
        UINT width, UINT height);

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<ID3D11Texture2D> m_captureTexture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    UINT m_width  = 0;
    UINT m_height = 0;

    // ABI:: (WinRT interop) capture objects
    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem> m_captureItem;
    ComPtr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool> m_framePool;
    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession> m_session;

    // DWM thumbnail visual (only used as a bridge to CreateGraphicsCaptureItemFromVisual)
    ComPtr<IDCompositionVisual> m_thumbVisual;
    HTHUMBNAIL m_hThumbnail = nullptr;

    // Small, separate InteropCompositor device — lazily initialized once,
    // shared by all WindowCapture instances. Deliberately NOT the app's own
    // m_dcompDevice; this one exists only to bridge a DComp visual into WGC.
    static ComPtr<IDCompositionDesktopDevice> s_dcompDevice;
};
