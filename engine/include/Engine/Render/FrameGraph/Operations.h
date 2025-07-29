#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec4.hpp>
DISABLE_WARNINGS_POP()

namespace Render {

struct ClearFrameBuffer {
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;
    
    struct Settings {
        glm::vec4 clearColor;
    } settings;

    static consteval void declareFrameResources(RenderPassBuilder& builder) { builder.useResource<"buffer">(D3D12_RESOURCE_STATE_RENDER_TARGET); }
    void execute(const FrameGraphRegistry<ClearFrameBuffer>& registry, const FrameGraphExecuteArgs& args);
};
struct ClearDepthBuffer {
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    struct Settings {
        float depthValue;
    } settings;

    static consteval void declareFrameResources(RenderPassBuilder& builder) { builder.useResource<"buffer">(D3D12_RESOURCE_STATE_DEPTH_WRITE); }
    void execute(const FrameGraphRegistry<ClearDepthBuffer>& registry, const FrameGraphExecuteArgs& args);
};
struct CopyTexture {
    static constexpr RenderPassType renderPassType = RenderPassType::Compute;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"source">(D3D12_RESOURCE_STATE_COPY_SOURCE);
        builder.useResource<"dest">(D3D12_RESOURCE_STATE_COPY_DEST);
    }
    void execute(const FrameGraphRegistry<CopyTexture>& registry, const FrameGraphExecuteArgs& args);
};
struct ShaderCopyTexture {
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"source">(D3D12_RESOURCE_STATE_COPY_SOURCE);
        builder.useRenderTarget<"dest">();
    }
    void initialize(const Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<ShaderCopyTexture>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
};
struct CopyTextureChannels {
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    // Mapping from input channels to output channels.
    // Numbers in the range of [0, 3] representing the R, G, B and A channel in the input texture.
    struct Settings {
        uint32_t r = 0, g = 0, b = 0;
        float offset = 0.0f;
        float scaling = 1.0f;
    } settings;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"source">(D3D12_RESOURCE_STATE_COPY_SOURCE);
        builder.useRenderTarget<"dest">();
    }
    void initialize(const Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<CopyTextureChannels>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
};

}