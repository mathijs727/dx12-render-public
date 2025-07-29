#include "Engine/RenderAPI/Descriptor/GpuDescriptorLinearAllocator.h"
#include "Engine/RenderAPI/Descriptor/DescriptorBlockAllocator.h"
#include <cassert>

namespace RenderAPI {

GPUDescriptorLinearAllocator::GPUDescriptorLinearAllocator(const WRL::ComPtr<ID3D12Device5>& pDevice, DescriptorBlockAllocator* pParentCPU, DescriptorBlockAllocator* pParentGPU)
    : m_pDevice(pDevice)
    , m_pParentCPU(pParentCPU)
    , m_pParentGPU(pParentGPU)
{
    assert(pParentCPU->descriptorHeapType == pParentGPU->descriptorHeapType);
    assert(pParentCPU->descriptorsPerBlock == pParentGPU->descriptorsPerBlock);
    assert(pParentCPU->descriptorIncrementSize == pParentGPU->descriptorIncrementSize);
    assert(!pParentCPU->shaderVisible);
    assert(pParentGPU->shaderVisible);
}

GPUDescriptorLinearAllocator::~GPUDescriptorLinearAllocator()
{
    reset();
}

DescriptorAllocation GPUDescriptorLinearAllocator::allocate(uint32_t numDescriptors)
{
    assert(numDescriptors < m_pParentGPU->descriptorsPerBlock);
    const auto descriptorIncrementSize = m_pParentGPU->descriptorIncrementSize;

    if (m_uploadQueue.empty() || m_offsetInBlock + numDescriptors > m_pParentGPU->descriptorsPerBlock) {
        // Current block is full. Allocate a new block.
        m_uploadQueue.push_back(BlockPair {
            .parentAllocationCPU = m_pParentCPU->allocate(),
            .parentAllocationGPU = m_pParentGPU->allocate() });
        m_offsetInBlock = 0;
    }
    auto& block = m_uploadQueue.back();

    DescriptorAllocation out {};
    out.firstCPUDescriptor.InitOffsetted(block.parentAllocationCPU.firstCPUDescriptor, m_offsetInBlock, descriptorIncrementSize);
    out.firstGPUDescriptor.InitOffsetted(block.parentAllocationGPU.firstGPUDescriptor, m_offsetInBlock, descriptorIncrementSize);
    out.numDescriptors = numDescriptors;
    m_offsetInBlock += numDescriptors;
    return out;
}

void GPUDescriptorLinearAllocator::flush()
{
    for (const auto& block : m_uploadQueue) {
        m_pDevice->CopyDescriptorsSimple(
            m_pParentGPU->descriptorsPerBlock,
            block.parentAllocationGPU.firstCPUDescriptor,
            block.parentAllocationCPU.firstCPUDescriptor,
            m_pParentGPU->descriptorHeapType);
        m_pParentCPU->release(block.parentAllocationCPU); // CPU descriptors can be freed immediately after they have been copied to the GPU.
        m_pParentAllocationsGPU.push_back(block.parentAllocationGPU); // GPU descriptors must be kept alive until reset() is called.
    }
    m_uploadQueue.clear();
    m_offsetInBlock = 0;
}

void GPUDescriptorLinearAllocator::reset()
{
    assert(m_uploadQueue.empty()); // This is probably a bug if this is not empty.
    for (const auto& block : m_uploadQueue) {
        m_pParentCPU->release(block.parentAllocationCPU);
        m_pParentGPU->release(block.parentAllocationGPU);
    }
    m_uploadQueue.clear();
    m_offsetInBlock = 0;
    for (const auto& block : m_pParentAllocationsGPU)
        m_pParentGPU->release(block);
    m_pParentAllocationsGPU.clear();
}

}
