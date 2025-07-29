#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/Render/ShaderInputs/groups/PrintSink.h"
#include "Engine/RenderAPI/Internal/D3D12MAHelpers.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/ShaderInput.h"

namespace Render {

class PrintfPass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Compute;

    struct Settings {
        uint32_t bufferSizeInBytes = 10 * 1024 * 1024;
    } settings;

    static consteval void declareFrameResources(RenderPassBuilder& builder) { }

    void initialize(RenderContext& renderContext);
    void execute(const FrameGraphRegistry<PrintfPass>& registry, const FrameGraphExecuteArgs& args);
    void displayGUI();

    ShaderInputs::PrintSink getShaderInputs() const;

private:
    bool m_paused { false };
    uint32_t m_currentFrameIdx { 0 };

    RenderAPI::D3D12MAResource m_pPrintBuffer;
    std::vector<RenderAPI::D3D12MAResource> m_readBackBuffers;
    ShaderInputs::PrintSink m_shaderInputs;
};

}
