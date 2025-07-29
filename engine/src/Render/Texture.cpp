#include "Engine/Render/Texture.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/Util/BinaryReader.h"
#include "Engine/Util/BinaryWriter.h"
#include "Engine/Util/ErrorHandling.h"
#include "VkFormat.h"
#include <tbx/disable_all_warnings.h>
#include <tbx/error_handling.h>
DISABLE_WARNINGS_PUSH()
#include <DirectXTex.h>
#include <DirectXTexEXR.h>
#include <Wincodec.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <ktx.h>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <type_traits>
#include <unordered_map>

namespace Render {

Texture Texture::uploadToGPU(const TextureCPU& textureCPU, D3D12_RESOURCE_STATES desiredResourceState, RenderContext& renderContext)
{
    auto pCommandList = renderContext.commandListManager.acquireCommandList();

    // See:
    // https://stackoverflow.com/questions/35568302/what-is-the-d3d12-equivalent-of-d3d11-createtexture2d
    // const auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(image.textureFormat, image.resolution.x, image.resolution.y, 1, 1);
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = (uint16_t)textureCPU.mipLevels.size();
    textureDesc.Format = textureCPU.textureFormat;
    textureDesc.Width = textureCPU.resolution.x;
    textureDesc.Height = textureCPU.resolution.y;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    auto pResource = renderContext.createResource(D3D12_HEAP_TYPE_DEFAULT, textureDesc, D3D12_RESOURCE_STATE_COPY_DEST);
    pResource->SetName(L"Texture");

    // Create the GPU upload buffer
    constexpr size_t maxMipLevel = 20;
    Tbx::assert_always(textureDesc.MipLevels < maxMipLevel);
    std::array<uint32_t, maxMipLevel> numRows;
    std::array<uint64_t, maxMipLevel> bytesPerRow;
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, maxMipLevel> layouts;
    uint64_t totalBytes = 0;
    renderContext.pDevice->GetCopyableFootprints(&textureDesc, 0, textureDesc.MipLevels, 0, layouts.data(), numRows.data(), bytesPerRow.data(), &totalBytes);

    const auto stagingResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBytes, D3D12_RESOURCE_FLAG_NONE);
    const auto pStagingResource = renderContext.createResource(D3D12_HEAP_TYPE_UPLOAD, stagingResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ);
    pStagingResource->SetName(L"StagingTexture");

    std::vector<D3D12_SUBRESOURCE_DATA> mipsResourceData;
    for (uint32_t mipLevel = 0; mipLevel < textureDesc.MipLevels; ++mipLevel) {
        auto& textureData = mipsResourceData.emplace_back();
        textureData.pData = textureCPU.pixelData.data() + textureCPU.mipLevels[mipLevel].mipLevelStart;
        textureData.RowPitch = bytesPerRow[mipLevel];
        textureData.SlicePitch = bytesPerRow[mipLevel] * textureCPU.resolution.y;
    }
    UpdateSubresources(pCommandList.Get(), pResource, pStagingResource.Get(), 0, 0, textureDesc.MipLevels, mipsResourceData.data());

    const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(pResource, D3D12_RESOURCE_STATE_COPY_DEST, desiredResourceState);
    pCommandList->ResourceBarrier(1, &transition);

    pCommandList->Close();
    std::array<ID3D12CommandList*, 1> commandLists { pCommandList.Get() };
    renderContext.pGraphicsQueue->ExecuteCommandLists((UINT)commandLists.size(), commandLists.data());
    renderContext.commandListManager.recycleCommandList(renderContext.pGraphicsQueue.Get(), pCommandList);
    // Wait for the GPU to finish copying before deleting the CPU buffer.
    renderContext.waitForIdle();

    const D3D12_SHADER_RESOURCE_VIEW_DESC resourceView {
        .Format = textureCPU.textureFormat,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = { .MostDetailedMip = 0, .MipLevels = textureDesc.MipLevels, .PlaneSlice = 0, .ResourceMinLODClamp = 0.0f }
    };
    return {
        .resourceView = resourceView,
        .pResource = std::move(pResource)
    };
}

