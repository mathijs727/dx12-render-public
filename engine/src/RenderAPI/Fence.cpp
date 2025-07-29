#include "Engine/RenderAPI/Fence.h"
#include "Engine/RenderAPI/Device.h"
#include <cassert>

namespace RenderAPI {

Fence::~Fence()
{
    if (eventHandle) {
        ::CloseHandle(eventHandle);
    }
}

Fence createFence(ID3D12Device5* pDevice)
{
    Fence out;
    ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&out.pFence)));
    out.eventHandle = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(out.eventHandle);
    return out;
}

bool fenceReached(const Fence& fence, uint64_t fenceValue)
{
    return (fence.pFence->GetCompletedValue() >= fenceValue);
}

uint64_t insertFence(Fence& fence, ID3D12CommandQueue* pCommandQueue)
{
    const uint64_t fenceValue = ++fence.fenceValue;
    ThrowIfFailed(pCommandQueue->Signal(fence.pFence.Get(), fenceValue));
    return fenceValue;
}

void waitForFence(const Fence& fence, uint64_t fenceValue)
{
    const auto completedValue = fence.pFence->GetCompletedValue();
    if (completedValue < fenceValue) {
        ThrowIfFailed(fence.pFence->SetEventOnCompletion(fenceValue, fence.eventHandle));
        WaitForSingleObject(fence.eventHandle, INFINITE);
    }
}

void waitForIdle(Fence& fence, ID3D12CommandQueue* pCommandQueue)
{
    waitForFence(fence, insertFence(fence, pCommandQueue));
}
}