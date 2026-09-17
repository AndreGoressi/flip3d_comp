// ============================================================================
// Flip3DComp_Composition.cpp — DirectComposition device init, desktop wash
// ============================================================================
#include "Flip3DComp.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <vector>

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

std::vector<MONITORINFO> EnumerateMonitors()
{
    EnumMonitorsContext ctx;
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorsProc, (LPARAM)&ctx);
    return ctx.monitors;
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
    //
    m_sceneVisual->SetDepthMode(DCOMPOSITION_DEPTH_MODE_TREE);
    //
    ComPtr<IDCompositionVisual> rootBase;
    root.As(&rootBase);
    rootBase->AddVisual(m_sceneVisual.Get(), FALSE, nullptr);

    return m_dcompDevice->Commit();
}

// ============================================================================
// Flip3DComp::DestroyMonitorBackdrops (Clean Modern Win11 Version)
// ============================================================================
void Flip3DComp::DestroyMonitorBackdrops()
{
    if (!m_rootVisual)
    {
        m_monitorBackdrops.clear();
        return;
    }
    ComPtr<IDCompositionVisual> rootBase;
    if (SUCCEEDED(m_rootVisual.As(&rootBase)))
    {
        for (auto& mon : m_monitorBackdrops)
        {
            if (mon.shellContainer)
                rootBase->RemoveVisual(mon.shellContainer.Get());

            if (mon.hShellThumb)
            {
                DwmUnregisterThumbnail(mon.hShellThumb);
                mon.hShellThumb = nullptr;
            }
        }
    }
    m_monitorBackdrops.clear();
}

// ============================================================================
// Flip3DComp::UpdateBackdropLayout (Clean Modern Win11 Version)
// ============================================================================
void Flip3DComp::UpdateBackdropLayout()
{
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);

    HWND shell = GetShellWindow();
    RECT shellWnd = {};
    if (shell)
        GetWindowRect(shell, &shellWnd);

    for (auto& mon : m_monitorBackdrops)
    {
        const LONG shellW = mon.rcWork.right - mon.rcWork.left;
        const LONG shellH = mon.rcWork.bottom - mon.rcWork.top;
        const float shellX = (float)(mon.rcWork.left - vx);
        const float shellY = (float)(mon.rcWork.top  - vy);
        
        if (mon.shellContainer)
        {
            ComPtr<IDCompositionVisual2> shell2;
            if (SUCCEEDED(mon.shellContainer.As(&shell2)))
            {
                const D2D_MATRIX_3X2_F xform = {
                    1.f, 0.f,
                    0.f, 1.f,
                    shellX, shellY,
                };
                shell2->SetTransform(xform);
            }
        }
        if (mon.hShellThumb && shellW > 0 && shellH > 0 && m_pfnUpdateSharedMultiWindowVisual)
        {
            RECT rcSource = mon.rcWork;
            SIZE targetSize = { shellW, shellH };
            HWND excludeHwnd = m_hwnd; 

            m_pfnUpdateSharedMultiWindowVisual(
                mon.hShellThumb,
                nullptr, 0,          // No Includes
                &excludeHwnd, 1,     // Exclude Flip3D
                &rcSource,
                &targetSize,
                1                    // Flag
            );
        }
    }
    if (m_dcompDevice)
        m_dcompDevice->Commit();
}

// ============================================================================
// Flip3DComp::RebuildMonitorBackdropsIfNeeded (Clean Modern Win11 Engine)
// ============================================================================
bool Flip3DComp::RebuildMonitorBackdropsIfNeeded()
{
    const std::vector<MONITORINFO> monitors = EnumerateMonitors();
    if (monitors.empty())
        return false;

    bool layoutSame = monitors.size() == m_monitorBackdrops.size();
    if (layoutSame)
    {
        for (size_t i = 0; i < monitors.size(); ++i)
        {
            const MONITORINFO& a = monitors[i];
            const MonitorBackdrop& b = m_monitorBackdrops[i];
            if (a.rcMonitor.left   != b.rcMonitor.left
             || a.rcMonitor.top    != b.rcMonitor.top
             || a.rcMonitor.right  != b.rcMonitor.right
             || a.rcMonitor.bottom != b.rcMonitor.bottom
             || a.rcWork.left      != b.rcWork.left
             || a.rcWork.top       != b.rcWork.top
             || a.rcWork.right     != b.rcWork.right
             || a.rcWork.bottom    != b.rcWork.bottom)
            {
                layoutSame = false;
                break;
            }
        }
    }

    if (layoutSame)
    {
        UpdateBackdropLayout();
        return false;
    }

    DestroyMonitorBackdrops();

    if (!m_dcompDevice || !m_rootVisual)
        return false;

    ComPtr<IDCompositionVisual> rootBase;
    if (FAILED(m_rootVisual.As(&rootBase)))
        return false;

    for (const MONITORINFO& mi : monitors)
    {
        MonitorBackdrop mon = {};
        mon.rcMonitor = mi.rcMonitor;
        mon.rcWork    = mi.rcWork;

        const LONG shellW = mon.rcWork.right - mon.rcWork.left;
        const LONG shellH = mon.rcWork.bottom - mon.rcWork.top;
        if (shellW <= 0 || shellH <= 0)
            continue;

        void* pv = nullptr;
        HRESULT hr = m_pfnCreateSharedMultiWindowVisual(
            m_hwnd,
            m_dcompDevice.Get(),
            &pv,
            &mon.hShellThumb);

        if (FAILED(hr) || !pv)
            continue;

        ComPtr<IDCompositionVisual> thumbBase;
        thumbBase.Attach((IDCompositionVisual*)pv);
        hr = thumbBase.As(&mon.shellThumb);
        if (FAILED(hr))
            continue;

        RECT rcSource = mon.rcWork;
        SIZE targetSize = { shellW, shellH };
        HWND excludeHwnd = m_hwnd; 

        if (m_pfnUpdateSharedMultiWindowVisual && mon.hShellThumb)
        {
            m_pfnUpdateSharedMultiWindowVisual(
                mon.hShellThumb,
                nullptr, 0,
                &excludeHwnd, 1,
                &rcSource,
                &targetSize,
                1
            );
        }
        ComPtr<IDCompositionVisual2> shellContainer;
        hr = m_dcompDevice->CreateVisual(&shellContainer);
        if (FAILED(hr))
            continue;

        hr = shellContainer.As(&mon.shellContainer);
        if (FAILED(hr))
            continue;

        shellContainer->AddVisual(thumbBase.Get(), FALSE, nullptr);
        ComPtr<IDCompositionDevice3> dcompDevice3;
        if (SUCCEEDED(m_dcompDevice.As(&dcompDevice3)))
        {
            ComPtr<IDCompositionGaussianBlurEffect> blurEffect;
            if (SUCCEEDED(dcompDevice3->CreateGaussianBlurEffect(&blurEffect)))
            {
                blurEffect->SetStandardDeviation(50.0f);
                blurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
                shellContainer->SetEffect(blurEffect.Get());
            }
        }
        rootBase->AddVisual(mon.shellContainer.Get(), FALSE, m_sceneVisual.Get());
        mon.shellContainer->SetOpacity(1.0f);
        m_monitorBackdrops.push_back(std::move(mon));
    }
    UpdateBackdropLayout();
    return true;
}

