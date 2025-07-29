#include "Engine/Render/RenderPasses/Rasterization/ForwardShadowRT.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/ForwardShadowRT.h"
#include "Engine/Render/ShaderInputs/inputgroups/StaticMeshTAAVertex.h"
#include "Engine/Render/ShaderInputs/inputgroups/StaticMeshVertex.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/Render/ShaderInputs/structs/DirectionalLight.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>
#include <imgui.h>
DISABLE_WARNINGS_POP()
#include <array>
#include <vector>

namespace Render {

void ForwardShadowRTPass::execute(const FrameGraphRegistry<ForwardShadowRTPass>& resources, const FrameGraphExecuteArgs& args)
{
    auto pCommandList = args.pCommandList;
    pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    pCommandList->SetPipelineState(m_pPipelineState.Get());

    settings.pScene->transitionVertexBuffers(pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    const auto resolution = resources.getTextureResolution<"framebuffer">();
    ShaderInputs::ForwardShadowRT forwardInputs;
    forwardInputs.setAccelerationStructure(settings.pScene->tlasBinding());
    forwardInputs.setCameraPosition(settings.pScene->camera.transform.position);
    forwardInputs.setExposure(1.0f);
    forwardInputs.setSun(settings.pScene->sun);
    forwardInputs.setRandomSeed(std::hash<uint32_t>()(m_frameIndex++));
    forwardInputs.setViewportSize(resolution);
    const auto compiledForwardInputs = forwardInputs.generateTransientBindings(*args.pRenderContext);
    ShaderInputs::DefaultLayout::bindPassGraphics(pCommandList, compiledForwardInputs);

    // Screen ranges from [-1, +1]
    const auto viewMatrix = settings.pScene->camera.transform.viewMatrix();
    const auto viewProjectionMatrix = settings.pScene->camera.projectionMatrix() * viewMatrix;
    const auto lastFrameViewProjectionMatrix = settings.pScene->camera.projectionMatrix() * settings.pScene->camera.previousTransform.viewMatrix();
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    uint32_t drawID = 0;
    for (const auto& instance : settings.pScene->meshInstances) {
        const auto modelMatrix = instance.transform.matrix();
        ShaderInputs::StaticMeshVertex instanceInput {};
        instanceInput.setModelMatrix(modelMatrix);
        instanceInput.setModelNormalMatrix(instance.transform.normalMatrix());
        instanceInput.setModelViewMatrix(viewMatrix * modelMatrix);
        instanceInput.setModelViewProjectionMatrix(viewProjectionMatrix * modelMatrix);
        const auto compiledInstanceInputs = instanceInput.generateTransientBindings(*args.pRenderContext);
        ShaderInputs::DefaultLayout::bindInstanceGraphics(pCommandList, compiledInstanceInputs);

        const auto& mesh = settings.pScene->meshes[instance.meshIdx];
        pCommandList->IASetIndexBuffer(&mesh.indexBufferView);
        pCommandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);

        for (size_t i = 0; i < mesh.subMeshes.size(); ++i) {
            const auto& subMesh = mesh.subMeshes[i];
            const auto& material = mesh.materials[i];
            ShaderInputs::DefaultLayout::bindMaterialGraphics(pCommandList, material.shaderInputs);
            pCommandList->SetGraphicsRoot32BitConstant(
                ShaderInputs::DefaultLayout::getDrawIDRootParameterIndex(), drawID++, 0);
            pCommandList->DrawIndexedInstanced(subMesh.numIndices, 1, subMesh.indexStart, subMesh.baseVertex, 0);
        }
    }
}

void ForwardShadowRTPass::initialize(Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Shared/static_mesh_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Rasterization/forward_shadow_rt_ps.dxil");

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
    m_pPipelineState->SetName(L"PSO Forward Shadow RT");
}

void ForwardShadowRTPass::displayGUI()
{
    ImGui::SliderFloat("TAA Jitter Amplitude", &m_taaJitter, 0.0f, 1.0f);
}

}