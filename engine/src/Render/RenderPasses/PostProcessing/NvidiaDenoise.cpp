#include "Engine/Render/RenderPasses/PostProcessing/NvidiaDenoise.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/NvidiaDenoiseDecode.h"
#include "Engine/Render/ShaderInputs/inputlayouts/ComputeLayout.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <NRD.h>
#include <NRDDescs.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <magic_enum/magic_enum.hpp>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <array>
#include <cstring>
#include <source_location>
#include <unordered_map>

using namespace RenderAPI;

static DXGI_FORMAT convertFormat(nrd::Format nrdFormat);

namespace Render {

void NvidiaDenoisePass::execute(const FrameGraphRegistry<NvidiaDenoisePass>& resources, const FrameGraphExecuteArgs& args)
{
    const auto resolution = resources.getTextureResolution<"out_diff_radiance_hitdist">();
    const auto projectionMatrix = glm::transpose(settings.pScene->camera.projectionMatrix());
    const auto viewMatrix = glm::transpose(settings.pScene->camera.transform.viewMatrix());
    const auto prevViewMatrix = glm::transpose(settings.pScene->camera.previousTransform.viewMatrix());

    nrd::HitDistanceParameters xxx;

    nrd::CommonSettings commonSettings {};
    std::memcpy(&commonSettings.viewToClipMatrix, glm::value_ptr(projectionMatrix), sizeof(projectionMatrix));
    std::memcpy(&commonSettings.viewToClipMatrixPrev, glm::value_ptr(projectionMatrix), sizeof(projectionMatrix));
    std::memcpy(&commonSettings.worldToViewMatrix, glm::value_ptr(viewMatrix), sizeof(viewMatrix));
    std::memcpy(&commonSettings.worldToViewMatrix, glm::value_ptr(prevViewMatrix), sizeof(prevViewMatrix));
    // commonSettings.motionVectorScale = {};
    commonSettings.cameraJitter[0] = 0.0f;
    commonSettings.cameraJitter[1] = 0.0f;
    commonSettings.cameraJitterPrev[0] = 0.0f;
    commonSettings.cameraJitterPrev[1] = 0.0f;
    commonSettings.resourceSize[0] = (uint16_t)resolution.x;
    commonSettings.resourceSize[1] = (uint16_t)resolution.y;
    commonSettings.resourceSizePrev[0] = (uint16_t)resolution.x;
    commonSettings.resourceSizePrev[1] = (uint16_t)resolution.y;
    commonSettings.rectSize[0] = (uint16_t)resolution.x;
    commonSettings.rectSize[1] = (uint16_t)resolution.y;
    commonSettings.rectSizePrev[0] = (uint16_t)resolution.x;
    commonSettings.rectSizePrev[1] = (uint16_t)resolution.y;
    commonSettings.printfAt[0] = m_mouseX;
    commonSettings.printfAt[1] = m_mouseY;
    commonSettings.debug = 1.0f;
    commonSettings.frameIndex = m_frameIndex++;
    commonSettings.isMotionVectorInWorldSpace = true;
    commonSettings.enableValidation = true;
    nrd::SetCommonSettings(*m_pNrdInstance, commonSettings);

    nrd::ReblurSettings reblurSettings {};
    nrd::SetDenoiserSettings(*m_pNrdInstance, 0, (const void*)&reblurSettings);

    std::array identifiers {
        nrd::Identifier { 0 }
    };
    uint32_t numDispatchSpecs;
    nrd::DispatchDesc const* dispatchDescs;
    nrd::GetComputeDispatches(*m_pNrdInstance, identifiers.data(), (uint32_t)identifiers.size(), dispatchDescs, numDispatchSpecs);

    auto& descriptorAllocator = args.pRenderContext->getCurrentCbvSrvUavDescriptorTransientAllocator();
    const nrd::InstanceDesc nrdInstanceDesc = nrd::GetInstanceDesc(*m_pNrdInstance);
    for (uint32_t dispatchIdx = 0; dispatchIdx < numDispatchSpecs; ++dispatchIdx) {
        const nrd::DispatchDesc& nrdDispatchDesc = dispatchDescs[dispatchIdx];
        spdlog::info("Denoiser [{}] - {}", nrdDispatchDesc.identifier, nrdDispatchDesc.name);

        const auto& dispatchData = m_dispatchDatas[nrdDispatchDesc.pipelineIndex];
        args.pCommandList->SetComputeRootSignature(dispatchData.pRootSignature.Get());
        args.pCommandList->SetPipelineState(dispatchData.pPipelineState.Get());

        auto descriptors = descriptorAllocator.allocate(dispatchData.descriptorTableSize);
        for (uint32_t resourceIdx = 0; resourceIdx < nrdDispatchDesc.resourcesNum; ++resourceIdx) {
            const nrd::ResourceDesc& resourceDesc = nrdDispatchDesc.resources[resourceIdx];
            auto descriptor = descriptors.offset(resourceIdx, args.pRenderContext->pCbvSrvUavDescriptorBaseAllocatorCPU->descriptorIncrementSize);

            if (resourceDesc.type == nrd::ResourceType::TRANSIENT_POOL || resourceDesc.type == nrd::ResourceType::PERMANENT_POOL) {
                Texture* pTexture = nullptr;
                if (resourceDesc.type == nrd::ResourceType::TRANSIENT_POOL) {
                    pTexture = &m_transientTextures[resourceDesc.indexInPool];
                } else if (resourceDesc.type == nrd::ResourceType::PERMANENT_POOL) {
                    pTexture = &m_persistentTextures[resourceDesc.indexInPool];
                } else {
                    spdlog::error("Unknown resource type {}", magic_enum::enum_name(resourceDesc.type));
                }

                const auto transitionIfNecessary = [&](D3D12_RESOURCE_STATES desiredState) {
                    if (pTexture->resourceState != desiredState) {
                        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(pTexture->pResource.Get(), pTexture->resourceState, desiredState);
                        args.pCommandList->ResourceBarrier(1, &barrier);
                        pTexture->resourceState = desiredState;
                    }
                };

                if (resourceDesc.descriptorType == nrd::DescriptorType::TEXTURE) {
                    transitionIfNecessary(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    args.pRenderContext->pDevice->CreateShaderResourceView(pTexture->pResource.Get(), &pTexture->srvDesc, descriptor.firstCPUDescriptor);
                } else if (resourceDesc.descriptorType == nrd::DescriptorType::STORAGE_TEXTURE) {
                    transitionIfNecessary(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                    args.pRenderContext->pDevice->CreateUnorderedAccessView(pTexture->pResource.Get(), nullptr, &pTexture->uavDesc, descriptor.firstCPUDescriptor);
                } else {
                    spdlog::error("Unknown NRD descriptor type");
                }
            } else {
                if (resourceDesc.type == nrd::ResourceType::OUT_VALIDATION) {
                    auto textureUAV = resources.getTextureUAV<"out_validation">();
                    args.pRenderContext->pDevice->CreateUnorderedAccessView(textureUAV.pResource, nullptr, &textureUAV.desc, descriptor.firstCPUDescriptor);
                } else if (resourceDesc.type == nrd::ResourceType::IN_MV) {
                    auto textureSRV = resources.getTextureSRV<"in_mv">();
                    args.pRenderContext->pDevice->CreateShaderResourceView(textureSRV.pResource, &textureSRV.desc, descriptor.firstCPUDescriptor);
                } else if (resourceDesc.type == nrd::ResourceType::IN_NORMAL_ROUGHNESS) {
                    auto textureSRV = resources.getTextureSRV<"in_normal_roughness">();
                    args.pRenderContext->pDevice->CreateShaderResourceView(textureSRV.pResource, &textureSRV.desc, descriptor.firstCPUDescriptor);
                } else if (resourceDesc.type == nrd::ResourceType::IN_VIEWZ) {
                    auto textureSRV = resources.getTextureSRV<"in_viewz">();
                    args.pRenderContext->pDevice->CreateShaderResourceView(textureSRV.pResource, &textureSRV.desc, descriptor.firstCPUDescriptor);
                } else if (resourceDesc.type == nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST) {
                    auto textureSRV = resources.getTextureSRV<"in_diff_radiance_hitdist">();
                    args.pRenderContext->pDevice->CreateShaderResourceView(textureSRV.pResource, &textureSRV.desc, descriptor.firstCPUDescriptor);
                } else if (resourceDesc.type == nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST) {
                    auto textureUAV = resources.getTextureUAV<"out_diff_radiance_hitdist">();
                    args.pRenderContext->pDevice->CreateUnorderedAccessView(textureUAV.pResource, nullptr, &textureUAV.desc, descriptor.firstCPUDescriptor);
                } else {
                    spdlog::warn("TODO: User Texture {}", magic_enum::enum_name(resourceDesc.type));
                }
            }
        }
        args.pCommandList->SetComputeRootDescriptorTable(0, descriptors.firstGPUDescriptor);
        if (nrdDispatchDesc.constantBufferDataSize > 0) {
            const auto cbvDesc = args.pRenderContext->singleFrameBufferAllocator.allocateCBV((const void*)nrdDispatchDesc.constantBufferData, nrdDispatchDesc.constantBufferDataSize);
            args.pCommandList->SetComputeRootConstantBufferView(1, cbvDesc.BufferLocation);
        }
        args.pCommandList->Dispatch(nrdDispatchDesc.gridWidth, nrdDispatchDesc.gridHeight, 1);
    }
    spdlog::info("numDispatchSpecs: {}", numDispatchSpecs);
}

void NvidiaDenoisePass::displayGUI()
{
    m_mouseX = (uint16_t)ImGui::GetIO().MousePos.x;
    m_mouseY = (uint16_t)ImGui::GetIO().MousePos.y;
}

static void ThrowIfFailed(nrd::Result result, std::source_location location = std::source_location::current())
{
    if (result != nrd::Result::SUCCESS)
        spdlog::error("{} ({}:{}) \"{}\"", location.file_name(), location.line(), location.column(), location.function_name());
    Tbx::assert_always(result == nrd::Result::SUCCESS);
}

void NvidiaDenoisePass::initialize(Render::RenderContext& renderContext)
{
    const auto nrdLibraryDesc = nrd::GetLibraryDesc();
    std::array denoisers {
        nrd::DenoiserDesc { .identifier = 0, .denoiser = nrd::Denoiser::SIGMA_SHADOW }
    };
    nrd::InstanceCreationDesc instanceCreationDesc {
        .allocationCallbacks = nullptr,
        .denoisers = denoisers.data(),
        .denoisersNum = (uint32_t)denoisers.size()
    };
    ThrowIfFailed(nrd::CreateInstance(instanceCreationDesc, m_pNrdInstance));

    const nrd::InstanceDesc nrdInstanceDesc = nrd::GetInstanceDesc(*m_pNrdInstance);

    std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
    for (uint32_t samplerIdx = 0; samplerIdx < nrdInstanceDesc.samplersNum; ++samplerIdx) {
        const nrd::Sampler nrdSampler = nrdInstanceDesc.samplers[samplerIdx];
        staticSamplers.push_back(D3D12_STATIC_SAMPLER_DESC {
            .Filter = (nrdSampler == nrd::Sampler::LINEAR_CLAMP ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT),
            .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            .MipLODBias = 0,
            .MaxAnisotropy = 0,
            .ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
            .BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
            .MinLOD = 0,
            .MaxLOD = 0,
            .ShaderRegister = nrdInstanceDesc.samplersBaseRegisterIndex + samplerIdx,
            .RegisterSpace = nrdInstanceDesc.samplersSpaceIndex,
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL });
    }

    for (uint32_t pipelineIdx = 0; pipelineIdx < nrdInstanceDesc.pipelinesNum; ++pipelineIdx) {
        const nrd::PipelineDesc& nrdPipelineDesc = nrdInstanceDesc.pipelines[pipelineIdx];

        spdlog::info("Creating NRD PSO: {}", nrdPipelineDesc.shaderFileName);

        DispatchData dispatchData {};
        uint32_t offsetInDescriptorsFromTableStart = 0;
        std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;
        for (uint32_t resourceRangeIdx = 0; resourceRangeIdx < nrdPipelineDesc.resourceRangesNum; ++resourceRangeIdx) {
            const nrd::ResourceRangeDesc& nrdResourceRange = nrdPipelineDesc.resourceRanges[resourceRangeIdx];
            if (nrdResourceRange.descriptorType == nrd::DescriptorType::TEXTURE)
                dispatchData.texturesOffsetInDescriptorTable = offsetInDescriptorsFromTableStart;
            else
                dispatchData.storageTexturesOffsetInDescriptorTable = offsetInDescriptorsFromTableStart;
            descriptorRanges.push_back(D3D12_DESCRIPTOR_RANGE {
                .RangeType = nrdResourceRange.descriptorType == nrd::DescriptorType::TEXTURE ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                .NumDescriptors = nrdResourceRange.descriptorsNum,
                .BaseShaderRegister = nrdInstanceDesc.resourcesBaseRegisterIndex,
                .RegisterSpace = nrdInstanceDesc.constantBufferAndResourcesSpaceIndex,
                .OffsetInDescriptorsFromTableStart = offsetInDescriptorsFromTableStart });
            offsetInDescriptorsFromTableStart += nrdResourceRange.descriptorsNum;
        }
        dispatchData.descriptorTableSize = offsetInDescriptorsFromTableStart;

        const std::array rootParameters {
            D3D12_ROOT_PARAMETER {
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                .DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE { .NumDescriptorRanges = (UINT)descriptorRanges.size(), .pDescriptorRanges = descriptorRanges.data() },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL },
            D3D12_ROOT_PARAMETER {
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
                .Descriptor = D3D12_ROOT_DESCRIPTOR {
                    .ShaderRegister = nrdInstanceDesc.constantBufferRegisterIndex,
                    .RegisterSpace = nrdInstanceDesc.constantBufferAndResourcesSpaceIndex },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL }
        };

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc {};
        rootSignatureDesc.Init_1_0(UINT(rootParameters.size()), rootParameters.data(), UINT(staticSamplers.size()), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_NONE);
        WRL::ComPtr<ID3DBlob> pRootSignatureBlob, pErrorBlob;
        RenderAPI::ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &pRootSignatureBlob, &pErrorBlob));

        RenderAPI::ThrowIfFailed(renderContext.pDevice->CreateRootSignature(0, pRootSignatureBlob->GetBufferPointer(), pRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&dispatchData.pRootSignature)));

        const D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc {
            .pRootSignature = dispatchData.pRootSignature.Get(),
            .CS = D3D12_SHADER_BYTECODE {
                .pShaderBytecode = nrdPipelineDesc.computeShaderDXIL.bytecode,
                .BytecodeLength = nrdPipelineDesc.computeShaderDXIL.size },
            .NodeMask = 0,
            .CachedPSO = D3D12_CACHED_PIPELINE_STATE { .pCachedBlob = nullptr, .CachedBlobSizeInBytes = 0 },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
        };
        RenderAPI::ThrowIfFailed(renderContext.pDevice->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&dispatchData.pPipelineState)));

        const auto nameStringLength = std::strlen(nrdPipelineDesc.shaderFileName);
        dispatchData.pName = std::make_unique<wchar_t[]>(nameStringLength);
        for (size_t i = 0; i < nameStringLength; ++i)
            dispatchData.pName[i] = nrdPipelineDesc.shaderFileName[i];
        dispatchData.pPipelineState->SetName(dispatchData.pName.get());

        m_dispatchDatas.push_back(std::move(dispatchData));
    }

    const auto createTexture = [&](const nrd::TextureDesc& nrdTextureDesc) -> Texture {
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            convertFormat(nrdTextureDesc.format),
            std::max(1, settings.resolution.x / nrdTextureDesc.downsampleFactor),
            std::max(1, settings.resolution.y / nrdTextureDesc.downsampleFactor));
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        return {
            .pResource = renderContext.createResource(D3D12_HEAP_TYPE_DEFAULT, resourceDesc, D3D12_RESOURCE_STATE_COMMON),
            .srvDesc = D3D12_SHADER_RESOURCE_VIEW_DESC {
                .Format = convertFormat(nrdTextureDesc.format),
                .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Texture2D = D3D12_TEX2D_SRV { .MostDetailedMip = 0, .MipLevels = 1, .PlaneSlice = 0, .ResourceMinLODClamp = 0.0f } },
            .uavDesc = D3D12_UNORDERED_ACCESS_VIEW_DESC { .Format = convertFormat(nrdTextureDesc.format), .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D, .Texture2D = D3D12_TEX2D_UAV { .MipSlice = 0, .PlaneSlice = 0 } },
            .resourceState = D3D12_RESOURCE_STATE_COMMON
        };
    };
    for (uint32_t textureIdx = 0; textureIdx < nrdInstanceDesc.permanentPoolSize; ++textureIdx) {
        const nrd::TextureDesc& nrdTextureDesc = nrdInstanceDesc.permanentPool[textureIdx];
        m_persistentTextures.push_back(createTexture(nrdTextureDesc));
    }
    for (uint32_t textureIdx = 0; textureIdx < nrdInstanceDesc.transientPoolSize; ++textureIdx) {
        const nrd::TextureDesc& nrdTextureDesc = nrdInstanceDesc.transientPool[textureIdx];
        m_transientTextures.push_back(createTexture(nrdTextureDesc));
    }
}

