#pragma once
#include "DescriptorAllocation.h"
#include "Engine/RenderAPI/Descriptor/DescriptorBlockAllocator.h"
#include "Engine/RenderAPI/ForwardDeclares.h"
#include "Engine/RenderAPI/Internal/D3D12MAHelpers.h"
#include <list>
#include <tbx/move_only.h>
#include <vector>

namespace RenderAPI {

class GPUDescriptorStaticAllocator {
public:
    GPUDescriptorStaticAllocator(WRL::ComPtr<ID3D12Device5> pDevice, DescriptorBlockAllocator* pParentCPU, DescriptorBlockAllocator* pParentGPU);
    NO_COPY(GPUDescriptorStaticAllocator);
    DEFAULT_MOVE(GPUDescriptorStaticAllocator);
    ~GPUDescriptorStaticAllocator();

    DescriptorAllocation allocate(uint32_t numDescriptors);

    void release(const DescriptorAllocation& alloc);

    // Upload all descriptors (that weren't flushed) before to the GPU.
    void flush();

private:
    WRL::ComPtr<ID3D12Device5> m_pDevice;
    Tbx::MovePointer<DescriptorBlockAllocator> m_pParentCPU;
    Tbx::MovePointer<DescriptorBlockAllocator> m_pParentGPU;

    // Structure-of-arrays style arguments to ID3D12Device->CopyDescriptors(...)
    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> m_uploadQueueSrcDescriptor;
    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> m_uploadQueueDstDescriptor;
    std::vector<unsigned> m_uploadQueueNumDescriptors;
    // CPU descriptors allocated using a linear allocator.
    std::vector<typename DescriptorBlockAllocator::Block> m_blocksCPU;
    uint32_t m_offsetInBlockCPU = 0;

    // Vector of blocks to sub allocate from.
    struct GPUBlock {
        typename DescriptorBlockAllocator::Block parentAllocation;
        D3D12MAWrapper<D3D12MA::VirtualBlock> maVirtualBlock; // Use AMDs memory allocator for sub allocations.
        uint32_t memoryUsed = 0; // Number of descriptors currently allocated from block. May differ from maxAllocSize due to fragmentation.
    };
    std::list<GPUBlock> m_blocksGPU;
};

}
