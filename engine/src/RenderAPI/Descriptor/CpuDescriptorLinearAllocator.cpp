#include "Engine/RenderAPI/Descriptor/CpuDescriptorLinearAllocator.h"
#include "Engine/RenderAPI/Descriptor/DescriptorBlockAllocator.h"
#include <cassert>

namespace RenderAPI {

 CPUDescriptorLinearAllocator::CPUDescriptorLinearAllocator(DescriptorBlockAllocator* pParent)
    : m_pParent(pParent)
{
    assert(!pParent->shaderVisible);
    m_offsetInBlock = 0;
}

CPUDescriptorLinearAllocator::~CPUDescriptorLinearAllocator()
{
    reset();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CPUDescriptorLinearAllocator::allocate(uint32_t numDescriptors)
{
    assert(numDescriptors < m_pParent->descriptorsPerBlock);

    if (m_blocks.empty() || m_offsetInBlock + numDescriptors > m_pParent->descriptorsPerBlock) {
        // Current block is full. Allocate a new block.
        m_blocks.push_back(m_pParent->allocate());
        m_offsetInBlock = 0;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE out;
    out.InitOffsetted(m_blocks.back().firstCPUDescriptor, m_offsetInBlock, m_pParent->descriptorIncrementSize);
    m_offsetInBlock += numDescriptors;
    return out;
}

void CPUDescriptorLinearAllocator::reset()
{
    m_offsetInBlock = 0;
    for (const auto& block : m_blocks)
        m_pParent->release(block);
    m_blocks.clear();
}

}
