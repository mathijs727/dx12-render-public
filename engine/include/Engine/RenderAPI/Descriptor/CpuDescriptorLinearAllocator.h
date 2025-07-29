#pragma once
#include "DescriptorAllocation.h"
#include "Engine/RenderAPI/Descriptor/DescriptorBlockAllocator.h"
#include "Engine/RenderAPI/ForwardDeclares.h"
#include <tbx/move_only.h>
#include <vector>

namespace RenderAPI {

class CPUDescriptorLinearAllocator {
public:
    CPUDescriptorLinearAllocator(DescriptorBlockAllocator* pParent);
    NO_COPY(CPUDescriptorLinearAllocator);
    DEFAULT_MOVE(CPUDescriptorLinearAllocator);
    ~CPUDescriptorLinearAllocator();

    CD3DX12_CPU_DESCRIPTOR_HANDLE allocate(uint32_t numDescriptors);
    void reset();

private:
    DescriptorBlockAllocator* m_pParent;

    uint32_t m_offsetInBlock = 0;
    std::vector<typename DescriptorBlockAllocator::Block> m_blocks;
};

}
