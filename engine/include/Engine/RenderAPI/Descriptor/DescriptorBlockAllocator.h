#pragma once
#include "../Internal/D3D12Includes.h"
#include "DescriptorAllocation.h"
#include <list>
#include <tbx/move_only.h>
#include <vector>

namespace RenderAPI {

class DescriptorBlockAllocator {
public:
    struct Block {
    public:
        CD3DX12_CPU_DESCRIPTOR_HANDLE firstCPUDescriptor;
        CD3DX12_GPU_DESCRIPTOR_HANDLE firstGPUDescriptor;

    protected:
        friend class DescriptorBlockAllocator;
        uint32_t blockIdx;
    };

public:
    const D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType;
    const bool shaderVisible;
    const uint32_t descriptorsPerBlock;
    const unsigned descriptorIncrementSize;

    WRL::ComPtr<ID3D12DescriptorHeap> pDescriptorHeap;

public:
    DescriptorBlockAllocator(const WRL::ComPtr<ID3D12Device5>& pDevice, D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, bool shaderVisible, uint32_t descriptorsPerBlock, uint32_t heapSizeInBlocks);
    NO_COPY(DescriptorBlockAllocator);
    DEFAULT_MOVE(DescriptorBlockAllocator);

    Block allocate();
    void release(const Block& descriptorBlock);

private:
    std::list<uint32_t> m_freeBlocks;
    std::vector<Block> m_blocks;
};

}