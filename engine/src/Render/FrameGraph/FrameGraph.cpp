#include "Engine/Render/FrameGraph/FrameGraph.h"
#include "Engine/Render/FrameGraph/Operations.h"
#include "Engine/Render/GPUProfiler.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/RenderAPI/Buffer/CpuBufferLinearAllocator.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/Util/ErrorHandling.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <EASTL/fixed_vector.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
DISABLE_WARNINGS_POP()
#include <functional>
#include <limits>
#include <optional>
#include <tbx/variant_helper.h>
#include <unordered_map>
#include <vector>

using namespace RenderAPI;
using namespace Render::FrameGraphInternal;

namespace Render {

void FrameGraph::displayGUI() const
{
    bool first = true;
    for (const auto& operation : m_operations) {
        if (!operation.pImplementation->willDisplayGUI())
            continue;

        if (!first) {
            ImGui::Spacing();
            ImGui::Separator();
            first = false;
        }
        ImGui::Text("%s", operation.name.c_str());
        ImGui::Spacing();

        operation.pImplementation->displayGUI();
    }
}

void FrameGraph::execute(GPUFrameProfiler* pProfiler)
{
    auto pCommandList = m_pRenderContext->commandListManager.acquireCommandList();
    if (pProfiler)
        pProfiler->startFrame(pCommandList.Get());

    // Update the framebuffer resource to point to the current frame.
    for (auto& resource : m_resourceRegistry) {
        if (resource.resourceType == FGResourceType::SwapChain)
            resource.pResource = m_pRenderContext->optSwapChain->getCurrentBackBuffer();
    }

    const auto insertAliasingBarrierIfNecessary = [&](const size_t resourceAccessIdx) {
        const auto& resourceAccess = m_resourceAccesses[resourceAccessIdx];
        auto& resource = m_resourceRegistry[resourceAccess.resourceIdx];
        if (resource.resourceType == FGResourceType::Transient && resource.firstResourceAccessIndex == resourceAccessIdx) {
            const auto barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(nullptr, resource.pResource.Get());
            pCommandList->ResourceBarrier(1, &barrier);
        }
    };
    const auto insertUAVBarrierIfNecessary = [&](const size_t resourceAccessIdx) {
        const auto& resourceAccess = m_resourceAccesses[resourceAccessIdx];
        const auto& resource = m_resourceRegistry[resourceAccess.resourceIdx];
        // No need for a UAV barrier when also performing a state transition.
        // if (resource.currentState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && resourceAccess.desiredState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        if (resource.currentState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS || resourceAccess.desiredState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
            assert(resource.pResource);
            const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.pResource.Get());
            pCommandList->ResourceBarrier(1, &barrier);
        }
    };
    const auto transitionResourceIfNecessary = [&](const size_t resourceAccessIdx) {
        const auto& resourceAccess = m_resourceAccesses[resourceAccessIdx];
        auto& resource = m_resourceRegistry[resourceAccess.resourceIdx];
        if (resource.currentState != resourceAccess.desiredState) {
            // TODO(Mathijs): batch state transitions
            const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.pResource.Get(), resource.currentState, resourceAccess.desiredState);
            pCommandList->ResourceBarrier(1, &barrier);
            resource.currentState = resourceAccess.desiredState;
        }
    };

    const std::array descriptorHeaps {
        m_pRenderContext->pCbvSrvUavDescriptorBaseAllocatorGPU->pDescriptorHeap.Get(),
        // renderContext.pImGuiDescriptorHeap.Get()
    };
    pCommandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());
    for (const auto& operation : m_operations) {
        uint32_t profilerTaskHandle = (uint32_t)-1;
        if (pProfiler)
            profilerTaskHandle = pProfiler->startTask(pCommandList.Get(), operation.name);

        for (size_t resourceAccessIdx = operation.resourceAccessBegin; resourceAccessIdx < operation.resourceAccessEnd; resourceAccessIdx++) {
            insertAliasingBarrierIfNecessary(resourceAccessIdx);
            // BEFORE stateTransition so it knows when **not** to insert a barrier (state transition == barrier).
            insertUAVBarrierIfNecessary(resourceAccessIdx);
            transitionResourceIfNecessary(resourceAccessIdx);
        }

        if (operation.renderPassType == RenderPassType::Graphics || operation.renderPassType == RenderPassType::MeshShading) {
            // Create & fill render pass info struct.
            eastl::fixed_vector<CD3DX12_CPU_DESCRIPTOR_HANDLE, 8, false> rtvDescriptorHandles;
            std::optional<CD3DX12_CPU_DESCRIPTOR_HANDLE> optDsvDescriptorHandle {};
            D3D12_VIEWPORT viewport { .TopLeftX = 0.0f, .TopLeftY = 0.0f, .MinDepth = 0.0f, .MaxDepth = 1.0f };
            D3D12_RECT scissorRect { .left = 0, .top = 0 };
            for (size_t resourceAccessIdx = operation.resourceAccessBegin; resourceAccessIdx < operation.resourceAccessEnd; resourceAccessIdx++) {
                const auto& resourceAccess = m_resourceAccesses[resourceAccessIdx];
                auto& resource = m_resourceRegistry[resourceAccess.resourceIdx];

                if (resourceAccess.accessType == FGResourceAccessType::RenderTarget) {
                    // Allocate RenderTargetView descriptor
                    const auto rtvDescriptor = m_pRenderContext->rtvDescriptorAllocator.allocate(1);
                    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {
                        .Format = resource.desc.Format,
                        .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D
                    };
                    rtvDesc.Texture2D.MipSlice = rtvDesc.Texture2D.PlaneSlice = 0;
                    m_pRenderContext->pDevice->CreateRenderTargetView(resource.pResource.Get(), &rtvDesc, rtvDescriptor);
                    rtvDescriptorHandles.push_back(rtvDescriptor);
                } else if (resourceAccess.accessType == FGResourceAccessType::Depth) {
                    // D3D12_RESOURCE_STATE_DEPTH_READ and/or D3D12_RESOURCE_STATE_DEPTH_WRITE
                    const auto dsvDescriptor = m_pRenderContext->dsvDescriptorAllocator.allocate(1);
                    const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {
                        .Format = resource.dsvFormat,
                        .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                        .Flags = D3D12_DSV_FLAG_NONE,
                        .Texture2D = D3D12_TEX2D_DSV {}
                    };
                    m_pRenderContext->pDevice->CreateDepthStencilView(resource.pResource.Get(), &dsvDesc, dsvDescriptor);
                    optDsvDescriptorHandle = dsvDescriptor;
                }

                if (resourceAccess.accessType == FGResourceAccessType::RenderTarget || resourceAccess.accessType == FGResourceAccessType::Depth) {
                    scissorRect.right = (LONG)resource.desc.Width;
                    scissorRect.bottom = (LONG)resource.desc.Height;
                    viewport.Width = (FLOAT)resource.desc.Width;
                    viewport.Height = (FLOAT)resource.desc.Height;
                }
            }

            pCommandList->RSSetViewports(1, &viewport);
            pCommandList->RSSetScissorRects(1, &scissorRect);
            pCommandList->OMSetRenderTargets((UINT)rtvDescriptorHandles.size(), rtvDescriptorHandles.data(), false, optDsvDescriptorHandle ? &optDsvDescriptorHandle.value() : nullptr);
        }

        const std::span<const FGResourceAccess> resourceAccesses = std::span(m_resourceAccesses).subspan(operation.resourceAccessBegin, operation.resourceAccessEnd - operation.resourceAccessBegin);
        const FrameGraphExecuteArgs executeArgs {
            .pRenderContext = m_pRenderContext,
            .pCommandList = pCommandList.Get()
        };
        operation.pImplementation->execute(m_resourceRegistry, resourceAccesses, executeArgs);
        if (pProfiler)
            pProfiler->endTask(pCommandList.Get(), profilerTaskHandle);
    }

    // Transition the frame buffer to the PRESENT state.
    for (auto& resource : m_resourceRegistry) {
        if (resource.resourceType == FGResourceType::SwapChain && resource.currentState != D3D12_RESOURCE_STATE_PRESENT) {
            const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.pResource.Get(), resource.currentState, D3D12_RESOURCE_STATE_PRESENT);
            pCommandList->ResourceBarrier(1, &barrier);
            resource.currentState = D3D12_RESOURCE_STATE_PRESENT;
        }
    }

    if (pProfiler)
        pProfiler->endFrame(pCommandList.Get());

    m_pRenderContext->getCurrentCbvSrvUavDescriptorTransientAllocator().flush();
    pCommandList->Close();
    ID3D12CommandList* const pRawCommandList = pCommandList.Get();
    m_pRenderContext->pGraphicsQueue->ExecuteCommandLists(1, &pRawCommandList);
    m_pRenderContext->commandListManager.recycleCommandList(m_pRenderContext->pGraphicsQueue.Get(), pCommandList);
}

