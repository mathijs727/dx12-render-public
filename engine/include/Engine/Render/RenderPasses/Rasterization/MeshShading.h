#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/RenderAPI.h"

namespace Render {

class MeshShadingPass {
public:
    struct Settings {
        Render::Scene* pScene;
        bool bindless;
    } settings;

public:
    static constexpr RenderPassType renderPassType = RenderPassType::MeshShading;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useRenderTarget<"framebuffer">();
        builder.depthStencilWrite<"depthbuffer">();
    }

    void initialize(const Render::RenderContext& renderContext, D3DX12_MESH_SHADER_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<MeshShadingPass>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
};

}
