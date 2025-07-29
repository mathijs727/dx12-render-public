#pragma once
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <string_view>

namespace Render {

enum class ToneMappingFunction {
    None,
    Uncharted2,
    ACES,
    AGX
};

class ColorCorrectionPass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;
    static constexpr std::string_view name = "Color Correction";

    struct Settings {
        uint32_t const* pSampleCount { nullptr }; // Pointer to the sample count, can be used for adaptive sampling.
    } settings;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"input">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useRenderTarget<"output">();
    }

    void initialize(const Render::RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<ColorCorrectionPass>& registry, const FrameGraphExecuteArgs& args);
    void displayGUI();

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;

    inline static ToneMappingFunction m_toneMappingFunction = ToneMappingFunction::ACES;
    inline static bool m_enableWhitePoint = true;
    inline static bool m_enableGammaCorrection = true;
};

}
