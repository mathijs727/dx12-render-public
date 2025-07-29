#include "Engine/RenderAPI/Descriptor/GpuDescriptorStaticAllocator.h"
#include "Engine/RenderAPI/Descriptor/DescriptorBlockAllocator.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <cassert>

namespace RenderAPI {

GPUDescriptorStaticAllocator::GPUDescriptorStaticAllocator(WRL::ComPtr<ID3D12Device5> pDevice, DescriptorBlockAllocator* pParentCPU, DescriptorBlockAllocator* pParentGPU)
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

GPUDescriptorStaticAllocator::~GPUDescriptorStaticAllocator()
{
    if (m_pParentCPU) {
        for (const auto& block : m_blocksCPU)
            m_pParentCPU->release(block);
    }
    if (m_pParentGPU) {
        for (const auto& block : m_blocksGPU)
            m_pParentGPU->release(block.parentAllocation);
    }
}

DescriptorAllocation GPUDescriptorStaticAllocator::allocate(uint32_t numDescriptors)
{
    assert(numDescriptors < m_pParentCPU->descriptorsPerBlock);
    const auto descriptorIncrementSize = m_pParentGPU->descriptorIncrementSize;

    DescriptorAllocation out {
        .numDescriptors = numDescriptors
    };
    // Allocate descriptor in CPU memory.
    if (m_blocksCPU.empty() || m_offsetInBlockCPU + numDescriptors > m_pParentCPU->descriptorsPerBlock) {
        // Current block is full. Allocate a new block.
        m_blocksCPU.push_back(m_pParentCPU->allocate());
        m_offsetInBlockCPU = 0;
    }
    auto& cpuBlock = m_blocksCPU.back();
    out.firstCPUDescriptor.InitOffsetted(
        cpuBlock.firstCPUDescriptor, m_offsetInBlockCPU, descriptorIncrementSize);
    m_offsetInBlockCPU += numDescriptors;

    // Try allocate descriptor in GPU memory from the provided block.
    // This may fail when the block is full.
    auto tryAllocFromBlockGPU = [this, &out, numDescriptors, descriptorIncrementSize](GPUBlock& block) -> bool {
        const D3D12MA::VIRTUAL_ALLOCATION_DESC allocDesc { .Size = numDescriptors, .Alignment = 1 };
        uint64_t offset;
        if (block.maVirtualBlock->Allocate(&allocDesc, &out.gpuDescriptorAlloc, &offset) == S_OK) {
            out.firstGPUDescriptor.InitOffsetted(block.parentAllocation.firstGPUDescriptor, (INT)offset, descriptorIncrementSize);
            block.memoryUsed += numDescriptors;
            assert(offset + numDescriptors <= m_pParentGPU->descriptorsPerBlock);

            // Store everything we need to copy the descriptors from CPU heap to GPU heap when flush() is called.
            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorGpuMemory;
            cpuDescriptorGpuMemory.InitOffsetted(block.parentAllocation.firstCPUDescriptor, (INT)offset, descriptorIncrementSize);
            m_uploadQueueSrcDescriptor.push_back(out.firstCPUDescriptor);
            m_uploadQueueDstDescriptor.push_back(cpuDescriptorGpuMemory);
            m_uploadQueueNumDescriptors.push_back(numDescriptors);
            return true;
        } else {
            return false;
        }
    };
    // Try to allocate from all available blocks.
    bool gpuAllocated = false;
    for (auto& blockGPU : m_blocksGPU) {
        if (tryAllocFromBlockGPU(blockGPU)) {
            gpuAllocated = true;
            break;
        }
    }
    // No space in the current blocks, allocate a new one.
    if (!gpuAllocated) {
        // Allocate a new block.
        GPUBlock blockGPU {
            .parentAllocation = m_pParentGPU->allocate()
        };
        const D3D12MA::VIRTUAL_BLOCK_DESC blockDesc { .Size = m_pParentGPU->descriptorsPerBlock };
        D3D12MA::VirtualBlock* pMABlock;
        ThrowIfFailed(CreateVirtualBlock(&blockDesc, &pMABlock));
        blockGPU.maVirtualBlock = pMABlock;

        // Perform sub allocation from this new block.
        [[maybe_unused]] bool success = tryAllocFromBlockGPU(blockGPU);
        assert(success);

        // Store the block so we can make more allocations from it.
        m_blocksGPU.push_front(std::move(blockGPU));
    }

    return out;
}

void GPUDescriptorStaticAllocator::release(const DescriptorAllocation& alloc)
{
    const auto descriptorIncrementSize = m_pParentGPU->descriptorIncrementSize;
    const auto blockSizeInBytes = m_pParentGPU->descriptorsPerBlock * descriptorIncrementSize;
    for (auto iter = std::begin(m_blocksGPU); iter != std::end(m_blocksGPU); ++iter) {
        auto& block = *iter;
        if (alloc.firstGPUDescriptor.ptr >= block.parentAllocation.firstGPUDescriptor.ptr && alloc.firstGPUDescriptor.ptr < block.parentAllocation.firstGPUDescriptor.ptr + blockSizeInBytes) {
            const uint64_t offsetInBytes = alloc.firstGPUDescriptor.ptr - block.parentAllocation.firstGPUDescriptor.ptr;
            assert(offsetInBytes % descriptorIncrementSize == 0);
            const uint64_t offsetInDescriptors = offsetInBytes / descriptorIncrementSize;
            block.maVirtualBlock->FreeAllocation(alloc.gpuDescriptorAlloc);
            block.memoryUsed -= alloc.numDescriptors;

            // When the free causes the block the become entirely empty, then we return the block to the parent allocator.
            if (block.memoryUsed == 0) {
                m_pParentGPU->release(block.parentAllocation);
            }
            return;
        }
    }
    assert(false); // Could not find the allocation.
}

void GPUDescriptorStaticAllocator::flush()
{
    assert(m_uploadQueueSrcDescriptor.size() == m_uploadQueueDstDescriptor.size());
    assert(m_uploadQueueNumDescriptors.size() == m_uploadQueueDstDescriptor.size());

    m_pDevice->CopyDescriptors(
        (UINT)m_uploadQueueDstDescriptor.size(),
        m_uploadQueueDstDescriptor.data(),
        m_uploadQueueNumDescriptors.data(),
        (UINT)m_uploadQueueSrcDescriptor.size(),
        m_uploadQueueSrcDescriptor.data(),
        m_uploadQueueNumDescriptors.data(),
        m_pParentGPU->descriptorHeapType);

    m_uploadQueueSrcDescriptor.clear();
    m_uploadQueueDstDescriptor.clear();
    m_uploadQueueNumDescriptors.clear();

    // CPU descriptors can be freed immediately after they have been copied to the GPU.
    for (const auto& block : m_blocksCPU)
        m_pParentCPU->release(block);
    m_blocksCPU.clear();
}
}
