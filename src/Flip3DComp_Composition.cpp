// ============================================================================
// Flip3DComp_Composition.cpp — DirectComposition device init, desktop wash
// ============================================================================
#include "Flip3DComp.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <vector>
#include <wincodec.h>

namespace {

struct EnumMonitorsContext
{
    std::vector<MONITORINFO> monitors;
};

BOOL CALLBACK EnumMonitorsProc(HMONITOR hMon, HDC, LPRECT, LPARAM lParam)
{
    auto* ctx = reinterpret_cast<EnumMonitorsContext*>(lParam);
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfoW(hMon, &mi))
        ctx->monitors.push_back(mi);
    return TRUE;
}

int ReadDesktopSetting(const wchar_t* name, int defaultValue)
{
    wchar_t value[32] = {};
    DWORD size = sizeof(value);
    DWORD type = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", name,
                     RRF_RT_REG_SZ, &type, value, &size) != ERROR_SUCCESS)
    {
        return defaultValue;
    }

    wchar_t* end = nullptr;
    const long parsed = std::wcstol(value, &end, 10);
    return end != value ? static_cast<int>(parsed) : defaultValue;
}

WallpaperPlacement GetWallpaperPlacement()
{
    if (ReadDesktopSetting(L"TileWallpaper", 0) != 0)
        return WallpaperPlacement::Tile;

    switch (ReadDesktopSetting(L"WallpaperStyle", 2))
    {
    case 0:  return WallpaperPlacement::Center;
    case 6:  return WallpaperPlacement::Fit;
    case 10: return WallpaperPlacement::Fill;
    case 22: return WallpaperPlacement::Span;
    case 2:
    default: return WallpaperPlacement::Stretch;
    }
}

std::vector<MONITORINFO> EnumerateMonitors()
{
    EnumMonitorsContext ctx;
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorsProc, (LPARAM)&ctx);
    return ctx.monitors;
}

struct TaskbarSearchContext
{
    HMONITOR monitor = nullptr;
    HWND taskbar = nullptr;
};

HRESULT CreateWallpaperSurface(ID3D11Device* d3d,
                               IDCompositionDesktopDevice* dcomp,
                               ComPtr<IDCompositionSurface>& outSurface,
                               UINT& outWidth,
                               UINT& outHeight)
{
    if (!d3d || !dcomp)
        return E_INVALIDARG;

    struct ComScope
    {
        HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ~ComScope()
        {
            if (result == S_OK)
                CoUninitialize();
        }
    } comScope;
    if (FAILED(comScope.result) && comScope.result != RPC_E_CHANGED_MODE)
        return comScope.result;

    wchar_t wallpaperPath[MAX_PATH] = {};
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, ARRAYSIZE(wallpaperPath),
                                wallpaperPath, 0)
        || wallpaperPath[0] == L'\0')
    {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        wallpaperPath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
        return hr;

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr))
        return hr;

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        return hr;

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0)
        return FAILED(hr) ? hr : E_FAIL;

    std::vector<BYTE> pixels(static_cast<size_t>(width) * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()),
                               pixels.data());
    if (FAILED(hr))
        return hr;

    ComPtr<IDCompositionSurfaceFactory> surfaceFactory;
    hr = dcomp->CreateSurfaceFactory(d3d, &surfaceFactory);
    if (FAILED(hr))
        return hr;

    ComPtr<IDCompositionSurface> surface;
    hr = surfaceFactory->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                                       DXGI_ALPHA_MODE_IGNORE, &surface);
    if (FAILED(hr))
        return hr;

    ComPtr<IDXGISurface> dxgiSurface;
    POINT offset = {};
    hr = surface->BeginDraw(nullptr, IID_PPV_ARGS(&dxgiSurface), &offset);
    if (FAILED(hr))
        return hr;

    ComPtr<ID3D11Texture2D> destination;
    hr = dxgiSurface.As(&destination);
    if (SUCCEEDED(hr))
    {
        ComPtr<ID3D11DeviceContext> context;
        d3d->GetImmediateContext(&context);
        context->UpdateSubresource(destination.Get(), 0, nullptr,
                                   pixels.data(), width * 4, 0);
        context->Flush();
    }

    HRESULT endDrawHr = surface->EndDraw();
    if (SUCCEEDED(hr))
        hr = endDrawHr;
    if (FAILED(hr))
        return hr;

    outSurface = std::move(surface);
    outWidth = width;
    outHeight = height;
    return S_OK;
}

} // namespace

// ============================================================================
// Flip3DCompApp::InitComposition
// ============================================================================
HRESULT Flip3DCompApp::InitComposition()
{
    if (!m_d3dDevice)
        return E_FAIL;

    ComPtr<IDXGIDevice> dxgi;
    HRESULT hr = m_d3dDevice.As(&dxgi);
    if (FAILED(hr))
        return hr;

    hr = DCompositionCreateDevice2(dxgi.Get(), IID_PPV_ARGS(&m_dcompDevice));
    if (FAILED(hr))
        return hr;

    hr = m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcompTarget);
    if (FAILED(hr))
        return hr;

    ComPtr<IDCompositionVisual2> root;
    hr = m_dcompDevice->CreateVisual(&root);
    if (FAILED(hr))
        return hr;
    m_rootVisual = root;
    m_dcompTarget->SetRoot(root.Get());

    ComPtr<IDCompositionVisual2> sceneBase;
    hr = m_dcompDevice->CreateVisual(&sceneBase);
    if (FAILED(hr))
        return hr;
    sceneBase.As(&m_sceneVisual);
    m_sceneVisual->SetDepthMode(DCOMPOSITION_DEPTH_MODE_TREE);
    //
    m_sceneVisual->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);
    m_sceneVisual->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);

    ComPtr<IDCompositionVisual> rootBase;
    root.As(&rootBase);
    rootBase->AddVisual(m_sceneVisual.Get(), FALSE, nullptr);

    return m_dcompDevice->Commit();
}

