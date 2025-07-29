#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/Render/RenderPasses/ForwardDeclares.h"
#include "Engine/RenderAPI/RenderAPI.h"

namespace Render {

class RasterDebugPass {
public:
    struct Settings {
        Render::Scene* pScene;
        const VisualDebugPass* pVisualDebugPass { nullptr };
    } settings;

public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useRenderTarget<"framebuffer">();
        builder.depthStencilWrite<"depthbuffer">();
    }

    void initialize(const Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<RasterDebugPass>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
};

}
