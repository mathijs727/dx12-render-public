#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include <type_traits>

namespace Render {

template <bool SupportTAA>
class ForwardPass {
public:
    struct Settings {
        Render::Scene* pScene;
    } settings;

public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useRenderTarget<"framebuffer">();
        if constexpr (SupportTAA)
            builder.useRenderTarget<"velocity">();
        builder.depthStencilWrite<"depthbuffer">();
    }

    void initialize(Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<ForwardPass<SupportTAA>>& registry, const FrameGraphExecuteArgs& args);
    void displayGUI();

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;

    inline static float m_taaJitter = 1.0f;
};

}
