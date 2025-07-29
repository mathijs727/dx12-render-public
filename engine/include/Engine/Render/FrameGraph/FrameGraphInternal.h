#pragma once
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/Util/CompileTimeStringMap.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <Tbx/move_only.h>
#include <tuple>
#include <utility> // std::forward

namespace Render::FrameGraphInternal {
enum class FGResourceType {
    Transient,
    Persistent,
    SwapChain
};
struct FGResource {
    FGResourceType resourceType;
    size_t firstResourceAccessIndex = (size_t)-1;
    size_t lastResourceAccessIndex = (size_t)-1;

    // Non-owning pointer to the same resource.
    WRL::ComPtr<ID3D12Resource> pResource;
    D3D12_RESOURCE_STATES currentState;

    CD3DX12_RESOURCE_DESC desc;
    DXGI_FORMAT dsvFormat;
    DXGI_FORMAT srvFormat;
};

enum class FGResourceAccessType {
    RenderTarget,
    Depth,
    General
};
struct FGResourceAccess {
    uint32_t resourceIdx;
    FGResourceAccessType accessType;
    D3D12_RESOURCE_STATES desiredState;
};

struct IFGRenderPassImpl {
    virtual ~IFGRenderPassImpl() = default;

    virtual void initialize(RenderContext&, D3D12_GRAPHICS_PIPELINE_STATE_DESC*, D3DX12_MESH_SHADER_PIPELINE_STATE_DESC*) = 0;
    virtual void destroy(RenderContext&) = 0;
    virtual bool willDisplayGUI() const = 0;
    virtual void displayGUI() = 0;
    virtual void execute(std::span<const FGResource>, std::span<const FGResourceAccess>, const FrameGraphExecuteArgs&) = 0;
};

struct FGRenderPass {
    Tbx::MovePointer<RenderContext> pRenderContext;
    std::unique_ptr<IFGRenderPassImpl> pImplementation;
    RenderPassType renderPassType;
    std::string name;

    size_t resourceAccessBegin, resourceAccessEnd;

    FGRenderPass() = default;
    ~FGRenderPass()
    {
        if (pRenderContext)
            pImplementation->destroy(*pRenderContext);
    }
    DEFAULT_MOVE(FGRenderPass);
};

struct FGBindingsMap {
    inline consteval void add(Util::CompileTimeString bindingSlot, FGResourceAccess access)
    {
        const auto idx = bindingSlotNameToIndex.size;
        resourceAccess[idx] = access;
        bindingSlotNameToIndex.append(bindingSlot, idx);
    }
    inline consteval bool contains(Util::CompileTimeString bindingSlot) const
    {
        return bindingSlotNameToIndex.contains(bindingSlot);
    }
    inline consteval int findResourceAccessIndex(Util::CompileTimeString bindingSlot) const
    {
        return bindingSlotNameToIndex.find(bindingSlot);
    }
    inline consteval FGBindingsMap erased(Util::CompileTimeString bindingSlot) const
    {
        FGBindingsMap out = *this;
        out.bindingSlotNameToIndex = bindingSlotNameToIndex.erased(bindingSlot);
        return out;
    }
    inline consteval size_t numUnbound() const { return bindingSlotNameToIndex.size; }

    Util::CompileTimeMap<Util::CompileTimeString, int> bindingSlotNameToIndex;
    std::array<FGResourceAccess, 8> resourceAccess;
};

}