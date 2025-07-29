#pragma once
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/Internal/D3D12MAHelpers.h"
#include <tbx/move_only.h>

namespace RenderAPI {

struct DescriptorAllocation {
    CD3DX12_CPU_DESCRIPTOR_HANDLE firstCPUDescriptor;
    CD3DX12_GPU_DESCRIPTOR_HANDLE firstGPUDescriptor;
    uint32_t numDescriptors;

    D3D12MA::VirtualAllocation gpuDescriptorAlloc;

    inline DescriptorAllocation offset(uint32_t offsetInDescriptors, uint32_t descriptorIncrementSize) const
    {
        DescriptorAllocation out = *this;
        out.firstCPUDescriptor.Offset(offsetInDescriptors, descriptorIncrementSize);
        out.firstGPUDescriptor.Offset(offsetInDescriptors, descriptorIncrementSize);
        // out.numDescriptors -= offsetInDescriptors;
        return out;
    }
};

}
