#include "Engine/Render/RenderPasses/PostProcessing/TAAResolve.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/TAAResolve.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <imgui.h>
DISABLE_WARNINGS_POP()

using namespace RenderAPI;

namespace Render {

void TAAResolvePass::execute(const FrameGraphRegistry<TAAResolvePass>& resources, const FrameGraphExecuteArgs& args)
{
    const auto viewProjectionMatrix = settings.pScene->camera.projectionMatrix() * settings.pScene->camera.transform.viewMatrix();
    const auto lastFrameViewProjectionMatrix = settings.pScene->camera.projectionMatrix() * settings.pScene->camera.previousTransform.viewMatrix();

    ShaderInputs::TAAResolve inputs {};
    inputs.setFrameBuffer(resources.getTextureSRV<"frameBuffer">());
    inputs.setDepth(resources.getTextureSRV<"depth">());
    inputs.setVelocity(resources.getTextureSRV<"velocity">());
    inputs.setHistory(resources.getTextureSRV<"history">());
    inputs.setHistoryDepth(resources.getTextureSRV<"historyDepth">());
    inputs.setResolution(resources.getTextureResolution<"history">());
    inputs.setAlpha(m_alpha);
    inputs.setInverseViewProjectionMatrix(glm::inverse(viewProjectionMatrix));
    inputs.setLastFrameInverseViewProjectionMatrix(glm::inverse(lastFrameViewProjectionMatrix));
    auto bindings = inputs.generateTransientBindings(*args.pRenderContext);

    args.pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState(m_pPipelineState.Get());
    args.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ShaderInputs::DefaultLayout::bindPassGraphics(args.pCommandList, bindings);
    args.pCommandList->DrawInstanced(3, 1, 0, 0);
}

void TAAResolvePass::displayGUI()
{
    ImGui::SliderFloat("Alpha", &m_alpha, 0.0f, 1.0f);
}

void TAAResolvePass::initialize(const Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Util/full_screen_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/PostProcessing/taa_resolve_ps.dxil");

    m_pRootSignature = ShaderInputs::DefaultLayout::getRootSignature(renderContext.pDevice.Get());
    setFullScreenPassPipelineState(pipelineStateDesc);
    pipelineStateDesc.pRootSignature = m_pRootSignature.Get();
    pipelineStateDesc.VS = vertexShader;
    pipelineStateDesc.PS = pixelShader;

    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
    m_pPipelineState->SetName(L"PSO TAA Resolve");
}

}
