#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include <type_traits>

namespace Render {

class DeferredRenderPass {
public:
    struct Settings {
        Render::Scene* pScene;
    } settings;

public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useRenderTarget<"position_metallic">();
        builder.useRenderTarget<"normal_alpha">();
        builder.useRenderTarget<"baseColor">();
        builder.depthStencilWrite<"depthbuffer">();
    }

    void initialize(Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<DeferredRenderPass>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
};

class SunVisibilityRTPass {
public:
    struct Settings {
        const Render::Scene* pScene;
    } settings;

public:
    static constexpr RenderPassType renderPassType = RenderPassType::Compute;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"position_metallic">(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        builder.useResource<"sunVisibility">(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    void initialize(Render::RenderContext& renderContext);
    void execute(const FrameGraphRegistry<SunVisibilityRTPass>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
    uint32_t m_frameIndex = 0;
};

class DeferredShadingPass {
public:
    struct Settings {
        const Render::Scene* pScene;
    } settings;

public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"position_metallic">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useResource<"normal_alpha">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useResource<"baseColor">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useResource<"sunVisibility">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useRenderTarget<"framebuffer">();
    }

    void initialize(Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<DeferredShadingPass>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
};

}
