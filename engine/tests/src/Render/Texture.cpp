#include "pch.h"
#include <Engine/Render/Texture.h>
#include <tbx/disable_all_warnings.h>
#include <tbx/format/fmt_glm.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec4.hpp>
DISABLE_WARNINGS_POP()
#include <algorithm>

using namespace Catch::literals;
using namespace Render;

TEST_CASE("Render::TextureCPU::readFromFile", "[Render]")
{
    SECTION("DDS")
    {
        TextureCPU::TextureReadSettings readSettings {
            .fileType = TextureFileType::DDS,
            .colorSpaceHint = ColorSpace::Srgb,
            .generateMipMaps = false
        };
        const auto textureCPU = TextureCPU::readFromFile("assets/blue16x16.dds", readSettings);
        REQUIRE(textureCPU.resolution == glm::uvec2(16, 16));
        REQUIRE(textureCPU.textureFormat == DXGI_FORMAT_BC3_UNORM_SRGB);
        REQUIRE(textureCPU.isOpague);
        REQUIRE(textureCPU.mipLevels.size() == 1);
    }

    SECTION("PNG")
    {
        using Pixel = glm::u8vec4;
        TextureCPU::TextureReadSettings readSettings {
            .fileType = TextureFileType::PNG,
            .colorSpaceHint = ColorSpace::Linear,
            .generateMipMaps = false
        };
        const auto textureCPU = TextureCPU::readFromFile("assets/colors16x16.png", readSettings);
        REQUIRE(textureCPU.resolution == glm::uvec2(16, 16));
        REQUIRE(textureCPU.textureFormat == DXGI_FORMAT_R8G8B8A8_UNORM);
        REQUIRE(textureCPU.isOpague);
        REQUIRE(textureCPU.mipLevels.size() == 1);
        REQUIRE(textureCPU.mipLevels[0].rowPitch == 16 * sizeof(Pixel));

        const Pixel* pPixels = reinterpret_cast<const Pixel*>(textureCPU.pixelData.data());
        REQUIRE(pPixels[0 * 16 + 0] == Pixel(0, 0, 255, 255)); // Left/top corner is blue.
        REQUIRE(pPixels[0 * 16 + 15] == Pixel(255, 0, 0, 255)); // Right/top corner is red.
        REQUIRE(pPixels[15 * 16 + 0] == Pixel(0, 255, 0, 255)); // Bottom/left corner is green.
        REQUIRE(pPixels[15 * 16 + 15] == Pixel(255, 255, 255, 255)); // Bottom/right corner is white.
    }

    SECTION("PNG (MipMap)")
    {
        using Pixel = glm::u8vec4;
        TextureCPU::TextureReadSettings readSettings {
            .fileType = TextureFileType::PNG,
            .colorSpaceHint = ColorSpace::Linear,
            .generateMipMaps = true
        };
        const auto textureCPU = TextureCPU::readFromFile("assets/colors16x16.png", readSettings);
        REQUIRE(textureCPU.resolution == glm::uvec2(16, 16));
        REQUIRE(textureCPU.textureFormat == DXGI_FORMAT_R8G8B8A8_UNORM);
        REQUIRE(textureCPU.isOpague);
        REQUIRE(textureCPU.mipLevels.size() == 5);
        REQUIRE(textureCPU.mipLevels[3].rowPitch == 2 * sizeof(Pixel));

        // Mip levels:
        // [0] 16x16
        // [1] 8x8
        // [2] 4x4
        // [3] 2x2
        // [4] 1x1
        const Pixel* pPixels = reinterpret_cast<const Pixel*>(&textureCPU.pixelData[textureCPU.mipLevels[3].mipLevelStart]);
        REQUIRE(pPixels[0] == Pixel(0, 0, 255, 255)); // Left/top corner is blue.
        REQUIRE(pPixels[1] == Pixel(255, 0, 0, 255)); // Right/top corner is red.
        REQUIRE(pPixels[2] == Pixel(0, 255, 0, 255)); // Bottom/left corner is green.
        REQUIRE(pPixels[3] == Pixel(255, 255, 255, 255)); // Bottom/right corner is white.
    }
}

