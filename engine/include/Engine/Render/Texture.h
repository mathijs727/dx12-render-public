#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include "Engine/Util/ForwardDeclares.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec2.hpp>
DISABLE_WARNINGS_POP()
#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace Render {

enum class ColorSpace {
    Linear,
    Srgb,
    Unknown
};

struct TextureCPU {
public:
    struct MipLevel {
        uint32_t mipLevelStart;
        uint32_t rowPitch;
    };
    glm::uvec2 resolution;
    DXGI_FORMAT textureFormat;
    std::vector<std::byte> pixelData;
    std::vector<MipLevel> mipLevels;
    bool isOpague;

public:
    struct TextureReadSettings {
        TextureFileType fileType;
        ColorSpace colorSpaceHint = ColorSpace::Unknown;
        bool generateMipMaps = true;
    };
    static TextureCPU readFromFile(std::filesystem::path filePath, const TextureReadSettings& loadSettings);
    static TextureCPU readFromBuffer(std::span<const std::byte> buffer, const TextureReadSettings& loadSettings);
    void saveToFile(const std::filesystem::path& filePath, TextureFileType textureFileType) const;

    TextureCPU tryConvertKTX2(bool generateMipMaps) const;

    void writeTo(Util::BinaryWriter& writer) const;
    void readFrom(Util::BinaryReader& reader);
};

struct Texture {
    D3D12_SHADER_RESOURCE_VIEW_DESC resourceView;
    RenderAPI::D3D12MAResource pResource;

    operator RenderAPI::SRVDesc() const
    {
        return RenderAPI::SRVDesc {
            .desc = resourceView,
            .pResource = pResource
        };
    }

    static Texture uploadToGPU(const TextureCPU& image, D3D12_RESOURCE_STATES resourceState, RenderContext& renderContext);
};

}
