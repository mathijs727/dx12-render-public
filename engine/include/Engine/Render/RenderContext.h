#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include "Engine/RenderAPI/Buffer/CpuBufferLinearAllocator.h"
#include "Engine/RenderAPI/Buffer/CpuBufferRingAllocator.h"
#include "Engine/RenderAPI/Descriptor/CpuDescriptorLinearAllocator.h"
#include "Engine/RenderAPI/Descriptor/DescriptorBlockAllocator.h"
#include "Engine/RenderAPI/Descriptor/GpuDescriptorLinearAllocator.h"
#include "Engine/RenderAPI/Descriptor/GpuDescriptorStaticAllocator.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include <Engine/Util/IsOfType.h>
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <dxgidebug.h>
#include <glm/vec2.hpp>
DISABLE_WARNINGS_POP()
#include <array>
#include <cstring> // std::memcpy
#include <memory>
#include <optional>
#include <span>

namespace Render {

struct ErrorReporter {
    inline ~ErrorReporter()
    {

        WRL::ComPtr<IDXGIDebug1> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
};

struct RenderContext {
    // Almost impossible to not "leak" when storing RootSignatures & PipelineStates as static variables
    // Also: FrameGraph keeps a static list of resources alive. Probably should change that at some point
    // in the future though.
    // ErrorReporter errorReporter;
    WRL::ComPtr<IDXGIAdapter4> pAdapter;
    WRL::ComPtr<ID3D12Device5> pDevice;
    RenderAPI::D3D12MAWrapper<D3D12MA::Allocator> pResourceAllocator;

    WRL::ComPtr<ID3D12CommandQueue> pGraphicsQueue;
    RenderAPI::Fence graphicsFence;
    std::optional<RenderAPI::SwapChain> optSwapChain;
    std::array<uint64_t, RenderAPI::SwapChain::s_parallelFrames> frameFenceValues;
    uint32_t backBufferIndex = 0;

    RenderAPI::CommandListManager commandListManager;

    std::unique_ptr<RenderAPI::DescriptorBlockAllocator> pCbvSrvUavDescriptorBaseAllocatorCPU;
    std::unique_ptr<RenderAPI::DescriptorBlockAllocator> pCbvSrvUavDescriptorBaseAllocatorGPU;
    std::vector<RenderAPI::GPUDescriptorLinearAllocator> cbvSrvUavDescriptorTransientAllocators;
    std::unique_ptr<RenderAPI::DescriptorBlockAllocator> pRtvDescriptorBaseAllocatorCPU;
    std::unique_ptr<RenderAPI::DescriptorBlockAllocator> pDsvDescriptorBaseAllocatorCPU;

    RenderAPI::GPUDescriptorStaticAllocator cbvSrvUavDescriptorStaticAllocator;
    RenderAPI::GPUDescriptorLinearAllocator& getCurrentCbvSrvUavDescriptorTransientAllocator();
    RenderAPI::CPUDescriptorLinearAllocator rtvDescriptorAllocator;
    RenderAPI::CPUDescriptorLinearAllocator dsvDescriptorAllocator;

    std::optional<RenderAPI::DescriptorAllocation> optImGuiDescriptorAllocation;

    // Allocator for transient CPU visible data such as per-frame ConstantBuffers.
    RenderAPI::CPUBufferRingAllocator singleFrameBufferAllocator;

public:
    RenderContext(); // Headless mode
    RenderContext(const Core::Window& window, bool imgui = true); // From window
    ~RenderContext();

    RenderAPI::ResourceAliasManager createResourceAliasManager(size_t size);

    RenderAPI::D3D12MAResource createResource(D3D12_HEAP_TYPE heapType, const D3D12_RESOURCE_DESC& resourceDesc, D3D12_RESOURCE_STATES initialState);
    template <typename T>
    RenderAPI::D3D12MAResource createBufferWithData(const T& data, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES initialState, size_t alignmentPadding = 0);
    template <typename T>
    RenderAPI::D3D12MAResource createBufferWithArrayData(std::span<const T> data, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES initialState);
    template <typename T>
    RenderAPI::SRVDescOwning createBufferSRVWithArrayData(std::span<const T> data, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES initialState);
    template <typename T>
    void copyBufferFromGPUToCPU(const RenderAPI::D3D12MAResource& buffer, D3D12_RESOURCE_STATES resourceState, std::span<T> out);

    void resizeSwapChain(const glm::uvec2& resolution);

    void submitGraphicsQueue(const WRL::ComPtr<ID3D12GraphicsCommandList6>& pCommandList);

    void waitForNextFrame() const;
    void waitForIdle();
    void resetFrameAllocators();
    void present();
};

template <typename T>
void RenderContext::copyBufferFromGPUToCPU(const RenderAPI::D3D12MAResource& buffer, D3D12_RESOURCE_STATES resourceState, std::span<T> out)
{
    const auto bufferSize = out.size_bytes();
    auto pReadbackBuffer = createResource(
        D3D12_HEAP_TYPE_READBACK,
        CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE),
        D3D12_RESOURCE_STATE_COPY_DEST);

