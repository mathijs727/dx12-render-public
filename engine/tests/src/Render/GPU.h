#pragma once
#include <Engine/Render/ForwardDeclares.h>
#include <Engine/Render/RenderContext.h>
#include <Engine/RenderAPI/ForwardDeclares.h>
#include <Engine/RenderAPI/Internal/D3D12Includes.h>
#include <Engine/RenderAPI/ShaderInput.h>
#include <filesystem>

RenderAPI::PipelineState createComputePipeline(const Render::RenderContext&, const std::filesystem::path&);
void setPipelineState(const WRL::ComPtr<ID3D12GraphicsCommandList6>&, const RenderAPI::PipelineState& pipelineState);
void setDescriptorHeaps(const WRL::ComPtr<ID3D12GraphicsCommandList6>& pCommandList, const Render::RenderContext& renderContext);

template <typename T>
inline RenderAPI::UAVDescOwning createUAVBuffer(Render::RenderContext& renderContext, const WRL::ComPtr<ID3D12GraphicsCommandList6>& pCommandList, unsigned numElements)
{
    auto pResource = renderContext.createResource(
        D3D12_HEAP_TYPE_DEFAULT,
        CD3DX12_RESOURCE_DESC::Buffer(numElements * sizeof(T), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_COMMON);

    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(pResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pCommandList->ResourceBarrier(1, &barrier);

    const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer = D3D12_BUFFER_UAV {
            .FirstElement = 0,
            .NumElements = numElements,
            .StructureByteStride = sizeof(T),
            .CounterOffsetInBytes = 0,
            .Flags = D3D12_BUFFER_UAV_FLAG_NONE }
    };

    return {
        .desc = uavDesc,
        .pResource = std::move(pResource)
    };
}