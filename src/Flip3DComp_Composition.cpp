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

} // namespace

// ============================================================================
// Flip3DComp::InitComposition
// ============================================================================
HRESULT Flip3DComp::InitComposition()
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
    //m_sceneVisual->SetDepthMode(DCOMPOSITION_DEPTH_MODE_TREE);
    m_sceneVisual->SetDepthMode(DCOMPOSITION_DEPTH_MODE_SORTED);
    //
    ComPtr<IDCompositionVisual> rootBase;
    root.As(&rootBase);
    rootBase->AddVisual(m_sceneVisual.Get(), FALSE, nullptr);

    return m_dcompDevice->Commit();
}

