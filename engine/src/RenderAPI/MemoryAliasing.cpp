#include "Engine/RenderAPI/MemoryAliasing.h"
#include "Engine/Util/Align.h"
#include <tbx/disable_all_warnings.h>
#include <tbx/format/fmt_helpers.h>
#include <tbx/hashmap_helper.h>
DISABLE_WARNINGS_PUSH()
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()

namespace RenderAPI {

ResourceAliasManager::ResourceAliasManager(
    const WRL::ComPtr<ID3D12Device5>& pDevice,
    D3D12MA::Allocator* pParentAllocator,
    size_t totalSizeInBytes,
    size_t numFrames)
    : m_pDevice(pDevice)
    , m_pParentAllocator(pParentAllocator)
{
    const auto createAllocator = [&](D3D12_HEAP_FLAGS heapFlags, size_t size) {
        D3D12MA::ALLOCATION_DESC allocDesc {};
        allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
        allocDesc.ExtraHeapFlags = heapFlags;
        D3D12_RESOURCE_ALLOCATION_INFO allocInfo {};
        allocInfo.SizeInBytes = size;
        allocInfo.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        D3D12MA::Allocation* pAllocation;
        ThrowIfFailed(m_pParentAllocator->AllocateMemory(
            &allocDesc,
            &allocInfo,
            &pAllocation));

        D3D12MA::VIRTUAL_BLOCK_DESC blockDesc {};
        blockDesc.Size = size;
        D3D12MA::VirtualBlock* pVirtualBlock;
        ThrowIfFailed(CreateVirtualBlock(&blockDesc, &pVirtualBlock));

        return Allocator {
            .pBaseAllocation = pAllocation,
            .pVirtualBlock = pVirtualBlock
        };
    };

    D3D12_FEATURE_DATA_D3D12_OPTIONS options {};
    ThrowIfFailed(pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));
    if (options.ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_1) {
        // On tier 1 devices we need to create separate heaps for each resource type.
        // We perform 3 allocations and D3D12MA will make sure that they come from different heaps.
        const size_t heapSizeInBytes = Util::roundUpToClosestMultiple(totalSizeInBytes / 3, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
        m_allocators.emplace_back(createAllocator(D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, heapSizeInBytes));
        m_allocators.emplace_back(createAllocator(D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES, heapSizeInBytes));
        m_allocators.emplace_back(createAllocator(D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS, heapSizeInBytes));
    } else {
        // On tier 2 hardware we can allocate all resources from a single heap type.
        m_allocators.emplace_back(createAllocator(D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, totalSizeInBytes));
    }
}

AliasingResource ResourceAliasManager::allocate(const D3D12_RESOURCE_DESC& resourceDesc, D3D12_RESOURCE_STATES initialState, D3D12_CLEAR_VALUE* pOptimizedClearValue)
{
    size_t allocatorIdx = 0;
    if (m_allocators.size() > 1) {
        switch (resourceDesc.Dimension) {
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
            if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 || (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
                allocatorIdx = RenderTargetAllocatorIdx;
            else
                allocatorIdx = TextureAllocatorIdx;
        } break;
        case D3D12_RESOURCE_DIMENSION_BUFFER: {
            allocatorIdx = BufferAllocatorIdx;
        } break;
        default: {
            spdlog::error("Cannot create aliased resource with dimension {}", Tbx::to_printable_value(resourceDesc.Dimension));
        } break;
        };
    }

    auto& allocator = m_allocators[allocatorIdx];
    const auto allocationInfo = m_pDevice->GetResourceAllocationInfo(0, 1, &resourceDesc);
    const D3D12MA::VIRTUAL_ALLOCATION_DESC virtualAllocDesc {
        .Size = allocationInfo.SizeInBytes,
        .Alignment = allocationInfo.Alignment,
        //.pUserData = nullptr
    };
    D3D12MA::VirtualAllocation virtualAllocation;
    size_t offset;
    ThrowIfFailed(allocator.pVirtualBlock->Allocate(&virtualAllocDesc, &virtualAllocation, &offset));

    WRL::ComPtr<ID3D12Resource> pResource;
    m_pParentAllocator->CreateAliasingResource(allocator.pBaseAllocation, offset, &resourceDesc, initialState, nullptr, IID_PPV_ARGS(&pResource));
    pResource->SetName(L"AliasingResource");
    return { .pResource = pResource, .allocatorIdx = allocatorIdx, .alloc = virtualAllocation };
}

void ResourceAliasManager::releaseMemory(const AliasingResource& resource)
{
    auto& allocator = m_allocators[resource.allocatorIdx];
    allocator.pVirtualBlock->FreeAllocation(resource.alloc);
}

}
