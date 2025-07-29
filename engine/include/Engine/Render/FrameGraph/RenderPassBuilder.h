#pragma once
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/FrameGraphInternal.h"
#include "Engine/Util/CompileTimeStringMap.h"
#include "Engine/Util/ErrorHandling.h"
#include <array>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <tuple>

namespace Render {

class RenderPassBuilder {
public:
    template <Util::CompileTimeString name>
    inline consteval void useRenderTarget()
    {
        m_bindings.add(name, { .accessType = FrameGraphInternal::FGResourceAccessType::RenderTarget, .desiredState = D3D12_RESOURCE_STATE_RENDER_TARGET });
    }
    template <Util::CompileTimeString name>
    inline consteval void depthStencilWrite()
    {
        m_bindings.add(name, { .accessType = FrameGraphInternal::FGResourceAccessType::Depth, .desiredState = D3D12_RESOURCE_STATE_DEPTH_WRITE });
    }
    template <Util::CompileTimeString name>
    inline consteval void depthStencilRead()
    {
        m_bindings.add(name, { .accessType = FrameGraphInternal::FGResourceAccessType::Depth, .desiredState = D3D12_RESOURCE_STATE_DEPTH_READ });
    }
    template <Util::CompileTimeString name>
    inline consteval void useResource(D3D12_RESOURCE_STATES desiredState)
    {
        m_bindings.add(name, { .accessType = FrameGraphInternal::FGResourceAccessType::General, .desiredState = desiredState });
    }

private:
    friend class FrameGraphBuilder;
    template <typename T>
    friend class FrameGraphRegistry;
    FrameGraphInternal::FGBindingsMap m_bindings;
};
template <typename RenderPass>
consteval auto fillRenderPassBuilder()
{
    RenderPassBuilder builder {};
    RenderPass::declareFrameResources(builder);
    return builder;
}

// Forward declaration so we friend it.
template <typename T, FrameGraphInternal::FGBindingsMap bindingsMap>
class RenderPassBinder;

// Bind frame graph resources to render pass slots
class FrameGraphBuilder;
template <typename T>
struct FinalRenderPassBinder {
    inline T* finalize() { return m_pRenderPass; };

private:
    template <typename T, FrameGraphInternal::FGBindingsMap bindingsMap>
    friend class RenderPassBinder;
    friend class FrameGraphBuilder;

    FinalRenderPassBinder(T* pRenderPass)
        : m_pRenderPass(pRenderPass)
    {
    }

private:
    T* m_pRenderPass;
};
template <typename T, FrameGraphInternal::FGBindingsMap bindingsMap>
class RenderPassBinder {
public:
    template <Util::CompileTimeString bindingSlot>
    [[nodiscard]] auto bind(uint32_t resourceHandle)
    {
        static_assert(bindingsMap.contains(bindingSlot), "Binding slot not declared by render pass.");
        constexpr auto localAccessIdx = bindingsMap.findResourceAccessIndex(bindingSlot);

        /* spdlog::info("FIND {} GAVE {} FOR:", bindingSlot.chars.data(), localAccessIdx);
        for (int i = 0; i < bindingsMap.bindingSlotNameToIndex.size; ++i) {
            auto key = bindingsMap.bindingSlotNameToIndex.keys[i];
            auto value = bindingsMap.bindingSlotNameToIndex.values[i];
            spdlog::info("key = {}, value = {}", key.chars.data(), value);
        } */

        auto resourceAccess = bindingsMap.resourceAccess[localAccessIdx];
        resourceAccess.resourceIdx = resourceHandle;
        m_resourceAccesses[localAccessIdx] = resourceAccess;

        constexpr auto updatedBindingsMap = bindingsMap.erased(bindingSlot);
        if constexpr (updatedBindingsMap.numUnbound() > 0)
            return RenderPassBinder<T, updatedBindingsMap>(m_pRenderPass, m_resourceAccesses);
        else
            return FinalRenderPassBinder<T>(m_pRenderPass);
    }

    inline void finalize() const
    {
        // Fail compilation if this function is ever invoked (instead of FinalRenderPassBinder::finalize()).
        // Using a lambda work-around because static_assert(false, ...) is ill formed, even in a function that is never called.
        // https://stackoverflow.com/questions/38304847/constexpr-if-and-static-assert
        []<bool flag = false>() { static_assert(flag, "Not all resources are bound"); }();
    }

private:
    friend class FrameGraphBuilder;
    template <typename T, FrameGraphInternal::FGBindingsMap>
    friend class RenderPassBinder;

    inline RenderPassBinder(T* pRenderPass, std::span<FrameGraphInternal::FGResourceAccess> resourceAccesses)
        : m_pRenderPass(pRenderPass)
        , m_resourceAccesses(resourceAccesses)
    {
    }

private:
    T* m_pRenderPass;
    std::span<FrameGraphInternal::FGResourceAccess> m_resourceAccesses;
};

}