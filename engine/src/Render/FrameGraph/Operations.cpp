#include "Engine/Render/FrameGraph/Operations.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/ShaderInputs/inputgroups/CopyChannelRaster.h"
#include "Engine/Render/ShaderInputs/inputgroups/CopyRaster.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/type_ptr.hpp>
DISABLE_WARNINGS_POP()

namespace Render {

void ClearFrameBuffer::execute(const FrameGraphRegistry<ClearFrameBuffer>& registry, const FrameGraphExecuteArgs& args)
{
    const auto& resource = registry.getInternalResource<"buffer">();
    auto& renderContext = *args.pRenderContext;

    const auto rtvDescriptor = renderContext.rtvDescriptorAllocator.allocate(1);
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {
        .Format = resource.desc.Format,
        .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D
    };
    rtvDesc.Texture2D.MipSlice = rtvDesc.Texture2D.PlaneSlice = 0;
    renderContext.pDevice->CreateRenderTargetView(resource.pResource.Get(), &rtvDesc, rtvDescriptor);
    args.pCommandList->ClearRenderTargetView(rtvDescriptor, glm::value_ptr(settings.clearColor), 0, nullptr);
}

void ClearDepthBuffer::execute(const FrameGraphRegistry<ClearDepthBuffer>& registry, const FrameGraphExecuteArgs& args)
{
    const auto& resource = registry.getInternalResource<"buffer">();
    auto& renderContext = *args.pRenderContext;

    const auto dsvDescriptor = renderContext.dsvDescriptorAllocator.allocate(1);
    const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {
        .Format = resource.dsvFormat,
        .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
        .Flags = D3D12_DSV_FLAG_NONE,
        .Texture2D = D3D12_TEX2D_DSV {}
    };
    renderContext.pDevice->CreateDepthStencilView(resource.pResource.Get(), &dsvDesc, dsvDescriptor);
    args.pCommandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, settings.depthValue, 0, 0, nullptr);
}

void CopyTexture::execute(const FrameGraphRegistry<CopyTexture>& registry, const FrameGraphExecuteArgs& args)
{
    const auto& sourceResource = registry.getInternalResource<"source">();
    const auto& destResource = registry.getInternalResource<"dest">();
    Util::AssertEQ(sourceResource.desc.Dimension, D3D12_RESOURCE_DIMENSION_TEXTURE2D);
    Util::AssertEQ(destResource.desc.Dimension, D3D12_RESOURCE_DIMENSION_TEXTURE2D);
    Util::AssertEQ(sourceResource.desc.Width, sourceResource.desc.Width);
    Util::AssertEQ(sourceResource.desc.Height, sourceResource.desc.Height);
    const CD3DX12_TEXTURE_COPY_LOCATION srcTexLocation { sourceResource.pResource.Get() };
    const CD3DX12_TEXTURE_COPY_LOCATION dstTexLocation { destResource.pResource.Get() };

    const CD3DX12_BOX copyDims { 0, 0, (LONG)sourceResource.desc.Width, (LONG)sourceResource.desc.Height };
    args.pCommandList->CopyTextureRegion(&dstTexLocation, 0, 0, 0, &srcTexLocation, &copyDims);
}

void ShaderCopyTexture::initialize(const RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Util/full_screen_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Util/copy_ps.dxil");

    m_pRootSignature = ShaderInputs::DefaultLayout::getRootSignature(renderContext.pDevice.Get());
    setFullScreenPassPipelineState(pipelineStateDesc);
    pipelineStateDesc.pRootSignature = m_pRootSignature.Get();
    pipelineStateDesc.VS = vertexShader;
    pipelineStateDesc.PS = pixelShader;

    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
    m_pPipelineState->SetName(L"Shader Copy Texture");
}

void ShaderCopyTexture::execute(const FrameGraphRegistry<ShaderCopyTexture>& registry, const FrameGraphExecuteArgs& args)
{
    ShaderInputs::CopyRaster inputs {};
    inputs.setInTexture(registry.getTextureSRV<"source">());
    auto bindings = inputs.generateTransientBindings(*args.pRenderContext);

    args.pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState(m_pPipelineState.Get());
    args.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ShaderInputs::DefaultLayout::bindPassGraphics(args.pCommandList, bindings);
    args.pCommandList->DrawInstanced(3, 1, 0, 0);
}

void CopyTextureChannels::initialize(const RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Util/full_screen_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Util/copy_channels_ps.dxil");

    m_pRootSignature = ShaderInputs::DefaultLayout::getRootSignature(renderContext.pDevice.Get());
    setFullScreenPassPipelineState(pipelineStateDesc);
    pipelineStateDesc.pRootSignature = m_pRootSignature.Get();
    pipelineStateDesc.VS = vertexShader;
    pipelineStateDesc.PS = pixelShader;

    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
    m_pPipelineState->SetName(L"Shader Copy Texture");
}

void CopyTextureChannels::execute(const FrameGraphRegistry<CopyTextureChannels>& registry, const FrameGraphExecuteArgs& args)
{
    ShaderInputs::CopyChannelRaster inputs {};
    inputs.setInTexture(registry.getTextureSRV<"source">());
    inputs.setR(settings.r);
    inputs.setG(settings.g);
    inputs.setB(settings.b);
    inputs.setOffset(settings.offset);
    inputs.setScaling(settings.scaling);
    auto bindings = inputs.generateTransientBindings(*args.pRenderContext);

    args.pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState(m_pPipelineState.Get());
    args.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ShaderInputs::DefaultLayout::bindPassGraphics(args.pCommandList, bindings);
    args.pCommandList->DrawInstanced(3, 1, 0, 0);
}

}