#pragma once
#include "Internal/D3D12Includes.h"
#include "Internal/D3D12MAHelpers.h"

namespace RenderAPI {
struct D3D12MAResource {
    WRL::ComPtr<ID3D12Resource> pResource { nullptr };
    RenderAPI::D3D12MAWrapper<D3D12MA::Allocation> pAllocation;

    D3D12MAResource() = default;
    D3D12MAResource(const WRL::ComPtr<ID3D12Resource>& pResource, D3D12MA::Allocation* pAllocation)
        : pResource(pResource)
        , pAllocation(pAllocation)
    {
    }
    D3D12MAResource(D3D12MAResource&&) = default;
    D3D12MAResource(const D3D12MAResource&) = delete;
    D3D12MAResource& operator=(D3D12MAResource&&) = default;
    D3D12MAResource& operator=(const D3D12MAResource&) = delete;

    inline ID3D12Resource* Get() const
    {
        return pResource.Get();
    }
    inline operator ID3D12Resource*() const
    {
        return pResource.Get();
    }
    inline explicit operator WRL::ComPtr<ID3D12Resource>() const
    {
        return pResource;
    }
    inline ID3D12Resource* operator->()
    {
        return pResource.Get();
    }
    inline ID3D12Resource* operator->() const
    {
        return pResource.Get();
    }
};
}
