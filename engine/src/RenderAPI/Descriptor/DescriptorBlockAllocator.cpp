#include "Engine/RenderAPI/Descriptor/DescriptorBlockAllocator.h"
#include <cassert>

namespace RenderAPI {

DescriptorBlockAllocator::DescriptorBlockAllocator(const WRL::ComPtr<ID3D12Device5>& pDevice, D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, bool shaderVisible, uint32_t descriptorsPerBlock, uint32_t heapSizeInBlocks)
    : descriptorHeapType(descriptorHeapType)
    , shaderVisible(shaderVisible)
    , descriptorsPerBlock(descriptorsPerBlock)
    , descriptorIncrementSize(pDevice->GetDescriptorHandleIncrementSize(descriptorHeapType))
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = descriptorsPerBlock * heapSizeInBlocks;
    desc.Type = descriptorHeapType;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pDescriptorHeap)));
    pDescriptorHeap->SetName(L"DescriptorBlockAllocator");

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptor { pDescriptorHeap->GetCPUDescriptorHandleForHeapStart() };
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptor { shaderVisible ? pDescriptorHeap->GetGPUDescriptorHandleForHeapStart() : CD3DX12_GPU_DESCRIPTOR_HANDLE {} };
    for (int i = 0; i < (int)heapSizeInBlocks; i++) {
        Block block {};
        block.firstCPUDescriptor = cpuDescriptor;
        block.firstGPUDescriptor = gpuDescriptor;
        block.blockIdx = (uint32_t)m_blocks.size();
        m_freeBlocks.push_back(block.blockIdx);
        m_blocks.push_back(block);

        cpuDescriptor.Offset(descriptorsPerBlock, descriptorIncrementSize);
        gpuDescriptor.Offset(descriptorsPerBlock, descriptorIncrementSize);
    }
}

DescriptorBlockAllocator::Block DescriptorBlockAllocator::allocate()
{
    assert(!m_freeBlocks.empty());
    auto blockIdx = m_freeBlocks.front();
    m_freeBlocks.pop_front();
    return m_blocks[blockIdx];
}

void DescriptorBlockAllocator::release(const Block& descriptorBlock)
{
    assert(descriptorBlock.blockIdx < m_blocks.size());
    m_freeBlocks.push_front(descriptorBlock.blockIdx);
}

}