static TextureCPU loadWithDirectXTex(auto loadTexture, const TextureCPU::TextureReadSettings& readSettings)
{
    DirectX::TexMetadata scratchMetaData;
    DirectX::ScratchImage scratchImage;
    loadTexture(&scratchMetaData, scratchImage);

    const DirectX::Image* pMIP0 = scratchImage.GetImage(0, 0, 0);
    TextureCPU out {
        .resolution = glm::uvec2(pMIP0->width, pMIP0->height),
        .textureFormat = pMIP0->format,
        .isOpague = (scratchMetaData.GetAlphaMode() == DirectX::TEX_ALPHA_MODE_OPAQUE || scratchImage.IsAlphaAllOpaque())
    };

    if (readSettings.colorSpaceHint == ColorSpace::Srgb) {
        switch (out.textureFormat) {
        case DXGI_FORMAT_BC1_UNORM: {
            out.textureFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
        } break;
        case DXGI_FORMAT_BC2_UNORM: {
            out.textureFormat = DXGI_FORMAT_BC2_UNORM_SRGB;
        } break;
        case DXGI_FORMAT_BC3_UNORM: {
            out.textureFormat = DXGI_FORMAT_BC3_UNORM_SRGB;
        } break;
        case DXGI_FORMAT_BC7_UNORM: {
            out.textureFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
        } break;
        case DXGI_FORMAT_R8G8B8A8_UNORM: {
            out.textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        } break;
        case DXGI_FORMAT_B8G8R8A8_UNORM: {
            out.textureFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        } break;
        default:
            break;
        };
    }

    if (readSettings.generateMipMaps) {
        DirectX::ScratchImage mipChain;
        RenderAPI::ThrowIfFailed(
            DirectX::GenerateMipMaps(scratchImage.GetImages(), scratchImage.GetImageCount(), scratchMetaData, DirectX::TEX_FILTER_DEFAULT, 0, mipChain));
        const auto mipMetaData = mipChain.GetMetadata();
        for (int mipLevel = 0; mipLevel < mipMetaData.mipLevels; ++mipLevel) {
            const DirectX::Image* pImage = mipChain.GetImage(mipLevel, 0, 0);
            const auto mipLevelStart = (uint32_t)out.pixelData.size();
            out.mipLevels.push_back({ mipLevelStart, (uint32_t)pImage->rowPitch });

            const auto imageSizeInBytes = pImage->slicePitch;
            out.pixelData.resize(mipLevelStart + imageSizeInBytes);
            std::memcpy(out.pixelData.data() + mipLevelStart, pImage->pixels, imageSizeInBytes);
        }
    } else {
        const auto imageSizeInBytes = pMIP0->slicePitch;
        out.pixelData.resize(imageSizeInBytes);
        std::memcpy(out.pixelData.data(), pMIP0->pixels, imageSizeInBytes);
        out.mipLevels.push_back({ 0, (uint32_t)pMIP0->rowPitch });
    }
    return out;
}

static TextureCPU loadWithKTX2(auto loadTexture, const TextureCPU::TextureReadSettings& settings)
{
    ktxTexture2* pKtxTexture2 = nullptr;
    KTX_error_code result = loadTexture(&pKtxTexture2);
    Tbx::assert_always(result == KTX_SUCCESS);

    const auto numComponents = ktxTexture2_GetNumComponents(pKtxTexture2);
    bool isOpague = true;
    DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;
    if (ktxTexture2_NeedsTranscoding(pKtxTexture2)) {
        const khr_df_model_e colorModel = ktxTexture2_GetColorModel_e(pKtxTexture2);
        ktx_texture_transcode_fmt_e transcodeFormat = KTX_TTF_NOSELECTION;
        if (numComponents == 1) {
            textureFormat = DXGI_FORMAT_BC4_UNORM;
            transcodeFormat = KTX_TTF_BC4_R;
        } else if (numComponents == 2) {
            textureFormat = DXGI_FORMAT_BC5_UNORM;
            transcodeFormat = KTX_TTF_BC5_RG;
        } else if (numComponents == 3) {
            textureFormat = settings.colorSpaceHint == ColorSpace::Srgb ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
            transcodeFormat = KTX_TTF_BC7_RGBA;
        } else if (numComponents == 4) {
            textureFormat = settings.colorSpaceHint == ColorSpace::Srgb ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
            transcodeFormat = KTX_TTF_BC7_RGBA;
            isOpague = false;
        }
        ktxTexture2_TranscodeBasis(pKtxTexture2, transcodeFormat, 0);
    } else {
        // spdlog::error("TODO: implement ktxTexture2 without transcoding");
        // Tbx::assert_always(false);
        isOpague = true;
    }

    std::vector<std::byte> pixelData;
    std::vector<typename TextureCPU::MipLevel> mipLevels;
    ktxTexture* pKtxTexture = ktxTexture(pKtxTexture2);
    constexpr ktx_uint32_t layer = 0, faceSlice = 0;
    for (ktx_uint32_t level = 0; level < pKtxTexture->numLevels; ++level) {
        ktx_size_t imageOffset = 0;
        ktxTexture_GetImageOffset(pKtxTexture, level, layer, faceSlice, &imageOffset);
        const auto* pLevelPixelData = ktxTexture_GetData(pKtxTexture) + imageOffset;
        const auto levelPixelDataSize = ktxTexture_GetImageSize(pKtxTexture, level);
        const uint32_t outOffset = (uint32_t)pixelData.size();
        mipLevels.push_back({ outOffset, ktxTexture_GetRowPitch(pKtxTexture, level) });
        pixelData.resize(pixelData.size() + levelPixelDataSize);
        std::memcpy(pixelData.data() + outOffset, pLevelPixelData, levelPixelDataSize);
    }

    TextureCPU out {
        .resolution = glm::uvec2(pKtxTexture2->baseWidth, pKtxTexture2->baseHeight),
        .textureFormat = textureFormat,
        .pixelData = std::move(pixelData),
        .mipLevels = std::move(mipLevels),
        .isOpague = isOpague,
    };
    ktxTexture_Destroy(pKtxTexture);
    return out;
}

