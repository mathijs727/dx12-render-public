#pragma once
#include "ForwardDeclares.h"
#include "Internal/D3D12Includes.h"
#include <array>
#include <tbx/move_only.h>

namespace RenderAPI {

struct SwapChain {
public:
    static constexpr int s_parallelFrames = 2;

    WRL::ComPtr<IDXGISwapChain4> pSwapChain;
    std::array<WRL::ComPtr<ID3D12Resource>, s_parallelFrames> backBuffers;
    DXGI_FORMAT backBufferFormat;

    unsigned width, height;
    bool tearingSupported;
    bool vsyncEnabled;

    std::array<uint64_t, s_parallelFrames> frameFenceValues {};

    SwapChain() = default;
    NO_COPY(SwapChain);
    DEFAULT_MOVE(SwapChain);

public:
    unsigned getCurrentBackBufferIndex() const;
    ID3D12Resource* getCurrentBackBuffer() const;
};

SwapChain createSwapChain(
    ID3D12Device5* pDevice,
    ID3D12CommandQueue* pCommandQueue,
    HWND hWnd,
    unsigned width, unsigned height,
    bool vsync);
void resizeSwapChain(SwapChain&, ID3D12Device5* pDevice, unsigned, unsigned);
void present(SwapChain&);

}