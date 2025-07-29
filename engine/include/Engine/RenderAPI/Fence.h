#pragma once
#include "CommandQueue.h"
#include "Device.h"
#include "Internal/D3D12Includes.h"
#include <tbx/move_only.h>

namespace RenderAPI {

struct Fence {
    WRL::ComPtr<ID3D12Fence> pFence;
    Tbx::MovePointer<void> eventHandle;
    uint64_t fenceValue = 0;

    Fence() = default;
    DEFAULT_MOVE(Fence);
    NO_COPY(Fence);
    ~Fence();
};

Fence createFence(ID3D12Device5* pDevice);

uint64_t insertFence(Fence& fence, ID3D12CommandQueue* pCommandQueue);
bool fenceReached(const Fence&, uint64_t);
void waitForFence(const Fence& fence, uint64_t fenceValue);
void waitForIdle(Fence& fence, ID3D12CommandQueue* pCommandQueue);

}