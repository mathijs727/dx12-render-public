#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/RenderAPI.h"

namespace Render {

class RayTracePipelineDebugPass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Compute;

    struct Settings {
        Scene* pScene;
    } settings;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"out">(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    };
    void initialize(RenderContext& renderContext);
    void execute(const FrameGraphRegistry<RayTracePipelineDebugPass>& registry, const FrameGraphExecuteArgs& args);

private:
    void buildShaderBindingTable(RenderContext& renderContext);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12StateObject> m_pPipelineStateObject;

    RenderAPI::D3D12MAResource m_shaderBindingTable;
    RenderAPI::ShaderBindingTableInfo m_shaderBindingTableInfo;
};

}