    // Copy from the default heap to the upload heap.
    auto pCommandList = commandListManager.acquireCommandList();
    if (resourceState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        const auto toCopySourceBarier = CD3DX12_RESOURCE_BARRIER::Transition(buffer, resourceState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        pCommandList->ResourceBarrier(1, &toCopySourceBarier);
    }
    pCommandList->CopyBufferRegion(
        pReadbackBuffer.pResource.Get(),
        0,
        buffer.pResource.Get(),
        0,
        bufferSize);
    if (resourceState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        const auto toResourceStateBarrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, resourceState);
        pCommandList->ResourceBarrier(1, &toResourceStateBarrier);
    }
    pCommandList->Close();
    ID3D12CommandList* const pRawCommandList = pCommandList.Get();
    pGraphicsQueue->ExecuteCommandLists(1, &pRawCommandList);
    commandListManager.recycleCommandList(pGraphicsQueue.Get(), pCommandList);

    // Wait for copy to complete before reading from the readback buffer.
    RenderAPI::waitForIdle(graphicsFence, pGraphicsQueue.Get());

    // Copy the data to the upload heap.
    void* pMappedReadbackBuffer;
    D3D12_RANGE readRange { 0, out.size_bytes() };
    RenderAPI::ThrowIfFailed(pReadbackBuffer->Map(0, &readRange, &pMappedReadbackBuffer));
    std::memcpy(out.data(), pMappedReadbackBuffer, out.size_bytes());
    pReadbackBuffer->Unmap(0, nullptr);
}

template <typename T>
RenderAPI::D3D12MAResource Render::RenderContext::createBufferWithData(const T& data, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES initialState, size_t alignmentPadding)
{
    static_assert(!Util::is_std_vector<T>::value, "createBufferWith*Array*Data to create array data");
    static_assert(!Util::is_std_array<T>::value, "createBufferWith*Array*Data to create array data");

    if (!alignmentPadding) {
        const std::span arrayData { &data, 1 };
        return createBufferWithArrayData(arrayData, resourceFlags, initialState);
    } else {
        const size_t alignedSize = Util::roundUpToClosestMultiple(sizeof(T), alignmentPadding);
        std::array<std::byte, sizeof(T) + 256> paddedBuffer;
        std::memcpy(paddedBuffer.data(), &data, alignedSize);
        return createBufferWithArrayData<std::byte>(std::span(paddedBuffer).subspan(0, alignedSize), resourceFlags, initialState);
    }
}

template <typename T>
RenderAPI::D3D12MAResource RenderContext::createBufferWithArrayData(std::span<const T> data, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES finalState)
{
    const size_t bufferSize = data.size() * sizeof(T);
    auto pCopyBuffer = createResource(
        D3D12_HEAP_TYPE_UPLOAD,
        CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE),
        D3D12_RESOURCE_STATE_GENERIC_READ);
    // NOTE(Mathijs): we cannot create the buffer in D3D12_RESOURCE_STATE_COPY_DEST state as this gives a validation layer warning:
    // > Ignoring InitialState D3D12_RESOURCE_STATE_COPY_DEST. Buffers are effectively created in state D3D12_RESOURCE_STATE_COMMON
    auto pFinalBuffer = createResource(
        D3D12_HEAP_TYPE_DEFAULT,
        CD3DX12_RESOURCE_DESC::Buffer(bufferSize, resourceFlags),
        D3D12_RESOURCE_STATE_COMMON);

    // Copy the data to the upload heap.
    void* pMappedCopyBuffer;
    D3D12_RANGE readRange { 0, 0 };
    RenderAPI::ThrowIfFailed(pCopyBuffer->Map(0, &readRange, &pMappedCopyBuffer));
    std::memcpy(pMappedCopyBuffer, data.data(), bufferSize);
    pCopyBuffer->Unmap(0, nullptr);

    // Copy from the upload heap to the default heap.
    auto pCommandList = commandListManager.acquireCommandList();
    const auto initialStateTransition = CD3DX12_RESOURCE_BARRIER::Transition(
        pFinalBuffer.pResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    pCommandList->ResourceBarrier(1, &initialStateTransition);
    pCommandList->CopyBufferRegion(
        pFinalBuffer.pResource.Get(),
        0,
        pCopyBuffer.pResource.Get(),
        0,
        bufferSize);
    const auto finalStateTransition = CD3DX12_RESOURCE_BARRIER::Transition(
        pFinalBuffer.pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
    pCommandList->ResourceBarrier(1, &finalStateTransition);

    pCommandList->Close();
    ID3D12CommandList* const pRawCommandList = pCommandList.Get();
    pGraphicsQueue->ExecuteCommandLists(1, &pRawCommandList);
    commandListManager.recycleCommandList(pGraphicsQueue.Get(), pCommandList);

    // Wait for copy to complete before freeing the copy buffer.
    waitForIdle();

    // pCopyBuffer will be deallocated automatically due to RAII.
    return pFinalBuffer;
}

template <typename T>
RenderAPI::SRVDescOwning RenderContext::createBufferSRVWithArrayData(std::span<const T> data, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES initialState)
{
    return {
        .desc = D3D12_SHADER_RESOURCE_VIEW_DESC {
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer = D3D12_BUFFER_SRV {
                .FirstElement = 0,
                .NumElements = (UINT)data.size(),
                .StructureByteStride = sizeof(T),
                .Flags = D3D12_BUFFER_SRV_FLAG_NONE } },
        .pResource = createBufferWithArrayData(data, resourceFlags, initialState),
    };
}
}