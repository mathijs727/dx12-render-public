#include "Engine/Render/RenderPasses/Rasterization/Deferred.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/DeferredShading.h"
#include "Engine/Render/ShaderInputs/inputgroups/StaticMeshVertex.h"
#include "Engine/Render/ShaderInputs/inputgroups/SunVisibilityRT.h"
#include "Engine/Render/ShaderInputs/inputlayouts/ComputeLayout.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/Render/ShaderInputs/structs/DirectionalLight.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/Util/Align.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>
#include <imgui.h>
DISABLE_WARNINGS_POP()
#include <array>
#include <vector>

namespace Render {

void DeferredRenderPass::execute(const FrameGraphRegistry<DeferredRenderPass>& resources, const FrameGraphExecuteArgs& args)
{
    auto pCommandList = args.pCommandList;
    const auto resolution = resources.getTextureResolution<"depthbuffer">();
    // setViewportAndScissor(pCommandList, );
    pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    pCommandList->SetPipelineState(m_pPipelineState.Get());

    settings.pScene->transitionVertexBuffers(pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    const auto viewProjectionMatrix = settings.pScene->camera.projectionMatrix() * settings.pScene->camera.transform.viewMatrix();
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const auto& instance : settings.pScene->meshInstances) {
        const auto modelMatrix = instance.transform.matrix();
        ShaderInputs::StaticMeshVertex instanceInput {};
        instanceInput.setModelViewProjectionMatrix(viewProjectionMatrix * modelMatrix);
        instanceInput.setModelMatrix(modelMatrix);
        instanceInput.setModelNormalMatrix(instance.transform.normalMatrix());
        const auto compiledInstanceInputs = instanceInput.generateTransientBindings(*args.pRenderContext);
        ShaderInputs::DefaultLayout::bindInstanceGraphics(pCommandList, compiledInstanceInputs);

        const auto& mesh = settings.pScene->meshes[instance.meshIdx];
        pCommandList->IASetIndexBuffer(&mesh.indexBufferView);
        pCommandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);

        for (size_t i = 0; i < mesh.subMeshes.size(); ++i) {
            const auto& subMesh = mesh.subMeshes[i];
            const auto& material = mesh.materials[i];
            ShaderInputs::DefaultLayout::bindMaterialGraphics(pCommandList, material.shaderInputs);
            pCommandList->DrawIndexedInstanced(subMesh.numIndices, 1, subMesh.indexStart, subMesh.baseVertex, 0);
        }
    }
}

void DeferredRenderPass::initialize(Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Shared/static_mesh_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Rasterization/deferred_render_ps.dxil");

    m_pRootSignature = ShaderInputs::DefaultLayout::getRootSignature(renderContext.pDevice.Get());

    pipelineStateDesc.pRootSignature = m_pRootSignature.Get();
    std::array<D3D12_INPUT_ELEMENT_DESC, 6> inputElements;
    setDefaultVertexLayout(pipelineStateDesc, inputElements);
    pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pipelineStateDesc.DepthStencilState.DepthEnable = true;
    pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pipelineStateDesc.VS = vertexShader;
    pipelineStateDesc.PS = pixelShader;
    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
    m_pPipelineState->SetName(L"PSO Deferred Render");
}

void DeferredShadingPass::execute(const FrameGraphRegistry<DeferredShadingPass>& resources, const FrameGraphExecuteArgs& args)
{
    ShaderInputs::DeferredShading inputs {};
    inputs.setPosition_metallic(resources.getTextureSRV<"position_metallic">());
    inputs.setNormal_alpha(resources.getTextureSRV<"normal_alpha">());
    inputs.setBaseColor(resources.getTextureSRV<"baseColor">());
    inputs.setSunVisibility(resources.getTextureSRV<"sunVisibility">());
    inputs.setCameraPosition(settings.pScene->camera.transform.position);
    inputs.setSun(settings.pScene->sun);
    auto bindings = inputs.generateTransientBindings(*args.pRenderContext);

    args.pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState(m_pPipelineState.Get());
    args.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ShaderInputs::DefaultLayout::bindPassGraphics(args.pCommandList, bindings);
    args.pCommandList->DrawInstanced(3, 1, 0, 0);
}

void DeferredShadingPass::initialize(Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Util/full_screen_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Rasterization/deferred_shading_ps.dxil");

    m_pRootSignature = ShaderInputs::DefaultLayout::getRootSignature(renderContext.pDevice.Get());
    setFullScreenPassPipelineState(pipelineStateDesc);
    pipelineStateDesc.pRootSignature = m_pRootSignature.Get();
    pipelineStateDesc.VS = vertexShader;
    pipelineStateDesc.PS = pixelShader;

    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
    m_pPipelineState->SetName(L"PSO Deferred Shading");
}

void SunVisibilityRTPass::execute(const FrameGraphRegistry<SunVisibilityRTPass>& registry, const FrameGraphExecuteArgs& args)
{
    ShaderInputs::SunVisibilityRT inputs;
    inputs.setOutput(registry.getTextureUAV<"sunVisibility">());
    inputs.setPosition_metallic(registry.getTextureSRV<"position_metallic">());
    inputs.setSun(settings.pScene->sun);
    inputs.setAccelerationStructure(settings.pScene->tlasBinding());
    inputs.setNumThreads(registry.getTextureResolution<"sunVisibility">());
    inputs.setRandomSeed(std::hash<uint32_t>()(m_frameIndex++));
    const auto compiledInputs = inputs.generateTransientBindings(*args.pRenderContext);

    const auto numThreadGroups = Util::roundUpToClosestMultiple(registry.getTextureResolution<"sunVisibility">(), glm::uvec2(8));
    args.pCommandList->SetComputeRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState(m_pPipelineState.Get());

    ShaderInputs::ComputeLayout::bindMainCompute(args.pCommandList, compiledInputs);
    args.pCommandList->Dispatch(numThreadGroups.x, numThreadGroups.y, 1);
}
void SunVisibilityRTPass::initialize(Render::RenderContext& renderContext)
{
    const auto shader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Rasterization/sun_visibility_rt_cs.dxil");
    m_pRootSignature = ShaderInputs::ComputeLayout::getRootSignature(renderContext.pDevice.Get());
    const D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc {
        .pRootSignature = m_pRootSignature.Get(),
        .CS = shader
    };
    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
    m_pPipelineState->SetName(L"PSO DeferredShadowsRTPass");
}

}