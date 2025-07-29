#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/Render/RenderPasses/ForwardDeclares.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include <string_view>
#include "Engine/Render/ShaderInputs/structs/RTScreenCamera.h"
#include <random>

namespace Render {

class PathTracingPass {
public:
    struct Settings {
        Render::Scene* pScene;
        VisualDebugPass const* pVisualDebugPass { nullptr };
    } settings;

    uint32_t sampleCount = 1; // Number of samples per pixel

public:
    static constexpr RenderPassType renderPassType = RenderPassType::RayTracing;
    static constexpr std::string_view name = "Path Tracing";

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"out">(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    };

public:
    void initialize(RenderContext& renderContext);
    void execute(const FrameGraphRegistry<PathTracingPass>& registry, const FrameGraphExecuteArgs& args);

private:
    void buildShaderBindingTable(RenderContext& renderContext);

private:
    std::mt19937_64 m_rng { 12345 };
    ShaderInputs::RTScreenCamera m_previousFrameCamera {};

    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12StateObject> m_pPipelineStateObject;
    RenderAPI::D3D12MAResource m_shaderBindingTable;
    RenderAPI::ShaderBindingTableInfo m_shaderBindingTableInfo;
};

}
