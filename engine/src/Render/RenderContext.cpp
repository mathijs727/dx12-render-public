#include "Engine/Render/RenderContext.h"
#include "Engine/Core/Window.h"
#include "Engine/RenderAPI/ImGui.h"
#include "Engine/RenderAPI/MemoryAliasing.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <cassert>

namespace Render {

static constexpr uint32_t descriptorAllocBlockSize = 2048;
static constexpr uint32_t dsvRtvDescriptorAllocBlockSize = 32;

static RenderAPI::D3D12MAWrapper<D3D12MA::Allocator> createGpuMemoryAllocator(IDXGIAdapter4* pAdapter, ID3D12Device5* pDevice);

RenderContext::RenderContext()
    : pAdapter(RenderAPI::createAdapter())
    , pDevice(RenderAPI::createDevice(pAdapter.Get()))
    , pResourceAllocator(createGpuMemoryAllocator(pAdapter.Get(), pDevice.Get()))
    , pGraphicsQueue(RenderAPI::createCommandQueue(pDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT))
    , graphicsFence(RenderAPI::createFence(pDevice.Get()))
    , commandListManager(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT)
    , pCbvSrvUavDescriptorBaseAllocatorCPU(std::make_unique<RenderAPI::DescriptorBlockAllocator>(
          pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false, descriptorAllocBlockSize, 20))
    , pCbvSrvUavDescriptorBaseAllocatorGPU(std::make_unique<RenderAPI::DescriptorBlockAllocator>(
          pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true, descriptorAllocBlockSize, 40))
    , pRtvDescriptorBaseAllocatorCPU(std::make_unique<RenderAPI::DescriptorBlockAllocator>(
          pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false, dsvRtvDescriptorAllocBlockSize, 20))
    , pDsvDescriptorBaseAllocatorCPU(std::make_unique<RenderAPI::DescriptorBlockAllocator>(
          pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false, dsvRtvDescriptorAllocBlockSize, 20))
    , cbvSrvUavDescriptorStaticAllocator(
          pDevice, pCbvSrvUavDescriptorBaseAllocatorCPU.get(), pCbvSrvUavDescriptorBaseAllocatorGPU.get())
    , rtvDescriptorAllocator(pRtvDescriptorBaseAllocatorCPU.get())
    , dsvDescriptorAllocator(pDsvDescriptorBaseAllocatorCPU.get())
    , singleFrameBufferAllocator(pDevice, 8 * 1024 * 1024, RenderAPI::SwapChain::s_parallelFrames)
{
    for (auto& frameFenceValue : frameFenceValues) {
        frameFenceValue = RenderAPI::insertFence(graphicsFence, pGraphicsQueue.Get());
        cbvSrvUavDescriptorTransientAllocators.emplace_back(pDevice, pCbvSrvUavDescriptorBaseAllocatorCPU.get(), pCbvSrvUavDescriptorBaseAllocatorGPU.get());
    }
}

RenderContext::RenderContext(const Core::Window& window, bool enableImgui)
    : RenderContext()
{
    // Create swap chain.
    optSwapChain = RenderAPI::createSwapChain(pDevice.Get(), pGraphicsQueue.Get(), window.hWnd, window.size.x, window.size.y, true);

    if (enableImgui) {
        // ImGui setup.
        optImGuiDescriptorAllocation = cbvSrvUavDescriptorStaticAllocator.allocate(1);
        RenderAPI::initImGuiDescriptorD3D12(
            pDevice.Get(), pCbvSrvUavDescriptorBaseAllocatorGPU->pDescriptorHeap.Get(), *optImGuiDescriptorAllocation, *optSwapChain);
        cbvSrvUavDescriptorStaticAllocator.flush();
    }
}

RenderContext::~RenderContext()
{
    if (optImGuiDescriptorAllocation)
        cbvSrvUavDescriptorStaticAllocator.release(*optImGuiDescriptorAllocation);

    D3D12MA::TotalStatistics stats;
    pResourceAllocator->CalculateStatistics(&stats);
    spdlog::debug("pResourceAllocator has {} used bytes at shutdown", stats.Total.Stats.AllocationBytes);
}

RenderAPI::ResourceAliasManager RenderContext::createResourceAliasManager(size_t size)
{
    return RenderAPI::ResourceAliasManager(pDevice, pResourceAllocator, size, RenderAPI::SwapChain::s_parallelFrames);
}

RenderAPI::D3D12MAResource RenderContext::createResource(D3D12_HEAP_TYPE heapType, const D3D12_RESOURCE_DESC& resourceDesc, D3D12_RESOURCE_STATES initialState)
{
    const D3D12MA::ALLOCATION_DESC allocationDesc {
        .HeapType = heapType
    };

    WRL::ComPtr<ID3D12Resource> pResource;
    D3D12MA::Allocation* pAllocation;
    RenderAPI::ThrowIfFailed(
        pResourceAllocator->CreateResource(
            &allocationDesc,
            &resourceDesc,
            initialState,
            nullptr,
            &pAllocation,
            IID_PPV_ARGS(&pResource)));
    return RenderAPI::D3D12MAResource(pResource, pAllocation);
}

void RenderContext::waitForIdle()
{
    RenderAPI::waitForFence(graphicsFence, RenderAPI::insertFence(graphicsFence, pGraphicsQueue.Get()));
}

void RenderContext::waitForNextFrame() const
{
    const uint64_t fenceValue = frameFenceValues[backBufferIndex];
    RenderAPI::waitForFence(graphicsFence, fenceValue);
}

void RenderContext::resetFrameAllocators()
{
    getCurrentCbvSrvUavDescriptorTransientAllocator().reset();
    rtvDescriptorAllocator.reset();
    dsvDescriptorAllocator.reset();
    // getCurrentConstantsLinearBufferAllocator().reset();
    singleFrameBufferAllocator.newFrame();
}

void RenderContext::present()
{
    auto& outFenceValue = frameFenceValues[backBufferIndex];
    outFenceValue = RenderAPI::insertFence(graphicsFence, pGraphicsQueue.Get());
    if (optSwapChain) {
        RenderAPI::present(*optSwapChain);
        backBufferIndex = optSwapChain->getCurrentBackBufferIndex();
    } else {
        // Headless mode, just increment the back buffer index.
        backBufferIndex = (backBufferIndex + 1) % RenderAPI::SwapChain::s_parallelFrames;
    }
}

RenderAPI::GPUDescriptorLinearAllocator& RenderContext::getCurrentCbvSrvUavDescriptorTransientAllocator()
{
    return cbvSrvUavDescriptorTransientAllocators[backBufferIndex];
}

void RenderContext::resizeSwapChain(const glm::uvec2& resolution)
{
    assert(optSwapChain);
    RenderAPI::resizeSwapChain(*optSwapChain, pDevice.Get(), resolution.x, resolution.y);
}

void RenderContext::submitGraphicsQueue(const WRL::ComPtr<ID3D12GraphicsCommandList6>& pCommandList)
{
    pCommandList->Close();
    std::array<ID3D12CommandList*, 1> commandLists { pCommandList.Get() };
    pGraphicsQueue->ExecuteCommandLists((UINT)commandLists.size(), commandLists.data());
    commandListManager.recycleCommandList(pGraphicsQueue.Get(), pCommandList);
}

static RenderAPI::D3D12MAWrapper<D3D12MA::Allocator> createGpuMemoryAllocator(IDXGIAdapter4* pAdapter, ID3D12Device5* pDevice)
{
    const D3D12MA::ALLOCATOR_DESC allocatorDesc {
        .pDevice = pDevice,
        .pAdapter = pAdapter
    };

    D3D12MA::Allocator* pAllocator;
    RenderAPI::ThrowIfFailed(D3D12MA::CreateAllocator(&allocatorDesc, &pAllocator));
    return pAllocator;
}

}