void NvidiaDenoisePass::destroy(RenderContext& renderContext)
{
    nrd::DestroyInstance(*m_pNrdInstance);
}

void NvidiaDenoiseDecodePass::execute(const FrameGraphRegistry<NvidiaDenoiseDecodePass>& resources, const FrameGraphExecuteArgs& args)
{
    // Unpack the color buffer into plain RGBA format.
    args.pCommandList->SetComputeRootSignature(m_pRootSignature.Get());
    args.pCommandList->SetPipelineState(m_pPipelineState.Get());

    ShaderInputs::NvidiaDenoiseDecode decodeInputs {};
    decodeInputs.setInTexture(resources.getTextureSRV<"in_diff_radiance_hitdist">());
    decodeInputs.setOutTexture(resources.getTextureUAV<"framebuffer">());
    const auto compiledInputs = decodeInputs.generateTransientBindings(*args.pRenderContext);
    ShaderInputs::ComputeLayout::bindMainCompute(args.pCommandList, compiledInputs);

    const auto numThreadGroups = Util::roundUpToClosestMultiple(resources.getTextureResolution<"framebuffer">(), glm::uvec2(8));
    args.pCommandList->Dispatch(numThreadGroups.x, numThreadGroups.y, 1);
}

void NvidiaDenoiseDecodePass::initialize(Render::RenderContext& renderContext)
{
    const auto decodeShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/PostProcessing/nvidia_denoise_decode_cs.dxil");

    m_pRootSignature = ShaderInputs::ComputeLayout::getRootSignature(renderContext.pDevice.Get());
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc {
        .pRootSignature = m_pRootSignature.Get(),
        .CS = decodeShader,
        .NodeMask = 0,
        .CachedPSO = D3D12_CACHED_PIPELINE_STATE { .pCachedBlob = nullptr, .CachedBlobSizeInBytes = 0 },
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
    };

    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));
    m_pPipelineState->SetName(L"Nvidia Denoise Decode");
}

}

