#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/mat4x4.hpp>
DISABLE_WARNINGS_POP()

namespace Render {

class DepthOnlyPass {
public:
    struct Settings {
        Render::Scene* pScene;
        glm::mat4 viewProjectionMatrix;
    } settings;

public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.depthStencilWrite<"depthbuffer">();
    }

    void initialize(Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<DepthOnlyPass>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
};

}