FrameGraphBuilder::FrameGraphBuilder(Render::RenderContext* pRenderContext)
    : m_pRenderContext(pRenderContext)
{
    if (m_pRenderContext->optSwapChain) {
        const auto dummyDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R8G8B8A8_UNORM, m_pRenderContext->optSwapChain->width, m_pRenderContext->optSwapChain->height);
        m_frameBuffer = (uint32_t)m_resourceRegistry.size();
        m_resourceRegistry.push_back(FGResource { .resourceType = FGResourceType::SwapChain, .currentState = D3D12_RESOURCE_STATE_PRESENT, .desc = dummyDesc });
    } else {
        m_frameBuffer = (uint32_t)-1;
    }
}

void FrameGraphBuilder::clearFrameBuffer(uint32_t frameBuffer, const glm::vec4& color)
{
    addOperation<Render::ClearFrameBuffer>({ color }).bind<"buffer">(frameBuffer).finalize();
}

void FrameGraphBuilder::clearDepthBuffer(uint32_t depthBuffer, float depth)
{
    addOperation<Render::ClearDepthBuffer>({ depth }).bind<"buffer">(depthBuffer).finalize();
}

uint32_t FrameGraphBuilder::getSwapChainResource()
{
    return m_frameBuffer;
    ;
}

uint32_t FrameGraphBuilder::createResourceInternal(FGResourceType resourceType, const CD3DX12_RESOURCE_DESC& desc, Formats formats)
{
    auto index = (uint32_t)m_resourceRegistry.size();
    if (formats.dsvFormat == DXGI_FORMAT_UNKNOWN)
        formats.dsvFormat = desc.Format;
    if (formats.srvFormat == DXGI_FORMAT_UNKNOWN)
        formats.srvFormat = desc.Format;
    m_resourceRegistry.push_back(FGResource {
        .resourceType = resourceType,
        .desc = desc,
        .dsvFormat = formats.dsvFormat,
        .srvFormat = formats.srvFormat });
    return index;
}

