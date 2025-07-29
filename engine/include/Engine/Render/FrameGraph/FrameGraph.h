#pragma once
#include "Engine/Memory/ForwardDeclares.h"
#include "Engine/Memory/LinearAllocator.h"
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/FrameGraphInternal.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/RenderAPI/MaResource.h"
#include "Engine/RenderAPI/MemoryAliasing.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "FrameGraphInternal.h"
#include "RenderPassBuilder.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec4.hpp>
DISABLE_WARNINGS_POP()
#include <memory>
#include <optional>
#include <tbx/move_only.h>
#include <vector>

namespace Render {

class FrameGraphBuilder;

// Frame graph implementation inspired by "FrameGraph: Extensible Rendering Architecture in Frostbite" by Yuriy O'Donnell
// https://media.gdcvault.com/gdc2017/Presentations/ODonnell_Yuriy_FrameGraph.pdf
class FrameGraph {
public:
    FrameGraph() = default;
    NO_COPY(FrameGraph);
    DEFAULT_MOVE(FrameGraph);

    void displayGUI() const;
    void execute(GPUFrameProfiler* pProfiler = nullptr);

    RenderAPI::D3D12MAResource const* getPersistentResource(uint32_t resourceIdx) const
    {
        if (resourceIdx >= m_persistentResourcesAllocations.size())
            throw std::out_of_range("Invalid resource index");
        return &m_persistentResourcesAllocations[resourceIdx];
    }

private:
    friend class FrameGraphBuilder;

private:
    Tbx::MovePointer<RenderContext> m_pRenderContext;
    std::vector<FrameGraphInternal::FGResource> m_resourceRegistry;
    std::vector<FrameGraphInternal::FGResourceAccess> m_resourceAccesses;
    std::vector<FrameGraphInternal::FGRenderPass> m_operations;

    std::optional<RenderAPI::ResourceAliasManager> m_resourceAliasingManager;
    std::vector<RenderAPI::D3D12MAResource> m_persistentResourcesAllocations;
};

class FrameGraphBuilder {
public:
    FrameGraphBuilder(Render::RenderContext*);

    template <render_pass T>
    auto addOperation(T::Settings&& settings);
    template <render_pass T>
    auto addOperation();

    void clearFrameBuffer(uint32_t frameBuffer, const glm::vec4& clearColor = glm::vec4(0.0f));
    void clearDepthBuffer(uint32_t depthBuffer, const float depth = 1.0f);

    struct Formats {
        DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
        DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
    };
    uint32_t getSwapChainResource();
    uint32_t createTransientResource(const CD3DX12_RESOURCE_DESC& desc, Formats formats = {});
    uint32_t createPersistentResource(const CD3DX12_RESOURCE_DESC& desc, Formats formats = {});
    glm::uvec2 getTextureResolution(uint32_t resourceIdx) const;

    FrameGraph compile();

private:
    template <render_pass T, typename... Args>
    auto addOperationInternal(Args&&... args);

    uint32_t createResourceInternal(FrameGraphInternal::FGResourceType resourceType, const CD3DX12_RESOURCE_DESC& desc, Formats formats);

private:
    Render::RenderContext* m_pRenderContext;
    uint32_t m_frameBuffer;
    std::vector<FrameGraphInternal::FGResource> m_resourceRegistry;
    std::vector<FrameGraphInternal::FGResourceAccess> m_resourceAccesses;
    std::vector<FrameGraphInternal::FGRenderPass> m_operations;
};

namespace FrameGraphInternal {
    template <render_pass T>
    struct FGRenderPassImpl : public IFGRenderPassImpl {
        template <typename... Args>
        FGRenderPassImpl(Args&&... args)
            : renderPass(std::forward<Args>(args)...)
        {
        }

        void initialize(RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC* optGraphicsPipelineStateDesc, D3DX12_MESH_SHADER_PIPELINE_STATE_DESC* optMeshShadingPipelineStateDesc) override
        {
            if constexpr (render_pass_has_initialize<T>) {
                renderPass.initialize(renderContext);
            } else if constexpr (render_pass_has_initialize_graphics<T>) {
                renderPass.initialize(renderContext, *optGraphicsPipelineStateDesc);
            } else if constexpr (render_pass_has_initialize_mesh_shading<T>) {
                renderPass.initialize(renderContext, *optMeshShadingPipelineStateDesc);
            }
        }
        void destroy(RenderContext& renderContext) override
        {
            if constexpr (render_pass_has_destroy<T>) {
                renderPass.destroy(renderContext);
            }
        }

        bool willDisplayGUI() const override { return render_pass_has_display_gui<T>; }
        void displayGUI() override
        {
            if constexpr (render_pass_has_display_gui<T>)
                renderPass.displayGUI();
        }

        void execute(std::span<const FGResource> resourceRegistry, std::span<const FGResourceAccess> resourceAccesses, const FrameGraphExecuteArgs& args) override
        {
            FrameGraphRegistry<T> registry;
            registry.m_resourceRegistry = resourceRegistry;
            registry.m_resourceAccesses = resourceAccesses;
            renderPass.execute(registry, args);
        }

        T renderPass;
    };
}

template <render_pass T>
[[nodiscard]] auto FrameGraphBuilder::addOperation(T::Settings&& settings)
{
    return addOperationInternal<T>(std::move(settings));
}
template <render_pass T>
[[nodiscard]] auto FrameGraphBuilder::addOperation()
{
    return addOperationInternal<T>();
}
template <render_pass T, typename... Args>
[[nodiscard]] auto FrameGraphBuilder::addOperationInternal(Args&&... args)
{
    constexpr RenderPassBuilder builder = fillRenderPassBuilder<T>();

    FrameGraphInternal::FGRenderPass renderPass;
    renderPass.pRenderContext = m_pRenderContext;
    auto pImplementation = std::make_unique<FrameGraphInternal::FGRenderPassImpl<T>>();
    T* pRenderPass = &pImplementation->renderPass;
    if constexpr (sizeof...(Args) == 1) {
        pImplementation->renderPass.settings = { std::forward<Args>(args)... };
    }
    renderPass.pImplementation = std::move(pImplementation);
    renderPass.renderPassType = T::renderPassType;
    renderPass.resourceAccessBegin = m_resourceAccesses.size();
    m_resourceAccesses.resize(m_resourceAccesses.size() + builder.m_bindings.numUnbound());
    renderPass.resourceAccessEnd = m_resourceAccesses.size();
    if constexpr (render_pass_has_name<T>) {
        renderPass.name = T::name;
    } else {
        // Try to auto generate a name.
        renderPass.name = typeid(T).name();
        if (renderPass.name.starts_with("struct "))
            renderPass.name = renderPass.name.substr(7);
        else if (renderPass.name.starts_with("class "))
            renderPass.name = renderPass.name.substr(6);
    }
    m_operations.emplace_back(std::move(renderPass));

    if constexpr (builder.m_bindings.bindingSlotNameToIndex.size == 0) {
        return FinalRenderPassBinder<T>(pRenderPass);
    } else {
        return RenderPassBinder<T, builder.m_bindings>(
            pRenderPass, std::span(m_resourceAccesses).subspan(renderPass.resourceAccessBegin, renderPass.resourceAccessEnd - renderPass.resourceAccessBegin));
    }
}

}
