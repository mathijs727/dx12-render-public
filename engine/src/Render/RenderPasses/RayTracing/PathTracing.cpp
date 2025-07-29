#include "Engine/Render/RenderPasses/RayTracing/PathTracing.h"
#include <cstring>
#include "Engine/Render/Camera.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Debug/VisualDebug.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/PathTracing.h"
#include "Engine/Render/ShaderInputs/inputlayouts/RayTraceGlobalLayout.h"
#include "Engine/Render/ShaderInputs/inputlayouts/RayTraceLocalLayout.h"
#include "Engine/Render/ShaderInputs/structs/DirectionalLight.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/ShaderBindingTableBuilder.h"
#include "Engine/RenderAPI/StateObjectBuilder.h"
#include "Engine/Util/Align.h"

namespace Render {

void PathTracingPass::execute(const FrameGraphRegistry<PathTracingPass>& resources, const FrameGraphExecuteArgs& args)
{
    const auto& scene = *settings.pScene;

    settings.pScene->transitionVertexBuffers(args.pCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    const auto rtCamera = getRayTracingCamera(scene.camera);

    ShaderInputs::PathTracing inputs;
    if (settings.pVisualDebugPass) {
        inputs.setDebugPixel(scene.mouseCursorPosition);
        inputs.setVisualDebug(settings.pVisualDebugPass->getShaderInputs());
    }
    inputs.setAccelerationStructure(scene.tlasBinding());
    inputs.setNumThreads(resources.getTextureResolution<"out">());
    inputs.setRandomSeed(m_rng());
    inputs.setCamera(rtCamera);
    inputs.setOutput(resources.getTextureUAV<"out">());
    bool cameraChanged = std::memcmp(&rtCamera, &m_previousFrameCamera, sizeof(rtCamera)) != 0;
    inputs.setOverwriteOutput(cameraChanged);
    if (cameraChanged)
        sampleCount = 0; // Reset sample count if camera changed.
    m_previousFrameCamera = rtCamera;

    inputs.setSun(scene.sun);
    if (scene.optEnvironmentMap) {
        const auto& envMap = scene.optEnvironmentMap;
        inputs.setEnvironmentMapStrength(envMap->strength);
        // inputs.setEnvironmentMap(scene.textures[envMap->textureIdx]);
        inputs.setEnvironmentMap(envMap->texture);
    } else {
        inputs.setEnvironmentMapStrength(0.0f);
    }

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

    ++sampleCount;
}

void PathTracingPass::initialize(RenderContext& renderContext)
{
    const auto pathTracingShader = Render::loadEngineShader(renderContext.pDevice.Get(),
        settings.pVisualDebugPass ? "Engine/RayTracing/path_tracing_debug_lib.dxil" : "Engine/RayTracing/path_tracing_lib.dxil");
    const auto pRootSignature = ShaderInputs::RayTraceGlobalLayout::getRootSignature(renderContext.pDevice.Get());
    const auto pLocalRootSignature = ShaderInputs::RayTraceLocalLayout::getRootSignature(renderContext.pDevice.Get());

    RenderAPI::StateObjectBuilder stateObjectBuilder {};
    stateObjectBuilder.addLibrary(pathTracingShader, L"rayGen");
    stateObjectBuilder.addLibrary(pathTracingShader, L"miss");
    stateObjectBuilder.addLibrary(pathTracingShader, L"shadowMiss");
    stateObjectBuilder.addLibrary(pathTracingShader, L"pbrClosestHit");
    stateObjectBuilder.addLibrary(pathTracingShader, L"pbrAnyHit");

    stateObjectBuilder.addRayGenShader(L"rayGen");
    stateObjectBuilder.addMissShader(L"miss");
    stateObjectBuilder.addMissShader(L"shadowMiss");
    stateObjectBuilder.addHitGroup(L"pbrAlphaTestedHitGroup", { .anyHit = L"pbrAnyHit", .closestHit = L"pbrClosestHit" }, pLocalRootSignature.Get());
    stateObjectBuilder.addHitGroup(L"pbrHitGroup", { .closestHit = L"pbrClosestHit" }, pLocalRootSignature.Get());

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig {
        .MaxPayloadSizeInBytes = 32,
        .MaxAttributeSizeInBytes = 16
    };
    D3D12_RAYTRACING_PIPELINE_CONFIG1 pipelineConfig {
        .MaxTraceRecursionDepth = 5,
        .Flags = D3D12_RAYTRACING_PIPELINE_FLAG_NONE
    };

    auto pPipelineStateObject = stateObjectBuilder.compile(
        renderContext.pDevice.Get(), pRootSignature.Get(), shaderConfig, pipelineConfig);
    m_pRootSignature = pRootSignature;
    m_pPipelineStateObject = pPipelineStateObject;

    buildShaderBindingTable(renderContext);
}

void PathTracingPass::buildShaderBindingTable(RenderContext& renderContext)
{
    RenderAPI::ShaderBindingTableBuilder sbtBuilder { m_pPipelineStateObject.Get() };
    sbtBuilder.addRayGenerationProgram(L"rayGen");
    sbtBuilder.addMissProgram(L"miss");
    sbtBuilder.addMissProgram(L"shadowMiss");
    const auto& scene = *settings.pScene;
    for (const auto& instance : scene.meshInstances) {
        const auto& mesh = scene.meshes[instance.meshIdx];
        for (uint32_t subMeshIdx = 0; subMeshIdx < mesh.subMeshes.size(); ++subMeshIdx) {
            const auto& material = mesh.materials[subMeshIdx];
            const auto& subMeshProperties = mesh.subMeshProperties[subMeshIdx];
            sbtBuilder.setHitGroup(
                instance.instanceContributionToHitGroupIndex + subMeshIdx,
                material.isOpague ? L"pbrHitGroup" : L"pbrAlphaTestedHitGroup",
                ShaderInputs::RayTraceLocalLayout::getShaderBindings(material.shaderInputs, subMeshProperties));
        }
    }

    const auto [sbtCPU, sbtInfo] = sbtBuilder.compile();
    m_shaderBindingTableInfo = sbtInfo;
    m_shaderBindingTable = renderContext.createBufferWithArrayData<std::byte>(sbtCPU, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

}
