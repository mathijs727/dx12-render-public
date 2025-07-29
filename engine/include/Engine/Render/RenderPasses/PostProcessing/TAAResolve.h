#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <string_view>

namespace Render {

class TAAResolvePass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;
    static constexpr std::string_view name = "TAA Resolve";

    struct Settings {
        Scene* pScene;
    } settings;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"frameBuffer">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useResource<"depth">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useResource<"velocity">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useResource<"history">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useResource<"historyDepth">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useRenderTarget<"output">();
    }
    void initialize(const Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<TAAResolvePass>& registry, const FrameGraphExecuteArgs& args);
    void displayGUI();

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;

    inline static float m_alpha = 0.1f;
};

}