TextureCPU TextureCPU::readFromFile(std::filesystem::path filePathToLoad, const TextureReadSettings& readSettings)
{
    Tbx::assert_always(std::filesystem::exists(filePathToLoad));
    filePathToLoad.make_preferred();
    switch (readSettings.fileType) {
    case TextureFileType::JPG:
    case TextureFileType::PNG:
        return loadWithDirectXTex(
            [&](DirectX::TexMetadata* pMetaData, DirectX::ScratchImage& scratchImage) {
                auto result = DirectX::LoadFromWICFile(filePathToLoad.c_str(), DirectX::WIC_FLAGS_NONE, pMetaData, scratchImage);
                Tbx::assert_always(!FAILED(result));
            },
            readSettings);
    case TextureFileType::DDS:
        return loadWithDirectXTex(
            [&](DirectX::TexMetadata* pMetaData, DirectX::ScratchImage& scratchImage) {
                auto result = DirectX::LoadFromDDSFile(filePathToLoad.c_str(), DirectX::DDS_FLAGS_NONE, pMetaData, scratchImage);
                Tbx::assert_always(!FAILED(result));
            },
            readSettings);
    case TextureFileType::HDR:
        return loadWithDirectXTex(
            [&](DirectX::TexMetadata* pMetaData, DirectX::ScratchImage& scratchImage) {
                auto result = DirectX::LoadFromHDRFile(filePathToLoad.c_str(), pMetaData, scratchImage);
                Tbx::assert_always(!FAILED(result));
            },
            readSettings);
    case TextureFileType::OpenEXR:
        return loadWithDirectXTex(
            [&](DirectX::TexMetadata* pMetaData, DirectX::ScratchImage& scratchImage) {
                auto result = DirectX::LoadFromEXRFile(filePathToLoad.c_str(), pMetaData, scratchImage);
                Tbx::assert_always(!FAILED(result));
            },
            readSettings);
    case TextureFileType::KTX2:
        return loadWithKTX2(
            [&](ktxTexture2** ppTexture) {
                const std::string strFilePath = filePathToLoad.string();
                return ktxTexture2_CreateFromNamedFile(strFilePath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, ppTexture);
            },
            readSettings);
    default:
        spdlog::error("Unsupported texture file type");
    };
    return {};
}

TextureCPU TextureCPU::readFromBuffer(std::span<const std::byte> buffer, const TextureReadSettings& readSettings)
{
    switch (readSettings.fileType) {
    case TextureFileType::JPG:
    case TextureFileType::PNG:
    case TextureFileType::DDS:
        return loadWithDirectXTex(
            [&](DirectX::TexMetadata* pMetaData, DirectX::ScratchImage& scratchImage) {
                DirectX::LoadFromWICMemory(buffer.data(), buffer.size_bytes(), DirectX::WIC_FLAGS_NONE, pMetaData, scratchImage);
            },
            readSettings);
    case TextureFileType::HDR:
        return loadWithDirectXTex(
            [&](DirectX::TexMetadata* pMetaData, DirectX::ScratchImage& scratchImage) {
                auto result = DirectX::LoadFromHDRMemory(buffer.data(), buffer.size_bytes(), pMetaData, scratchImage);
                Tbx::assert_always(!FAILED(result));
            },
            readSettings);
    case TextureFileType::KTX2:
        return loadWithKTX2(
            [&](ktxTexture2** ppTexture) {
                return ktxTexture2_CreateFromMemory((const ktx_uint8_t*)buffer.data(), buffer.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, ppTexture);
            },
            readSettings);
    default:
        spdlog::error("Unsupported texture file type");
    };

    return {};
}

