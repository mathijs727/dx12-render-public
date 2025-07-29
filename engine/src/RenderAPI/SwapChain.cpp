#include "Engine/RenderAPI/SwapChain.h"
#include "Engine/RenderAPI/Device.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include <dxgidebug.h>

namespace RenderAPI {

static bool checkTearingSupport();
static UINT swapChainFlags();
static WRL::ComPtr<IDXGISwapChain4> createSwapChainD3D12(HWND hWnd, ID3D12CommandQueue* pCommandQueue, unsigned width, unsigned height, uint32_t bufferCount);
template <int N>
static std::array<WRL::ComPtr<ID3D12Resource>, N> createBackBuffers(ID3D12Device5* pDevice, IDXGISwapChain4* pSwapChain);

static constexpr DXGI_FORMAT BACKBUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

SwapChain createSwapChain(
    ID3D12Device5* pDevice,
    ID3D12CommandQueue* pCommandQueue,
    HWND hWnd,
    unsigned width, unsigned height,
    bool vsync)
{
    SwapChain out;
    out.pSwapChain = createSwapChainD3D12(hWnd, pCommandQueue, width, height, SwapChain::s_parallelFrames);
    out.backBuffers = createBackBuffers<SwapChain::s_parallelFrames>(pDevice, out.pSwapChain.Get());
    out.backBufferFormat = BACKBUFFER_FORMAT;
    out.width = width;
    out.height = height;
    out.tearingSupported = checkTearingSupport();
    out.vsyncEnabled = vsync;
    return out;
}

void resizeSwapChain(SwapChain& swapChain, ID3D12Device5* pDevice, unsigned width, unsigned height)
{
    for (auto& backBuffer : swapChain.backBuffers)
        backBuffer.Reset();

    ThrowIfFailed(swapChain.pSwapChain->ResizeBuffers(SwapChain::s_parallelFrames, width, height, swapChain.backBufferFormat, swapChainFlags()));
    swapChain.backBuffers = createBackBuffers<SwapChain::s_parallelFrames>(pDevice, swapChain.pSwapChain.Get());
    swapChain.width = width;
    swapChain.height = height;
}

void present(SwapChain& swapChain)
{
    const UINT syncInterval = swapChain.vsyncEnabled ? 1 : 0;
    const UINT presentFlags = (swapChain.tearingSupported && !swapChain.vsyncEnabled) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(swapChain.pSwapChain->Present(syncInterval, presentFlags));
}

// Check whether the device/display support rendering with tearing (v-sync off). This is also used for
// adaptive sync support (gsync/freesync).
static bool checkTearingSupport()
{
    // https://www.3dgep.com/learning-directx12-1/#Enable_the_Direct3D_12_Debug_Layer
    BOOL allowTearing = TRUE;

    WRL::ComPtr<IDXGIFactory5> pFactory;
    if (SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory)))) {
        if (FAILED(pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) {
            allowTearing = FALSE;
        }
    }

    // Conversion from TRUE to true / FALSE to Zzzzzzzzzzzzzzzzzzzzzzza false.
    return allowTearing == TRUE;
}

static UINT swapChainFlags()
{
    return checkTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
}

static WRL::ComPtr<IDXGISwapChain4> createSwapChainD3D12(
    HWND hWnd,
    ID3D12CommandQueue* pCommandQueue,
    unsigned width, unsigned height,
    uint32_t bufferCount)
{
    // https://www.3dgep.com/learning-directx12-1/#Enable_the_Direct3D_12_Debug_Layer
    WRL::ComPtr<IDXGISwapChain4> pDxgiSwapChain4;
    WRL::ComPtr<IDXGIFactory4> pDxgiFactory4;

    UINT createFactoryFlags = 0;
#if D3D12_ENABLE_VALIDATION
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&pDxgiFactory4)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = BACKBUFFER_FORMAT;
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = bufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // Allow tearing (vsync off) when available
    swapChainDesc.Flags = swapChainFlags();

    WRL::ComPtr<IDXGISwapChain1> pSwapChain1;
    ThrowIfFailed(pDxgiFactory4->CreateSwapChainForHwnd(
        pCommandQueue,
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &pSwapChain1));

    // Disable alt+enter full screen toggle feature.
    ThrowIfFailed(pDxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(pSwapChain1.As(&pDxgiSwapChain4));
    return pDxgiSwapChain4;
}

template <int N>
static std::array<WRL::ComPtr<ID3D12Resource>, N> createBackBuffers(ID3D12Device5* pDevice, IDXGISwapChain4* pSwapChain)
{
    std::array<WRL::ComPtr<ID3D12Resource>, N> backBuffers;
    for (int i = 0; i < N; i++) {
        WRL::ComPtr<ID3D12Resource> pBackBuffer;
        ThrowIfFailed(pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
        pBackBuffer->SetName(L"SwapChain BackBuffer");
        backBuffers[i] = pBackBuffer;
    }
    return backBuffers;
}

unsigned SwapChain::getCurrentBackBufferIndex() const
{
    return pSwapChain->GetCurrentBackBufferIndex();
}

ID3D12Resource* SwapChain::getCurrentBackBuffer() const
{
    return backBuffers[getCurrentBackBufferIndex()].Get();
}

}
