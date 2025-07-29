#pragma once
#include "DescriptorAllocation.h"
#include "Engine/RenderAPI/Descriptor/DescriptorBlockAllocator.h"
#include "Engine/RenderAPI/ForwardDeclares.h"
#include <deque>
#include <tbx/move_only.h>

namespace RenderAPI {

class GPUDescriptorLinearAllocator {
public:
    GPUDescriptorLinearAllocator(const WRL::ComPtr<ID3D12Device5>& pDevice, DescriptorBlockAllocator* pParentCPU, DescriptorBlockAllocator* pParentGPU);
    NO_COPY(GPUDescriptorLinearAllocator);
    DEFAULT_MOVE(GPUDescriptorLinearAllocator);
    ~GPUDescriptorLinearAllocator();

    DescriptorAllocation allocate(uint32_t numDescriptors);

    // Upload all descriptors (that weren't flushed) before to the GPU.
    void flush();
    // Release all CPU and GPU descriptors.
    void reset();

private:
    WRL::ComPtr<ID3D12Device5> m_pDevice;
    DescriptorBlockAllocator* m_pParentCPU;
    DescriptorBlockAllocator* m_pParentGPU;

    uint32_t m_offsetInBlock = 0;
    struct BlockPair {
        uint32_t numFlushedDescriptors = 0; // Number of descriptors (from start of block) that have been flushed.
        typename DescriptorBlockAllocator::Block parentAllocationCPU;
        typename DescriptorBlockAllocator::Block parentAllocationGPU;
    };
    std::deque<BlockPair> m_uploadQueue;

    // When we call flush() we immediately release the CPU blocks to the parent.
    // The GPU blocks must be tracked seprately so that they can stay alive and be freed when reset() is called.
    std::vector<typename DescriptorBlockAllocator::Block> m_pParentAllocationsGPU;
};

}
