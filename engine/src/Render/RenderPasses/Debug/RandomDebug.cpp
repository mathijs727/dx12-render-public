#include "Engine/Render/RenderPasses/Debug/RandomDebug.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/ShaderInputs/inputgroups/RandomDebug.h"
#include "Engine/Render/ShaderInputs/inputlayouts/ComputeLayout.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec2.hpp>
DISABLE_WARNINGS_POP()
#include <fmt/format.h>
#include <memory>

namespace Render {
void RandomDebugPass::execute(const FrameGraphRegistry<RandomDebugPass>& resources, const FrameGraphExecuteArgs& args)
{
    ShaderInputs::RandomDebug inputs {};
    inputs.setSeed(10108870980642855433ull);
    inputs.setTextureResolution(resources.getTextureResolution<"out">());
    inputs.setOutTexture(resources.getTextureUAV<"out">());
    const auto compiledInputs = inputs.generateTransientBindings(*args.pRenderContext);

    const auto numThreadGroups = resources.getTextureResolution<"out">() / glm::uvec2(8);
    args.pCommandList->SetComputeRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState(m_pPipelineState.Get());

    ShaderInputs::ComputeLayout::bindMainCompute(args.pCommandList, compiledInputs);
    args.pCommandList->Dispatch(numThreadGroups.x, numThreadGroups.y, 1);
}

void RandomDebugPass::initialize(const Render::RenderContext& renderContext)
{
    const auto shader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Debug/debug_random_cs.dxil");
    m_pRootSignature = ShaderInputs::ComputeLayout::getRootSignature(renderContext.pDevice.Get());
    const D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc { .pRootSignature = m_pRootSignature.Get(), .CS = shader };
    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
}
}
