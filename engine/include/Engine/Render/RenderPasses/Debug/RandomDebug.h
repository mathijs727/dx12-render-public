#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/RenderAPI.h"

namespace Render {

class RandomDebugPass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Compute;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"out">(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    void initialize(const Render::RenderContext& renderContext);
    void execute(const FrameGraphRegistry<RandomDebugPass>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
};

}
