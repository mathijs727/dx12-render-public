#include "Engine/Render/RenderPasses/PostProcessing/ColorCorrection.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/ShaderInputs/inputgroups/ColorCorrection.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/Util/ImguiHelpers.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <imgui.h>
#include <magic_enum/magic_enum.hpp>
DISABLE_WARNINGS_POP()

using namespace RenderAPI;

namespace Render {

void ColorCorrectionPass::execute(const FrameGraphRegistry<ColorCorrectionPass>& resources, const FrameGraphExecuteArgs& args)
{
    ShaderInputs::ColorCorrection inputs {};
    inputs.setLinearFrameBuffer(resources.getTextureSRV<"input">());
    inputs.setInvSampleCount(settings.pSampleCount ? 1.0f / float(*settings.pSampleCount) : 1.0f);
    inputs.setToneMappingFunction((int)*magic_enum::enum_index(m_toneMappingFunction));
    inputs.setEnableWhitePoint(m_enableWhitePoint);
    inputs.setEnableGammaCorrection(m_enableGammaCorrection);
    auto bindings = inputs.generateTransientBindings(*args.pRenderContext);

    args.pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState(m_pPipelineState.Get());
    args.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ShaderInputs::DefaultLayout::bindPassGraphics(args.pCommandList, bindings);
    args.pCommandList->DrawInstanced(3, 1, 0, 0);
}

void ColorCorrectionPass::displayGUI()
{
    Util::imguiCombo("Tone Mapping", m_toneMappingFunction);
    if (m_toneMappingFunction == ToneMappingFunction::Uncharted2)
        ImGui::Checkbox("White Point", &m_enableWhitePoint);
    ImGui::Checkbox("Gamma Correction", &m_enableGammaCorrection);
}

void ColorCorrectionPass::initialize(const Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Util/full_screen_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/PostProcessing/color_correction_ps.dxil");

    m_pRootSignature = ShaderInputs::DefaultLayout::getRootSignature(renderContext.pDevice.Get());
    setFullScreenPassPipelineState(pipelineStateDesc);
    pipelineStateDesc.pRootSignature = m_pRootSignature.Get();
    pipelineStateDesc.VS = vertexShader;
    pipelineStateDesc.PS = pixelShader;

    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
    m_pPipelineState->SetName(L"PSO Color Correction");
}

}
