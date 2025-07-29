#include "GPU.h"
#include "ShaderInputs/inputlayouts/TestComputeLayout.h"
#include <Engine/Render/RenderContext.h>
#include <Engine/RenderAPI/Internal/D3D12Includes.h>
#include <Engine/RenderAPI/Shader.h>
#include <Engine/RenderAPI/ShaderInput.h>
#include <filesystem>

static RenderAPI::Shader loadEngineShader(ID3D12Device5* pDevice, const std::filesystem::path& filePath)
{
    return RenderAPI::loadShader(pDevice, "shaders" / filePath);
}

RenderAPI::PipelineState createComputePipeline(const Render::RenderContext& renderContext, const std::filesystem::path& shaderFilePath)
{
    // Load the shader and create a pipeline state object.
    const auto shader = loadEngineShader(renderContext.pDevice.Get(), shaderFilePath);

    RenderAPI::PipelineState out;
    out.pRootSignature = ShaderInputs::TestComputeLayout::getRootSignature(renderContext.pDevice.Get());
    const D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc { .pRootSignature = out.pRootSignature.Get(), .CS = shader };
    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&out.pPipelineState)));
    return out;
}

void setPipelineState(const WRL::ComPtr<ID3D12GraphicsCommandList6>& pCommandList, const RenderAPI::PipelineState& pipelineState)
{
    pCommandList->SetComputeRootSignature(pipelineState.pRootSignature.Get());
    pCommandList->SetPipelineState(pipelineState.pPipelineState.Get());
}
void setDescriptorHeaps(const WRL::ComPtr<ID3D12GraphicsCommandList6>& pCommandList, const Render::RenderContext& renderContext)
{
    const std::array descriptorHeaps {
        renderContext.pCbvSrvUavDescriptorBaseAllocatorGPU->pDescriptorHeap.Get()
    };
    pCommandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());
}