TEST_CASE("Render::TextureCPU::saveToFile", "[Render]")
{

    SECTION("PNG (BGRA8)")
    {
        using Pixel = glm::u8vec4;
        TextureCPU texture {
            .resolution = glm::uvec2(8, 8),
            .textureFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
            .isOpague = true
        };
        texture.mipLevels.push_back({ 0, 8 * sizeof(Pixel) });
        texture.pixelData.resize(8 * 8 * sizeof(Pixel));
        Pixel* pPixels = reinterpret_cast<Pixel*>(texture.pixelData.data());
        std::fill(pPixels, pPixels + (8 * 8), Pixel(255, 128, 0, 255));
        texture.saveToFile("test_png_rgba8.png", TextureFileType::PNG);

        auto readTexture = TextureCPU::readFromFile("test_png_rgba8.png", { .fileType = TextureFileType::PNG, .generateMipMaps = true });
        REQUIRE(readTexture.resolution == glm::uvec2(8, 8));
        REQUIRE(readTexture.textureFormat == DXGI_FORMAT_B8G8R8A8_UNORM);
        REQUIRE(readTexture.isOpague == true);
        REQUIRE(readTexture.mipLevels.size() == 4);
        REQUIRE(readTexture.mipLevels[0].rowPitch == 8 * sizeof(Pixel));
        Pixel* pReadPixels = reinterpret_cast<Pixel*>(readTexture.pixelData.data());
        for (size_t i = 0; i < 8 * 8; ++i) {
            REQUIRE(pReadPixels[i] == Pixel(255, 128, 0, 255));
        }
    }

    SECTION("PNG (RGBA8)")
    {
        using Pixel = glm::u8vec4;
        TextureCPU texture {
            .resolution = glm::uvec2(8, 8),
            .textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
            .isOpague = true
        };
        texture.mipLevels.push_back({ 0, 8 * sizeof(Pixel) });
        texture.pixelData.resize(8 * 8 * sizeof(Pixel));
        Pixel* pPixels = reinterpret_cast<Pixel*>(texture.pixelData.data());
        std::fill(pPixels, pPixels + (8 * 8), Pixel(0, 128, 255, 255));
        texture.saveToFile("test_png_rgba8.png", TextureFileType::PNG);

        auto readTexture = TextureCPU::readFromFile("test_png_rgba8.png", { .fileType = TextureFileType::PNG, .generateMipMaps = true });
        REQUIRE(readTexture.resolution == glm::uvec2(8, 8));
        REQUIRE(readTexture.textureFormat == DXGI_FORMAT_B8G8R8A8_UNORM);
        REQUIRE(readTexture.isOpague == true);
        REQUIRE(readTexture.mipLevels.size() == 4);
        REQUIRE(readTexture.mipLevels[0].rowPitch == 8 * sizeof(Pixel));
        Pixel* pReadPixels = reinterpret_cast<Pixel*>(readTexture.pixelData.data());
        for (size_t i = 0; i < 8 * 8; ++i) {
            REQUIRE(pReadPixels[i] == Pixel(255, 128, 0, 255));
        }
    }

    // TODO(Mathijs): the *.hdr reader in DirectXTex always adds 0.25 to each floating point value.
    /* SECTION("HDR (RGBA float32)")
    {
        using Pixel = glm::vec4;
        TextureCPU texture {
            .resolution = glm::uvec2(8, 8),
            .textureFormat = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .isOpague = true
        };
        texture.mipLevels.push_back({ 0, 8 * sizeof(Pixel) });
        texture.pixelData.resize(8 * 8 * sizeof(Pixel));
        Pixel* pPixels = reinterpret_cast<Pixel*>(texture.pixelData.data());
        std::fill(pPixels, pPixels + (8 * 8), Pixel(0.5f, 1.0f, 123.0f, 1.0f));
        texture.saveToFile("test_hdr_rgba_f32.hdr", TextureFileType::HDR);

        auto readTexture = TextureCPU::readFromFile("test_hdr_rgba_f32.hdr", { .fileType = TextureFileType::HDR, .generateMipMaps = true });
        REQUIRE(readTexture.resolution == glm::uvec2(8, 8));
        REQUIRE(readTexture.textureFormat == DXGI_FORMAT_R32G32B32A32_FLOAT);
        REQUIRE(readTexture.isOpague == true);
        REQUIRE(readTexture.mipLevels.size() == 4);
        REQUIRE(readTexture.mipLevels[0].rowPitch == 8 * sizeof(Pixel));
        Pixel* pReadPixels = reinterpret_cast<Pixel*>(readTexture.pixelData.data());
        for (size_t i = 0; i < 8 * 8; ++i) {
            // Check if the pixel values are approximately equal due to floating point precision.
            const auto readPixel = pReadPixels[i];
            REQUIRE(readPixel.r == Catch::Approx(0.5f));
            REQUIRE(readPixel.g == Catch::Approx(1.0f));
            REQUIRE(readPixel.b == Catch::Approx(123.0f));
            REQUIRE(readPixel.a == Catch::Approx(1.0f));
        }
    } */
}
