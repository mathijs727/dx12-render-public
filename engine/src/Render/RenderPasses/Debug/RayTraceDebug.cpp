#include "Engine/Render/RenderPasses/Debug/RayTraceDebug.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/RayTraceDebug.h"
#include "Engine/Render/ShaderInputs/inputlayouts/ComputeLayout.h"
#include "Engine/RenderAPI/RenderAPI.h"

namespace Render {

void RayTraceDebugPass::execute(const FrameGraphRegistry<RayTraceDebugPass>& registry, const FrameGraphExecuteArgs& args)
{
    settings.pScene->transitionVertexBuffers(args.pCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    ShaderInputs::RayTraceDebug inputs;
    inputs.setCamera(getRayTracingCamera(settings.pScene->camera));
    inputs.setNumThreads(registry.getTextureResolution<"out">());
    inputs.setAccelerationStructure(settings.pScene->tlasBinding());
    inputs.setOutput(registry.getTextureUAV<"out">());
    const auto compiledInputs = inputs.generateTransientBindings(*args.pRenderContext);

    const auto numThreadGroups = registry.getTextureResolution<"out">() / glm::uvec2(8);
    args.pCommandList->SetComputeRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState(m_pPipelineState.Get());

    ShaderInputs::ComputeLayout::bindMainCompute(args.pCommandList, compiledInputs);
    args.pCommandList->Dispatch(numThreadGroups.x, numThreadGroups.y, 1);
}

void RayTraceDebugPass::initialize(const Render::RenderContext& renderContext)
{
    const auto shader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Debug/debug_rt_cs.dxil");
    m_pRootSignature = ShaderInputs::ComputeLayout::getRootSignature(renderContext.pDevice.Get());
    const D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc {
        .pRootSignature = m_pRootSignature.Get(),
        .CS = shader
    };
    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
    m_pPipelineState->SetName(L"PSO RayTraceDebugPass");
}

}
