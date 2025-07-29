#pragma once
#pragma once
#include "Internal/D3D12Includes.h"
#include "Internal/D3D12MAHelpers.h"
#include <vector>

namespace RenderAPI {

struct AliasingResource {
    WRL::ComPtr<ID3D12Resource> pResource;
    size_t allocatorIdx;
    D3D12MA::VirtualAllocation alloc;

    inline operator ID3D12Resource*() const
    {
        return pResource.Get();
    }
    inline operator WRL::ComPtr<ID3D12Resource>() const
    {
        return pResource;
    }
    inline ID3D12Resource* operator->()
    {
        return pResource.Get();
    }
    inline const ID3D12Resource* operator->() const
    {
        return pResource.Get();
    }
};

class ResourceAliasManager {
public:
    ResourceAliasManager(const WRL::ComPtr<ID3D12Device5>& pDevice, D3D12MA::Allocator* pParentAllocator, size_t size, size_t numFrames);
    
    AliasingResource allocate(const D3D12_RESOURCE_DESC& resourceDesc, D3D12_RESOURCE_STATES initialState, D3D12_CLEAR_VALUE* pOptimizedClearValue = nullptr);
    void releaseMemory(const AliasingResource& resource);

private:
    WRL::ComPtr<ID3D12Device5> m_pDevice;
    D3D12MA::Allocator* m_pParentAllocator;

    // Owning pointers to allocators which automatically free the memory when the ResourceAliasManager is destroyed.
    struct Allocator {
        D3D12MAWrapper<D3D12MA::Allocation> pBaseAllocation;
        D3D12MAWrapper<D3D12MA::VirtualBlock> pVirtualBlock;
    };
    std::vector<Allocator> m_allocators;

    static constexpr size_t RenderTargetAllocatorIdx = 0;
    static constexpr size_t TextureAllocatorIdx = 1;
    static constexpr size_t BufferAllocatorIdx = 2;
};

}
