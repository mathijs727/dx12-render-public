#pragma once
#include "Engine/Memory/ForwardDeclares.h"
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
// Hide macro error in intrin0.h included by memory_resource
#include <memory_resource>
DISABLE_WARNINGS_POP()
#include <concepts>
#include <string_view>
#include <utility>

namespace Render {

struct FrameGraphExecuteArgs {
    RenderContext* pRenderContext;
    ID3D12GraphicsCommandList6* pCommandList;
    std::pmr::memory_resource* pMemoryResource;
};
enum class RenderPassType {
    Graphics,
    MeshShading,
    Compute,
    RayTracing
};

template <typename RenderPass>
concept render_pass_has_settings = requires(RenderPass& renderPass) {
    {
        RenderPass::settings
    } -> std::common_with<typename RenderPass::Settings&>;
};
template <typename RenderPass>
concept render_pass_has_initialize = requires(RenderPass& renderPass) {
    {
        renderPass.initialize(std::declval<RenderContext&>())
    } -> std::same_as<void>;
};
template <typename RenderPass>
concept render_pass_has_initialize_graphics = requires(RenderPass& renderPass) {
    {
        renderPass.initialize(std::declval<RenderContext&>(), std::declval<D3D12_GRAPHICS_PIPELINE_STATE_DESC>())
    } -> std::same_as<void>;
};
template <typename RenderPass>
concept render_pass_has_initialize_mesh_shading = requires(RenderPass& renderPass) {
    {
        renderPass.initialize(std::declval<RenderContext&>(), std::declval<D3DX12_MESH_SHADER_PIPELINE_STATE_DESC>())
    } -> std::same_as<void>;
};

template <typename RenderPass>
concept render_pass_has_destroy = requires(RenderPass& renderPass) {
    {
        renderPass.destroy(std::declval<RenderContext&>())
    } -> std::same_as<void>;
};
template <typename RenderPass>
concept render_pass_has_display_gui = requires(RenderPass& renderPass) {
    {
        renderPass.displayGUI()
    } -> std::same_as<void>;
};
template <typename RenderPass>
concept render_pass_has_name = requires() {
    {
        RenderPass::name
    } -> std::common_with<std::string_view&>;
};
template <typename RenderPass>
concept render_pass = requires(RenderPass& renderPass) {
    {
        RenderPass::renderPassType
    } -> std::same_as<const RenderPassType&>;
    // render_pass_has_initialize1<RenderPass> || render_pass_has_initialize2<RenderPass>;
    {
        renderPass.execute(std::declval<const FrameGraphRegistry<RenderPass>&>(), std::declval<const FrameGraphExecuteArgs&>())
    } -> std::same_as<void>;
};

}
