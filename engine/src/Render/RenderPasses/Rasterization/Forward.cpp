#include "Engine/Render/RenderPasses/Rasterization/Forward.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/Forward.h"
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

template <bool SupportTAA>
void ForwardPass<SupportTAA>::execute(const FrameGraphRegistry<ForwardPass<SupportTAA>>& resources, const FrameGraphExecuteArgs& args)
{
    auto pCommandList = args.pCommandList;
    const auto resolution = resources.getTextureResolution<"framebuffer">();
    // setViewportAndScissor(pCommandList, );
    pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    pCommandList->SetPipelineState(m_pPipelineState.Get());

    settings.pScene->transitionVertexBuffers(pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ShaderInputs::Forward forwardInputs;
    forwardInputs.setCameraPosition(settings.pScene->camera.transform.position);
    forwardInputs.setSun(settings.pScene->sun);
    const auto compiledForwardInputs = forwardInputs.generateTransientBindings(*args.pRenderContext);
    ShaderInputs::DefaultLayout::bindPassGraphics(pCommandList, compiledForwardInputs);

    // Screen ranges from [-1, +1]
    const auto cameraJitterTAA = SupportTAA ? (2.0f * settings.pScene->cameraJitterTAA - 1.0f) * m_taaJitter / glm::vec2(resolution) : glm::vec2(0);
    const auto jitterMatrix = glm::translate(glm::identity<glm::mat4>(), glm::vec3(cameraJitterTAA.x, cameraJitterTAA.y, 0));
    const auto viewProjectionMatrix = settings.pScene->camera.projectionMatrix() * settings.pScene->camera.transform.viewMatrix();
    const auto jitteredViewProjectionMatrix = jitterMatrix * viewProjectionMatrix;
    const auto lastFrameViewProjectionMatrix = settings.pScene->camera.projectionMatrix() * settings.pScene->camera.previousTransform.viewMatrix();
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const auto& instance : settings.pScene->meshInstances) {
        const auto modelMatrix = instance.transform.matrix();
        if constexpr (SupportTAA) {
            ShaderInputs::StaticMeshTAAVertex instanceInput {};
            instanceInput.setJitteredModelViewProjectionMatrix(jitteredViewProjectionMatrix * modelMatrix);
            instanceInput.setModelViewProjectionMatrix(viewProjectionMatrix * modelMatrix);
            instanceInput.setLastFrameModelViewProjectionMatrix(lastFrameViewProjectionMatrix * instance.previousTransform.matrix());
            instanceInput.setModelMatrix(modelMatrix);
            instanceInput.setModelNormalMatrix(instance.transform.normalMatrix());
            const auto compiledInstanceInputs = instanceInput.generateTransientBindings(*args.pRenderContext);
            ShaderInputs::DefaultLayout::bindInstanceGraphics(pCommandList, compiledInstanceInputs);
        } else {
            ShaderInputs::StaticMeshVertex instanceInput {};
            instanceInput.setModelViewProjectionMatrix(viewProjectionMatrix * modelMatrix);
            instanceInput.setModelMatrix(modelMatrix);
            instanceInput.setModelNormalMatrix(instance.transform.normalMatrix());
            const auto compiledInstanceInputs = instanceInput.generateTransientBindings(*args.pRenderContext);
            ShaderInputs::DefaultLayout::bindInstanceGraphics(pCommandList, compiledInstanceInputs);
        }

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

template <bool SupportTAA>
void ForwardPass<SupportTAA>::initialize(Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), SupportTAA ? "Engine/Shared/static_mesh_taa_vs.dxil" : "Engine/Shared/static_mesh_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), SupportTAA ? "Engine/Rasterization/forward_taa_ps.dxil" : "Engine/Rasterization/forward_ps.dxil");

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
    m_pPipelineState->SetName(L"PSO Forward");
}

template <bool SupportTAA>
void ForwardPass<SupportTAA>::displayGUI()
{
    if constexpr (SupportTAA)
        ImGui::SliderFloat("TAA Jitter Amplitude", &m_taaJitter, 0.0f, 1.0f);
}

template class ForwardPass<true>;
template class ForwardPass<false>;

}