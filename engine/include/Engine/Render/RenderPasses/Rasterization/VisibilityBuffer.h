#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/Render/RenderPasses/ForwardDeclares.h"
#include "Engine/RenderAPI/RenderAPI.h"

namespace Render {

class VisiblityBufferRenderPass {
public:
    struct Settings {
        Render::Scene* pScene;
        const Render::PrintfPass* pDebugPrintPass;
    } settings;

public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useRenderTarget<"visibilityBuffer">();
        builder.depthStencilWrite<"depthbuffer">();
    }

    void initialize(RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<VisiblityBufferRenderPass>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
};

class VisiblityToGBufferPass {
public:
    struct Settings {
        const Render::Scene* pScene;
        const Render::PrintfPass* pDebugPrintPass;
    } settings;

public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"visibilityBuffer">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useRenderTarget<"position_metallic">();
        builder.useRenderTarget<"normal_alpha">();
        builder.useRenderTarget<"baseColor">();
    }

    void initialize(const Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<VisiblityToGBufferPass>& registry, const FrameGraphExecuteArgs& args);

    void displayGUI();

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;

    enum class DisplayType : int {
        TexturedMIP0,
        TexturedRD,
        RD_DDX,
        RD_DDY,
    };
    inline static DisplayType m_displayType = DisplayType::TexturedRD;
    inline static bool m_rayDifferentialsChristoph = false;
};

}