uint32_t FrameGraphBuilder::createTransientResource(const CD3DX12_RESOURCE_DESC& desc, Formats formats)
{
    return createResourceInternal(FGResourceType::Transient, desc, formats);
}

uint32_t FrameGraphBuilder::createPersistentResource(const CD3DX12_RESOURCE_DESC& desc, Formats formats)
{
    return createResourceInternal(FGResourceType::Persistent, desc, formats);
}

glm::uvec2 FrameGraphBuilder::getTextureResolution(uint32_t resourceIdx) const
{
    const auto& resourceDesc = m_resourceRegistry[resourceIdx].desc;
    assert(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
    return glm::uvec2(resourceDesc.Width, resourceDesc.Height);
}

// Invoke F on each item.
template <typename F, typename T, typename... Ts>
void invoke_items(F&& f, T&& arg, Ts&&... tail)
{
    f(std::forward<T>(arg));
    invoke_items(std::forward<F>(f), std::forward<Ts>(tail)...);
}
template <typename F, typename T>
void invoke_items(F&& f, T&& arg)
{
    f(std::forward<T>(arg));
}

FrameGraph FrameGraphBuilder::compile()
{
    // Create the global/static state (such as shaders, root signature and pipeline states) for each render pass.
    for (const auto& operation : m_operations) {
        const auto initializePipelineState = [&]<typename T>(T& pipelineStateDesc) {
            RenderAPI::setSensibleDefaultPipelineStateDesc(pipelineStateDesc);
            for (size_t resourceAccessIdx = operation.resourceAccessBegin; resourceAccessIdx < operation.resourceAccessEnd; ++resourceAccessIdx) {
                const auto& resourceAccess = m_resourceAccesses[resourceAccessIdx];
                const auto& resource = m_resourceRegistry[resourceAccess.resourceIdx];
                if (resourceAccess.accessType == FGResourceAccessType::RenderTarget) {
                    pipelineStateDesc.RTVFormats[pipelineStateDesc.NumRenderTargets++] = resource.desc.Format;
                } else if (resourceAccess.accessType == FGResourceAccessType::Depth) {
                    pipelineStateDesc.DSVFormat = resource.dsvFormat;
                }
            }
        };

        if (operation.renderPassType == RenderPassType::Graphics) {
            // Provide the render target/depth buffer layouts so we don't need to hard code them inside the render passes.
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc {};
            initializePipelineState(pipelineStateDesc);
            operation.pImplementation->initialize(*m_pRenderContext, &pipelineStateDesc, nullptr);
        } else if (operation.renderPassType == RenderPassType::MeshShading) {
            // Provide the render target/depth buffer layouts so we don't need to hard code them inside the render passes.
            D3DX12_MESH_SHADER_PIPELINE_STATE_DESC pipelineStateDesc {};
            initializePipelineState(pipelineStateDesc);
            operation.pImplementation->initialize(*m_pRenderContext, nullptr, &pipelineStateDesc);
        } else {
            operation.pImplementation->initialize(*m_pRenderContext, nullptr, nullptr);
        }
    }

    // Computes first/last operations that touch a resource.
    for (size_t resourceAccessIndex = 0; resourceAccessIndex < m_resourceAccesses.size(); ++resourceAccessIndex) {
        auto& resource = m_resourceRegistry[m_resourceAccesses[resourceAccessIndex].resourceIdx];
        resource.lastResourceAccessIndex = resourceAccessIndex;
        if (resource.firstResourceAccessIndex == (size_t)-1)
            resource.firstResourceAccessIndex = resourceAccessIndex;
    }

    // Allocate and release the temporary textures in the order that the operations will be executed.
    // Note that releasing a resource is not the same as destroying it. The resource will stay alive
    // but it's memory may be reused by another resource allocated afterwards.
    auto resourceAliasManager = m_pRenderContext->createResourceAliasManager(512 * 1024 * 1024);
    std::unordered_map<size_t, RenderAPI::AliasingResource> aliasingMemoryAllocations;
    std::vector<RenderAPI::D3D12MAResource> persistentResourcesAllocations;
    for (const auto& operation : m_operations) {
        for (size_t resourceAccessIndex = operation.resourceAccessBegin; resourceAccessIndex < operation.resourceAccessEnd; ++resourceAccessIndex) {
            const auto& resourceAccess = m_resourceAccesses[resourceAccessIndex];
            auto& resource = m_resourceRegistry[resourceAccess.resourceIdx];
            // Buffer cannot be created in D3D12_RESOURCE_STATE_UNORDERED_ACCESS state.
            const auto initialState = resourceAccess.desiredState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS ? resourceAccess.desiredState : D3D12_RESOURCE_STATE_COMMON;
            if (resource.resourceType == FGResourceType::Transient && resource.firstResourceAccessIndex == resourceAccessIndex) {
                auto& allocation = aliasingMemoryAllocations[resourceAccess.resourceIdx] = resourceAliasManager.allocate(resource.desc, initialState);
                resource.pResource = allocation.pResource;
                resource.currentState = initialState;
            }

            if (resource.resourceType == FGResourceType::Persistent && resource.firstResourceAccessIndex == resourceAccessIndex) {
                auto resourceAllocation = m_pRenderContext->createResource(D3D12_HEAP_TYPE_DEFAULT, resource.desc, initialState);
                resource.pResource = resourceAllocation;
                persistentResourcesAllocations.emplace_back(std::move(resourceAllocation));
                resource.currentState = initialState;
            }
        }
        for (size_t resourceAccessIndex = operation.resourceAccessBegin; resourceAccessIndex < operation.resourceAccessEnd; ++resourceAccessIndex) {
            const auto& resourceAccess = m_resourceAccesses[resourceAccessIndex];
            auto& resource = m_resourceRegistry[resourceAccess.resourceIdx];
            if (resource.resourceType == FGResourceType::Transient && resource.lastResourceAccessIndex == resourceAccessIndex) {
                resourceAliasManager.releaseMemory(aliasingMemoryAllocations.find(resourceAccess.resourceIdx)->second);
            }
        }
    }

    FrameGraph out;
    out.m_pRenderContext = m_pRenderContext;
    out.m_resourceRegistry = std::move(m_resourceRegistry);
    out.m_resourceAccesses = std::move(m_resourceAccesses);
    out.m_operations = std::move(m_operations);
    out.m_resourceAliasingManager = std::move(resourceAliasManager); // Ensure that the memory used by the transient memory pool remains allocated.
    out.m_persistentResourcesAllocations = std::move(persistentResourcesAllocations); // Ensure that the memory used by the transient memory pool remains allocated.
    return out;
}

}
