#include "Engine/Render/RenderPasses/Debug/RayTracePipelineDebug.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/RayTracePipelineDebug.h"
#include "Engine/Render/ShaderInputs/inputlayouts/RayTraceGlobalLayout.h"
#include "Engine/Render/ShaderInputs/inputlayouts/RayTraceLocalLayout.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/ShaderBindingTableBuilder.h"
#include "Engine/RenderAPI/StateObjectBuilder.h"
#include "Engine/Util/Align.h"

namespace Render {

void RayTracePipelineDebugPass::execute(const FrameGraphRegistry<RayTracePipelineDebugPass>& resources, const FrameGraphExecuteArgs& args)
{
    settings.pScene->transitionVertexBuffers(args.pCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    ShaderInputs::RayTracePipelineDebug inputs;
    inputs.setCamera(getRayTracingCamera(settings.pScene->camera));
    inputs.setNumThreads(resources.getTextureResolution<"out">());
    inputs.setAccelerationStructure(settings.pScene->tlasBinding());
    inputs.setOutput(resources.getTextureUAV<"out">());
    const auto compiledInputs = inputs.generateTransientBindings(*args.pRenderContext);

    // const auto numThreadGroups = resources.getTextureResolution<"out">() / glm::uvec2(8);
    const auto outResolution = resources.getTextureResolution<"out">();
    args.pCommandList->SetComputeRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState1(m_pPipelineStateObject.Get());

    D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc {
        .Width = outResolution.x,
        .Height = outResolution.y,
        .Depth = 1
    };
    m_shaderBindingTableInfo.fillDispatchRays(
        m_shaderBindingTable->GetGPUVirtualAddress(), dispatchRaysDesc);
    ShaderInputs::RayTraceGlobalLayout::bindMainCompute(args.pCommandList, compiledInputs);
    args.pCommandList->DispatchRays(&dispatchRaysDesc);
}

void RayTracePipelineDebugPass::initialize(RenderContext& renderContext)
{
    const auto rayGenShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Debug/rt/ray_gen.dxil");
    const auto missShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Debug/rt/miss.dxil");
    const auto hitGroupShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Debug/rt/hit_group.dxil");
    const auto pRootSignature = ShaderInputs::RayTraceGlobalLayout::getRootSignature(renderContext.pDevice.Get());
    const auto pLocalRootSignature = ShaderInputs::RayTraceLocalLayout::getRootSignature(renderContext.pDevice.Get());

    RenderAPI::StateObjectBuilder stateObjectBuilder {};
    stateObjectBuilder.addLibrary(rayGenShader, L"rayGen");
    stateObjectBuilder.addLibrary(missShader, L"miss");
    stateObjectBuilder.addLibrary(hitGroupShader, L"pbrClosestHit");
    stateObjectBuilder.addLibrary(hitGroupShader, L"pbrAnyHit");

    stateObjectBuilder.addRayGenShader(L"rayGen");
    stateObjectBuilder.addMissShader(L"miss");
    stateObjectBuilder.addHitGroup(L"hitGroup1", { .anyHit = L"pbrAnyHit", .closestHit = L"pbrClosestHit" }, pLocalRootSignature.Get());

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig {
        .MaxPayloadSizeInBytes = 32,
        .MaxAttributeSizeInBytes = 16
    };
    D3D12_RAYTRACING_PIPELINE_CONFIG1 pipelineConfig {
        .MaxTraceRecursionDepth = 4,
        .Flags = D3D12_RAYTRACING_PIPELINE_FLAG_NONE
    };

    auto pPipelineStateObject = stateObjectBuilder.compile(
        renderContext.pDevice.Get(), pRootSignature.Get(), shaderConfig, pipelineConfig);
    m_pRootSignature = pRootSignature;
    m_pPipelineStateObject = pPipelineStateObject;

    buildShaderBindingTable(renderContext);
}

void RayTracePipelineDebugPass::buildShaderBindingTable(RenderContext& renderContext)
{
    RenderAPI::ShaderBindingTableBuilder sbtBuilder { m_pPipelineStateObject.Get() };
    sbtBuilder.addRayGenerationProgram(L"rayGen");
    sbtBuilder.addMissProgram(L"miss");
    for (const auto& instance : settings.pScene->meshInstances) {
        const auto& mesh = settings.pScene->meshes[instance.meshIdx];
        for (uint32_t subMeshIdx = 0; subMeshIdx < mesh.subMeshes.size(); ++subMeshIdx) {
            const auto& material = mesh.materials[subMeshIdx];
            const auto& subMeshProperties = mesh.subMeshProperties[subMeshIdx];
            sbtBuilder.setHitGroup(
                instance.instanceContributionToHitGroupIndex + subMeshIdx,
                L"hitGroup1",
                ShaderInputs::RayTraceLocalLayout::getShaderBindings(material.shaderInputs, subMeshProperties));
        }
    }

    const auto [sbtCPU, sbtInfo] = sbtBuilder.compile();
    m_shaderBindingTableInfo = sbtInfo;
    m_shaderBindingTable = renderContext.createBufferWithArrayData<std::byte>(sbtCPU, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}


}
