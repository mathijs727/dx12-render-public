#pragma once
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/MaResource.h"

namespace RenderAPI {

struct SRVDesc {
    D3D12_SHADER_RESOURCE_VIEW_DESC desc;
    ID3D12Resource* pResource;
};
struct UAVDesc {
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
    ID3D12Resource* pResource;
};

struct SRVDescOwning {
    D3D12_SHADER_RESOURCE_VIEW_DESC desc;
    D3D12MAResource pResource;

    operator SRVDesc() const
    {
        return SRVDesc { .desc = desc, .pResource = pResource.Get() };
    }
};
struct UAVDescOwning {
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
    D3D12MAResource pResource;

    operator UAVDesc() const
    {
        return UAVDesc { .desc = desc, .pResource = pResource.Get() };
    }
};

template <typename T>
static RenderAPI::SRVDesc createSRVDesc(const RenderAPI::D3D12MAResource& resource, uint32_t firstElement, uint32_t numElements)
{
    // Create a binding to the mesh index & vertex buffers so we can decode materials from the hitgroup shader.
    RenderAPI::SRVDesc bufferDesc;
    bufferDesc.desc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    bufferDesc.desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    bufferDesc.desc.Buffer.FirstElement = firstElement;
    bufferDesc.desc.Buffer.NumElements = numElements;
    bufferDesc.desc.Buffer.StructureByteStride = sizeof(T);
    bufferDesc.desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    bufferDesc.pResource = resource;
    return bufferDesc;
}

}