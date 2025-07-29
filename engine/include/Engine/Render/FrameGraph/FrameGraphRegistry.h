#pragma once
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/FrameGraphInternal.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include "Engine/Util/CompileTimeStringMap.h"
#include <cassert>
#include <glm/vec2.hpp>
#include <span>

// Forward declare.
namespace Render::FrameGraphInternal {
template <render_pass T>
struct FGRenderPassImpl;
}

namespace Render {

template <typename RenderPass>
class FrameGraphRegistry {
public:
    struct ResourceBindingNameToIndex {
        template <size_t N>
        consteval ResourceBindingNameToIndex(const char (&array)[N])
        {
            idx = m_bindings.findResourceAccessIndex(Util::CompileTimeString(array));
        }
        int idx;
    };

    template <ResourceBindingNameToIndex>
    RenderAPI::SRVDesc getTextureSRV() const;
    template <ResourceBindingNameToIndex>
    RenderAPI::UAVDesc getTextureUAV() const;

    template <ResourceBindingNameToIndex>
    const FrameGraphInternal::FGResource& getInternalResource() const;

    template <ResourceBindingNameToIndex>
    glm::uvec2 getTextureResolution() const;

private:
    friend class FrameGraphBuilder;
    template <render_pass T>
    friend struct FrameGraphInternal::FGRenderPassImpl;

    static constexpr auto m_bindings = fillRenderPassBuilder<RenderPass>().m_bindings;
    std::span<const FrameGraphInternal::FGResource> m_resourceRegistry;
    std::span<const FrameGraphInternal::FGResourceAccess> m_resourceAccesses;
};

template <typename RenderPass>
template <typename FrameGraphRegistry<RenderPass>::ResourceBindingNameToIndex NameToIndex>
RenderAPI::SRVDesc FrameGraphRegistry<RenderPass>::getTextureSRV() const
{
    const auto& frameGraphResource = m_resourceRegistry[m_resourceAccesses[NameToIndex.idx].resourceIdx];
    assert(frameGraphResource.desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
    return RenderAPI::SRVDesc {
        .desc = D3D12_SHADER_RESOURCE_VIEW_DESC {
            .Format = frameGraphResource.srvFormat,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = D3D12_TEX2D_SRV { .MostDetailedMip = 0, .MipLevels = 1, .PlaneSlice = 0, .ResourceMinLODClamp = 0.0f } },
        .pResource = frameGraphResource.pResource.Get()
    };
}

template <typename RenderPass>
template <typename FrameGraphRegistry<RenderPass>::ResourceBindingNameToIndex NameToIndex>
RenderAPI::UAVDesc FrameGraphRegistry<RenderPass>::getTextureUAV() const
{
    const auto& frameGraphResource = m_resourceRegistry[m_resourceAccesses[NameToIndex.idx].resourceIdx];
    assert(frameGraphResource.desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
    return RenderAPI::UAVDesc {
        .desc = D3D12_UNORDERED_ACCESS_VIEW_DESC {
            .Format = frameGraphResource.desc.Format,
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
            .Texture2D = D3D12_TEX2D_UAV { .MipSlice = 0, .PlaneSlice = 0 } },
        .pResource = frameGraphResource.pResource.Get()
    };
}

template <typename RenderPass>
template <typename FrameGraphRegistry<RenderPass>::ResourceBindingNameToIndex NameToIndex>
const FrameGraphInternal::FGResource& FrameGraphRegistry<RenderPass>::getInternalResource() const
{
    return m_resourceRegistry[m_resourceAccesses[NameToIndex.idx].resourceIdx];
}

template <typename RenderPass>
template <typename FrameGraphRegistry<RenderPass>::ResourceBindingNameToIndex NameToIndex>
glm::uvec2 FrameGraphRegistry<RenderPass>::getTextureResolution() const
{
    const auto& resourceDesc = m_resourceRegistry[m_resourceAccesses[NameToIndex.idx].resourceIdx].desc;
    assert(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
    return glm::uvec2(resourceDesc.Width, resourceDesc.Height);
}

}