TextureCPU TextureCPU::tryConvertKTX2(bool generateMipMaps) const
{
    static std::unordered_map<DXGI_FORMAT, ktx_uint32_t> dxgiToVulkanFormat {
        { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, VkFormat::VK_FORMAT_R8G8B8A8_SRGB },
        { DXGI_FORMAT_R8G8B8A8_UNORM, VkFormat::VK_FORMAT_R8G8B8A8_UNORM },
        //{ DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, VkFormat::VK_FORMAT_B8G8R8A8_SRGB },
        //{ DXGI_FORMAT_B8G8R8A8_UNORM, VkFormat::VK_FORMAT_B8G8R8A8_UNORM },
        { DXGI_FORMAT_R8_UNORM, VkFormat::VK_FORMAT_R8_UNORM },
    };

    ktx_uint32_t vkFormat = 0;
    if (auto iter = dxgiToVulkanFormat.find(this->textureFormat); iter != std::end(dxgiToVulkanFormat)) {
        vkFormat = iter->second;
    } else {
        // Add mapping to the code above when this happens.
        // Util::ThrowError("Missing DXGI_FORMAT -> VkFormat mapping for format.");
        spdlog::warn("Missing DXGI_FORMAT -> VkFormat mapping for format.");
        return *this;
    }

    ktxTextureCreateInfo createInfo {
        .vkFormat = vkFormat,
        .baseWidth = resolution.x,
        .baseHeight = resolution.y,
        .baseDepth = 1,
        .numDimensions = 2,
        .numLevels = (ktx_uint32_t)mipLevels.size(),
        .numLayers = 1,
        .numFaces = 1,
        .isArray = false,
        .generateMipmaps = generateMipMaps
    };
    return loadWithKTX2(
        [&](ktxTexture2** ppTexture) {
            auto result = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, ppTexture);
            Util::AssertEQ(result, KTX_SUCCESS);
            ktxTexture_SetImageFromMemory(ktxTexture(*ppTexture), 0, 0, 0, (const ktx_uint8_t*)pixelData.data(), pixelData.size());
            result = ktxTexture2_CompressBasis(*ppTexture, 255);
            return result;
        },
        {});
}

void TextureCPU::saveToFile(const std::filesystem::path& filePath, TextureFileType textureFileType) const
{
    DirectX::Image dxImage {
        .width = resolution.x,
        .height = resolution.y,
        .format = textureFormat,
        .rowPitch = mipLevels[0].rowPitch,
        .slicePitch = pixelData.size(),
        .pixels = (uint8_t*)&pixelData[mipLevels[0].mipLevelStart]
    };

    switch (textureFileType) {
    case TextureFileType::JPG:
    case TextureFileType::PNG: {
        const std::unordered_map<TextureFileType, DirectX::WICCodecs> fileTypeToWICCodec {
            { TextureFileType::JPG, DirectX::WICCodecs::WIC_CODEC_JPEG },
            { TextureFileType::PNG, DirectX::WICCodecs::WIC_CODEC_PNG },
        };
        RenderAPI::ThrowIfFailed(
            DirectX::SaveToWICFile(
                &dxImage, 1, DirectX::WIC_FLAGS_NONE,
                DirectX::GetWICCodec(fileTypeToWICCodec.find(textureFileType)->second),
                filePath.wstring().c_str()));
    } break;
    case TextureFileType::OpenEXR: {
        RenderAPI::ThrowIfFailed(
            DirectX::SaveToEXRFile(dxImage, filePath.wstring().c_str()));
    } break;
    case TextureFileType::HDR: {
        RenderAPI::ThrowIfFailed(
            DirectX::SaveToHDRFile(dxImage, filePath.wstring().c_str()));
    } break;
        SWITCH_FAIL_DEFAULT
    };
}

void TextureCPU::writeTo(Util::BinaryWriter& writer) const
{
    writer.write(resolution);
    writer.write(textureFormat);
    writer.write(pixelData);
    writer.write(mipLevels);
    writer.write(isOpague);
}

void TextureCPU::readFrom(Util::BinaryReader& reader)
{
    reader.read(resolution);
    reader.read(textureFormat);
    reader.read(pixelData);
    reader.read(mipLevels);
    reader.read(isOpague);
}
}
