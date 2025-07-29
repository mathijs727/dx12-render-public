#include "Engine/Render/RenderPasses/Rasterization/VisibilityBuffer.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/RenderPasses/Util/Printf.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/StaticMeshTAAVertex.h"
#include "Engine/Render/ShaderInputs/inputgroups/StaticMeshVertex.h"
#include "Engine/Render/ShaderInputs/inputgroups/VisiblityRender.h"
#include "Engine/Render/ShaderInputs/inputgroups/VisiblityToGBuffer.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/Render/ShaderInputs/structs/DirectionalLight.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/Util/ImguiHelpers.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>
#include <imgui.h>
#include <itertools.hpp>
DISABLE_WARNINGS_POP()
#include <array>
#include <cassert>
#include <vector>

namespace Render {

void VisiblityBufferRenderPass::initialize(RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Shared/static_mesh_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Rasterization/visibility_buffer_render_ps.dxil");

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
    m_pPipelineState->SetName(L"PSO VisiblityBufferRender");
}

void VisiblityBufferRenderPass::execute(const FrameGraphRegistry<VisiblityBufferRenderPass>& resources, const FrameGraphExecuteArgs& args)
{
    auto pCommandList = args.pCommandList;
    const auto resolution = resources.getTextureResolution<"visibilityBuffer">();
    pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    pCommandList->SetPipelineState(m_pPipelineState.Get());

    settings.pScene->transitionVertexBuffers(pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ShaderInputs::VisiblityRender passInputs;
    passInputs.setPrintSink(settings.pDebugPrintPass->getShaderInputs());
    const auto compiledPassInputs = passInputs.generateTransientBindings(*args.pRenderContext);
    ShaderInputs::DefaultLayout::bindPassGraphics(pCommandList, compiledPassInputs);

    const auto viewMatrix = settings.pScene->camera.transform.viewMatrix();
    const auto viewProjectionMatrix = settings.pScene->camera.projectionMatrix() * viewMatrix;
    const auto lastFrameViewProjectionMatrix = settings.pScene->camera.projectionMatrix() * settings.pScene->camera.previousTransform.viewMatrix();
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const auto& [instanceID, instance] : iter::enumerate(settings.pScene->meshInstances)) {
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
        for (uint32_t subMeshIdx = 0; subMeshIdx < mesh.subMeshes.size(); ++subMeshIdx) {
            const auto& subMesh = mesh.subMeshes[subMeshIdx];
            assert(subMeshIdx < 0xFFFF);
            const uint32_t drawID = ((uint32_t)instanceID << 16) | subMeshIdx;
            pCommandList->SetGraphicsRoot32BitConstant(
                ShaderInputs::DefaultLayout::getDrawIDRootParameterIndex(), drawID, 0);
            pCommandList->DrawIndexedInstanced(subMesh.numIndices, 1, subMesh.indexStart, subMesh.baseVertex, 0);
        }
    }
}

void VisiblityToGBufferPass::initialize(const Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Util/full_screen_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Rasterization/visibility_to_gbuffer_ps.dxil");

    m_pRootSignature = ShaderInputs::DefaultLayout::getRootSignature(renderContext.pDevice.Get());
    setFullScreenPassPipelineState(pipelineStateDesc);
    pipelineStateDesc.pRootSignature = m_pRootSignature.Get();
    pipelineStateDesc.VS = vertexShader;
    pipelineStateDesc.PS = pixelShader;

    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
    m_pPipelineState->SetName(L"PSO Visibility To GBuffer");
}

void VisiblityToGBufferPass::execute(const FrameGraphRegistry<VisiblityToGBufferPass>& resources, const FrameGraphExecuteArgs& args)
{
    const glm::vec2 invResolution = 1.0f / glm::vec2(resources.getTextureResolution<"baseColor">());
    const auto rtCamera = getRayTracingCamera(settings.pScene->camera);
    std::array pixelToRayDirectionWorldSpace {
        2.0f * invResolution.x * rtCamera.screenU, // Image plane ranges from [-u, +u]
        2.0f * invResolution.y * rtCamera.screenV, // Image plane ranges from [-v, +v]
    };

    ShaderInputs::VisiblityToGBuffer inputs;
    inputs.setPrintSink(settings.pDebugPrintPass->getShaderInputs());
    inputs.setVisibilityBuffer(resources.getTextureSRV<"visibilityBuffer">());
    inputs.setCamera(rtCamera);
    inputs.setPixelToRayDirectionWorldSpace(pixelToRayDirectionWorldSpace);
    inputs.setInvResolution(invResolution);
    inputs.setOutputType(static_cast<int>(m_displayType));
    inputs.setRayDifferentialsChristoph(m_rayDifferentialsChristoph);
    auto bindings = inputs.generateTransientBindings(*args.pRenderContext);

    args.pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState(m_pPipelineState.Get());
    args.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ShaderInputs::DefaultLayout::bindPassGraphics(args.pCommandList, settings.pScene->bindlessScene);
    ShaderInputs::DefaultLayout::bindInstanceGraphics(args.pCommandList, bindings);
    args.pCommandList->DrawInstanced(3, 1, 0, 0);
}

void VisiblityToGBufferPass::displayGUI()
{
    Util::imguiCombo("Texture Sampling", m_displayType);
    ImGui::Checkbox("RD Christoph", &m_rayDifferentialsChristoph);
}

}