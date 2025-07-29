#include "Engine/Render/RenderPasses/Debug/RasterDebug.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Debug/VisualDebug.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/RasterDebug.h"
#include "Engine/Render/ShaderInputs/inputgroups/StaticMeshVertex.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <Tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <ImGui.h>
DISABLE_WARNINGS_POP()

namespace Render {

void RasterDebugPass::initialize(const Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Shared/static_mesh_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Debug/debug_ps.dxil");

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
}

void RasterDebugPass::execute(const FrameGraphRegistry<RasterDebugPass>& resources, const FrameGraphExecuteArgs& args)
{
    auto pCommandList = args.pCommandList;
    setViewportAndScissor(pCommandList, resources.getTextureResolution<"framebuffer">());
    pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    pCommandList->SetPipelineState(m_pPipelineState.Get());

    settings.pScene->transitionVertexBuffers(pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ShaderInputs::RasterDebug passInputs {};
    if (settings.pVisualDebugPass) {
        passInputs.setDebugPixel(settings.pScene->mouseCursorPosition);
        passInputs.setVisualDebug(settings.pVisualDebugPass->getShaderInputs());
    } else {
        passInputs.setDebugPixel(glm::ivec2(-1));
    }
    const auto compiledPassInputs = passInputs.generateTransientBindings(*args.pRenderContext);
    ShaderInputs::DefaultLayout::bindPassGraphics(pCommandList, compiledPassInputs);

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

}