static DXGI_FORMAT convertFormat(nrd::Format nrdFormat)
{
    std::unordered_map<nrd::Format, DXGI_FORMAT> lut;
    lut[nrd::Format::R8_UNORM] = DXGI_FORMAT_R8_UNORM;
    lut[nrd::Format::R8_SNORM] = DXGI_FORMAT_R8_SNORM;
    lut[nrd::Format::R8_UINT] = DXGI_FORMAT_R8_UINT;
    lut[nrd::Format::R8_SINT] = DXGI_FORMAT_R8_SINT;
    lut[nrd::Format::RG8_UNORM] = DXGI_FORMAT_R8G8_UNORM;
    lut[nrd::Format::RG8_SNORM] = DXGI_FORMAT_R8G8_SNORM;
    lut[nrd::Format::RG8_SNORM] = DXGI_FORMAT_R8G8_SNORM;
    lut[nrd::Format::RG8_UINT] = DXGI_FORMAT_R8G8_UINT;
    lut[nrd::Format::RG8_SINT] = DXGI_FORMAT_R8G8_SINT;
    lut[nrd::Format::RGBA8_UNORM] = DXGI_FORMAT_R8G8B8A8_UNORM;
    lut[nrd::Format::RGBA8_SNORM] = DXGI_FORMAT_R8G8B8A8_SNORM;
    lut[nrd::Format::RGBA8_UINT] = DXGI_FORMAT_R8G8B8A8_UINT;
    lut[nrd::Format::RGBA8_SINT] = DXGI_FORMAT_R8G8B8A8_SINT;
    lut[nrd::Format::RGBA8_SRGB] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    lut[nrd::Format::R16_UNORM] = DXGI_FORMAT_R16_UNORM;
    lut[nrd::Format::R16_SNORM] = DXGI_FORMAT_R16_SNORM;
    lut[nrd::Format::R16_UINT] = DXGI_FORMAT_R16_UINT;
    lut[nrd::Format::R16_SINT] = DXGI_FORMAT_R16_SINT;
    lut[nrd::Format::R16_SFLOAT] = DXGI_FORMAT_R16_FLOAT;
    lut[nrd::Format::RG16_UNORM] = DXGI_FORMAT_R16G16_UNORM;
    lut[nrd::Format::RG16_SNORM] = DXGI_FORMAT_R16G16_SNORM;
    lut[nrd::Format::RG16_UINT] = DXGI_FORMAT_R16G16_UINT;
    lut[nrd::Format::RG16_SINT] = DXGI_FORMAT_R16G16_SINT;
    lut[nrd::Format::RG16_SFLOAT] = DXGI_FORMAT_R16G16_FLOAT;
    lut[nrd::Format::RGBA16_UNORM] = DXGI_FORMAT_R16G16B16A16_UNORM;
    lut[nrd::Format::RGBA16_SNORM] = DXGI_FORMAT_R16G16B16A16_SNORM;
    lut[nrd::Format::RGBA16_UINT] = DXGI_FORMAT_R16G16B16A16_UINT;
    lut[nrd::Format::RGBA16_SINT] = DXGI_FORMAT_R16G16B16A16_SINT;
    lut[nrd::Format::RGBA16_SFLOAT] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    lut[nrd::Format::R32_UINT] = DXGI_FORMAT_R32_UINT;
    lut[nrd::Format::R32_SINT] = DXGI_FORMAT_R32_SINT;
    lut[nrd::Format::R32_SFLOAT] = DXGI_FORMAT_R32_FLOAT;
    lut[nrd::Format::RG32_UINT] = DXGI_FORMAT_R32G32_UINT;
    lut[nrd::Format::RG32_SINT] = DXGI_FORMAT_R32G32_SINT;
    lut[nrd::Format::RG32_SFLOAT] = DXGI_FORMAT_R32G32_FLOAT;
    lut[nrd::Format::RGB32_UINT] = DXGI_FORMAT_R32G32B32_UINT;
    lut[nrd::Format::RGB32_SINT] = DXGI_FORMAT_R32G32B32_SINT;
    lut[nrd::Format::RGB32_SFLOAT] = DXGI_FORMAT_R32G32B32_FLOAT;
    lut[nrd::Format::RGBA32_UINT] = DXGI_FORMAT_R32G32B32A32_UINT;
    lut[nrd::Format::RGBA32_SINT] = DXGI_FORMAT_R32G32B32A32_SINT;
    lut[nrd::Format::RGBA32_SFLOAT] = DXGI_FORMAT_R32G32B32A32_FLOAT;
    lut[nrd::Format::R10_G10_B10_A2_UNORM] = DXGI_FORMAT_R10G10B10A2_UNORM;
    lut[nrd::Format::R10_G10_B10_A2_UINT] = DXGI_FORMAT_R10G10B10A2_UINT;
    lut[nrd::Format::R11_G11_B10_UFLOAT] = DXGI_FORMAT_R11G11B10_FLOAT;
    lut[nrd::Format::R9_G9_B9_E5_UFLOAT] = DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
    return lut[nrdFormat];
}
