#include "GPU.h"
#include "pch.h"
#include <Engine/Core/Window.h>
#include <Engine/Render/FrameGraph/FrameGraph.h>
#include <Engine/Render/FrameGraph/Operations.h>
#include <Engine/Render/RenderContext.h>
#include <Engine/Render/RenderPasses/Debug/RasterDebug.h>
#include <Engine/Render/RenderPasses/RayTracing/PathTracing.h>
#include <Engine/Render/RenderPasses/Util/DownloadImagePass.h>
#include <Engine/Render/Scene.h>
#include <Engine/Util/Math.h>
#include <Tbx/format/fmt_glm.h>
#include <cmath>
#include <iostream>

#define WINDOWED 0

using namespace Catch::literals;

static Render::MeshCPU createSphereMesh(const glm::vec3& position, float radius)
{
    const int thetaSteps = 16;
    const int phiSteps = 32;

    Render::MeshCPU out {};
    for (int i = 0; i < thetaSteps; ++i) {
        const float theta = i * glm::pi<float>() / (thetaSteps - 1);
        for (int j = 0; j < phiSteps; ++j) {
            const float phi = j * glm::two_pi<float>() / (phiSteps - 1);
            ShaderInputs::Vertex vertex;
            vertex.pos = position + glm::vec3(radius * std::sin(theta) * std::cos(phi), radius * std::cos(theta), radius * std::sin(theta) * std::sin(phi));
            vertex.normal = glm::normalize(vertex.pos);
            vertex.texCoord = glm::vec2(phi / glm::two_pi<float>(), theta / glm::pi<float>());
            // Add the vertex to the mesh.
            out.vertices.push_back(vertex);
        }

        if (i == thetaSteps - 1)
            continue;

        const auto ringStart = i * phiSteps;
        const auto nextRingStart = (i + 1) * phiSteps;
        assert(nextRingStart == out.vertices.size());
        for (int j = 0; j < phiSteps; ++j) {
            const int nextJ = (j + 1) % phiSteps;
            out.indices.push_back(nextRingStart + nextJ);
            out.indices.push_back(nextRingStart + j);
            out.indices.push_back(ringStart + j);
            out.indices.push_back(ringStart + nextJ);
            out.indices.push_back(nextRingStart + nextJ);
            out.indices.push_back(ringStart + j);
        }
    }
    out.subMeshes.push_back({ .indexStart = 0,
        .numIndices = static_cast<uint32_t>(out.indices.size()),
        .baseVertex = 0,
        .numVertices = static_cast<uint32_t>(out.vertices.size()) });
    out.materials.emplace_back(ShaderInputs::PBRMaterial {
        .baseColor = glm::vec3(0.5f),
        .baseColorTextureIdx = 0,
        .metallic = 0.0f,
        .alpha = 0.0f });
    out.generateMeshlets();
    return out;
}

static Render::MeshCPU createPlaneMesh(const glm::vec3& position, float radius)
{
    Render::MeshCPU out {};
    out.vertices.push_back(ShaderInputs::Vertex {
        .pos = glm::vec3(-radius, -radius, 0.0f) + position,
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texCoord = glm::vec2(0.0f, 0.0f) });
    ;
    out.vertices.push_back(ShaderInputs::Vertex {
        .pos = glm::vec3(radius, -radius, 0.0f) + position,
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texCoord = glm::vec2(1.0f, 0.0f) });
    out.vertices.push_back(ShaderInputs::Vertex {
        .pos = glm::vec3(radius, radius, 0.0f) + position,
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texCoord = glm::vec2(1.0f, 1.0f) });
    out.vertices.push_back(ShaderInputs::Vertex {
        .pos = glm::vec3(-radius, radius, 0.0f) + position,
        .normal = glm::vec3(0.0f, 0.0f, 1.0f),
        .texCoord = glm::vec2(0.0f, 1.0f) });
    out.indices = { 0, 1, 2, 0, 2, 3 };
    out.subMeshes.push_back({ .indexStart = 0,
        .numIndices = static_cast<uint32_t>(out.indices.size()),
        .baseVertex = 0,
        .numVertices = static_cast<uint32_t>(out.vertices.size()) });
    out.materials.emplace_back(ShaderInputs::PBRMaterial {
        .baseColor = glm::vec3(0.5f),
        .baseColorTextureIdx = 0,
        .metallic = 0.0f,
        .alpha = 0.0f });
    out.generateMeshlets();
    return out;
}

static Render::TextureCPU createBasicTexture()
{
    Render::TextureCPU texture;
    texture.resolution = glm::ivec2(8);
    texture.isOpague = true;
    texture.textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture.pixelData.resize(8 * 8 * sizeof(uint32_t), (std::byte)0xFF);
    texture.mipLevels.push_back({ 0, 8 * sizeof(uint32_t) });
    return texture;
}

static Render::TextureCPU createHDRTexture(float value)
{
    using Pixel = glm::vec4;
    Render::TextureCPU texture;
    texture.resolution = glm::ivec2(8);
    texture.isOpague = true;
    texture.textureFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texture.pixelData.resize(8 * 8 * sizeof(Pixel));
    auto* pPixels = (Pixel*)texture.pixelData.data();
    for (int i = 0; i < 8 * 8; ++i) {
        pPixels[i] = Pixel(value, value, value, 1.0f);
    }
    texture.mipLevels.push_back({ 0, 8 * sizeof(Pixel) });
    return texture;
}

TEST_CASE("Render::FrameGraph::ClearFrameBuffer", "[Render][GPU]")
{
    static constexpr float cameraDistance = 5.0f;
    static constexpr uint32_t resolution = 64;

    Render::RenderContext renderContext {};

    Render::FrameGraphBuilder frameGraphBuilder { &renderContext };
    auto frameBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, resolution, resolution);
    frameBufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    auto frameBufferHandle = frameGraphBuilder.createPersistentResource(frameBufferDesc);
    frameGraphBuilder.clearFrameBuffer(frameBufferHandle, glm::vec4(0.0f));
    const Render::DownloadImagePass* pDownloadImagePass = frameGraphBuilder.addOperation<Render::DownloadImagePass>()
                                                              .bind<"image">(frameBufferHandle)
                                                              .finalize();

    auto frameGraph = frameGraphBuilder.compile();
    frameGraph.execute();

    const Render::TextureCPU texture = pDownloadImagePass->syncAndGetTexture(renderContext);
    using Pixel = glm::u8vec4;
    REQUIRE(texture.textureFormat == DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(texture.resolution == glm::uvec2(resolution));
    const auto* pPixels = (const Pixel*)texture.pixelData.data();
    for (size_t i = 0; i < resolution * resolution; ++i) {
        REQUIRE(pPixels[i] == Pixel(0, 0, 0, 0)); // All pixels should be black.
    }
}

TEST_CASE("Render::GPU::RasterDebug", "[Render][GPU]")
{
    static constexpr float sphereRadius = 1.0f;
    static constexpr float cameraDistance = 5.0f;
    static constexpr uint32_t resolution = 64;

    Render::RenderContext renderContext {};
    std::vector<Render::TextureCPU> textures { createBasicTexture() };
    std::vector<Render::MeshCPU> meshes { createSphereMesh(glm::vec3(0.0f), sphereRadius) };
    Render::Scene scene;
    auto& meshInstance = scene.meshInstances.emplace_back();
    meshInstance.meshIdx = 0; // Use the first mesh.
    scene.loadFromMeshes(meshes, textures, renderContext);
    scene.camera.aspectRatio = 1.0f;
    scene.camera.fovY = 2 * std::tan(sphereRadius / cameraDistance); // FOVY is set such that the sphere fits in the view frustum.
    scene.camera.transform = Core::Transform::lookAt(glm::vec3(0, 0, -cameraDistance), glm::vec3(0), glm::vec3(0, 1, 0));
    // scene.createBindlessScene(renderContext);
    scene.buildRayTracingAccelerationStructure(renderContext);

    Render::FrameGraphBuilder frameGraphBuilder { &renderContext };
    auto frameBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, resolution, resolution);
    frameBufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    auto frameBufferHandle = frameGraphBuilder.createPersistentResource(frameBufferDesc);

    auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, resolution, resolution);
    depthBufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    auto depthBufferHandle = frameGraphBuilder.createPersistentResource(depthBufferDesc);

    frameGraphBuilder.clearFrameBuffer(frameBufferHandle, glm::vec4(0.0f));
    frameGraphBuilder.clearDepthBuffer(depthBufferHandle, 1.0f);
    frameGraphBuilder.addOperation<Render::RasterDebugPass>({ &scene })
        .bind<"framebuffer">(frameBufferHandle)
        .bind<"depthbuffer">(depthBufferHandle)
        .finalize();
    const Render::DownloadImagePass* pDownloadImagePass = frameGraphBuilder.addOperation<Render::DownloadImagePass>()
                                                              .bind<"image">(frameBufferHandle)
                                                              .finalize();

    auto frameGraph = frameGraphBuilder.compile();
    frameGraph.execute();

    const Render::TextureCPU texture = pDownloadImagePass->syncAndGetTexture(renderContext);
    // texture.saveToFile("raster_debug_sphere.png", Render::TextureFileType::PNG);
    REQUIRE(texture.textureFormat == DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(texture.resolution == glm::uvec2(resolution));
    using Pixel = glm::u8vec4;
    const auto* pPixels = (const Pixel*)texture.pixelData.data();
    for (uint32_t y = 0; y < resolution; ++y) {
        for (uint32_t x = 0; x < resolution; ++x) {
            const float normalizedDistanceFromCenter = glm::length((glm::vec2(x, y) + glm::vec2(0.5f)) / glm::vec2(resolution) - glm::vec2(0.5f));
            // Add some margin to avoid artifacts at the edges.
            if (normalizedDistanceFromCenter < 0.45f) {
                REQUIRE(pPixels[y * resolution + x] == Pixel(127, 127, 127, 255)); // material.baseColor == 0.5f
            } else if (normalizedDistanceFromCenter > 0.55f) {
                REQUIRE(pPixels[y * resolution + x] == Pixel(0));
            }
        }
    }
}

TEST_CASE("Render::GPU::PathTrace", "[Render][GPU]")
{
    static constexpr glm::vec3 spherePosition = glm::vec3(0.0f);
    static constexpr float sphereRadius = 1.0f;
    static constexpr glm::vec3 planePosition = glm::vec3(sphereRadius * 5.0f, 0, 0);
    static constexpr float planeRadius = 0.5f;
    static constexpr float cameraDistance = 5.0f;
    static constexpr uint32_t resolution = 256;
    static constexpr uint32_t samplesPerPixel = 256;

#if WINDOWED
    Core::Window window { "Test", glm::uvec2(resolution), nullptr, 1 };
    Render::RenderContext renderContext { window, false };
#else
    Render::RenderContext renderContext;
#endif
    std::vector<Render::TextureCPU> textures { createBasicTexture() };
    std::vector<Render::MeshCPU> meshes { createSphereMesh(spherePosition, sphereRadius), createPlaneMesh(planePosition, planeRadius) };
    meshes[0].materials[0].baseColor = glm::vec3(1.0f);
    meshes[1].materials[0].baseColor = glm::vec3(1.0f);
    Render::Scene scene;
    scene.meshInstances.emplace_back().meshIdx = 0;
    scene.meshInstances.emplace_back().meshIdx = 1;
    scene.loadFromMeshes(meshes, textures, renderContext);
    scene.camera.aspectRatio = 1.0f;
    scene.camera.fovY = 2 * std::tan(sphereRadius / cameraDistance); // FOVY is set such that the sphere fits in the view frustum.
    // scene.createBindlessScene(renderContext);
    scene.buildRayTracingAccelerationStructure(renderContext);

    Render::FrameGraphBuilder frameGraphBuilder { &renderContext };
    auto frameBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, resolution, resolution);
    frameBufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    auto frameBufferHandle = frameGraphBuilder.createPersistentResource(frameBufferDesc);

    const auto* pPathTracing = frameGraphBuilder.addOperation<Render::PathTracingPass>({ &scene })
                                   .bind<"out">(frameBufferHandle)
                                   .finalize();
    const Render::DownloadImagePass* pDownloadImagePass = frameGraphBuilder.addOperation<Render::DownloadImagePass>()
                                                              .bind<"image">(frameBufferHandle)
                                                              .finalize();

    auto frameGraph = frameGraphBuilder.compile();

    SECTION("Light from camera")
    {
        scene.sun.direction = glm::vec3(0, 0, 1);
        scene.sun.intensity = glm::vec3(1.0f);
        scene.camera.transform = Core::Transform::lookAt(glm::vec3(0, 0, -cameraDistance), spherePosition, glm::vec3(0, 1, 0));
        frameGraph.execute();

        const auto texture = pDownloadImagePass->syncAndGetTexture(renderContext);
        // texture.saveToFile("path_trace_sun_sphere.hdr", Render::TextureFileType::HDR);
        // texture.saveToFile("path_trace_sun_sphere.exr", Render::TextureFileType::OpenEXR);
        REQUIRE(texture.textureFormat == DXGI_FORMAT_R32G32B32A32_FLOAT);
        REQUIRE(texture.resolution == glm::uvec2(resolution));
        using Pixel = glm::vec4;
        const Pixel* pPixels = (const Pixel*)texture.pixelData.data();

        // Normalization factor to ensure that the integral of the BRDF <= 1.0
        constexpr auto oneOverPi = 1.0f / glm::pi<float>();
        REQUIRE(pPixels[resolution / 2 * resolution + resolution / 2].x == Catch::Approx(oneOverPi).margin(0.05f));
    }

    SECTION("Furnace (sphere)")
    {
        scene.sun.intensity = glm::vec3(0.0f);
        scene.optEnvironmentMap = {
            .texture = Render::Texture::uploadToGPU(createHDRTexture(1.0f), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, renderContext),
            .strength = 1.0f
        };
        scene.camera.transform = Core::Transform::lookAt(spherePosition + glm::vec3(0, 0, -cameraDistance), spherePosition, glm::vec3(0, 1, 0));
#if WINDOWED
        while (!window.shouldClose) {
            // for (uint32_t i = 0; i < 3; ++i) {
            window.updateInput();
#else
        for (uint32_t i = 0; i < samplesPerPixel; ++i) {
#endif
            renderContext.waitForNextFrame();
            renderContext.resetFrameAllocators();
            frameGraph.execute();
            renderContext.present();
        }

        auto texture = pDownloadImagePass->syncAndGetTexture(renderContext);
        REQUIRE(texture.textureFormat == DXGI_FORMAT_R32G32B32A32_FLOAT);
        REQUIRE(texture.resolution == glm::uvec2(resolution));

        // Normalize the pixel values by the number of samples per pixel.
        for (uint32_t i = 0; i < resolution * resolution; ++i) {
            auto* pPixel = (glm::vec4*)texture.pixelData.data();
            pPixel[i] /= static_cast<float>(pPathTracing->sampleCount);
        }
        // texture.saveToFile("path_trace_furnace_sphere.hdr", Render::TextureFileType::HDR);
        // texture.saveToFile("path_trace_furnace_sphere.exr", Render::TextureFileType::OpenEXR);

        // The surface of the sphere should not reflect more light than is coming in.
        // However, due to monte carlo noise there may be a small number of very bright pixels.
        // We allow at most 1% of the pixels to be outliers; the rest should be less than the environment light.
        using Pixel = glm::vec4;
        const Pixel* pPixels = (const Pixel*)texture.pixelData.data();
        constexpr size_t numPixels = resolution * resolution;
        size_t numWhitePixels = 0;
        for (size_t i = 0; i < numPixels; ++i) {
            if (pPixels[i].x > 1.0f)
                ++numWhitePixels;
        }
        constexpr size_t numWhitePixelsThreshold = numPixels / 100;
        REQUIRE(numWhitePixels <= numWhitePixelsThreshold);
    }

    SECTION("Furnace (plane)")
    {
        scene.sun.intensity = glm::vec3(0.0f);
        scene.optEnvironmentMap = {
            .texture = Render::Texture::uploadToGPU(createHDRTexture(1.0f), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, renderContext),
            .strength = 1.0f
        };
        scene.camera.transform = Core::Transform::lookAt(planePosition + glm::vec3(0, 0, -cameraDistance), planePosition, glm::vec3(0, 1, 0));
#if WINDOWED
        while (!window.shouldClose) {
            // for (uint32_t i = 0; i < 3; ++i) {
            window.updateInput();
#else
        for (uint32_t i = 0; i < samplesPerPixel; ++i) {
#endif
            renderContext.waitForNextFrame();
            renderContext.resetFrameAllocators();
            frameGraph.execute();
            renderContext.present();
        }

        auto texture = pDownloadImagePass->syncAndGetTexture(renderContext);
        REQUIRE(texture.textureFormat == DXGI_FORMAT_R32G32B32A32_FLOAT);
        REQUIRE(texture.resolution == glm::uvec2(resolution));

        // Normalize the pixel values by the number of samples per pixel.
        for (size_t i = 0; i < resolution * resolution; ++i) {
            auto* pPixel = (glm::vec4*)texture.pixelData.data();
            pPixel[i] /= static_cast<float>(pPathTracing->sampleCount);
        }
        // texture.saveToFile("path_trace_furnace_plane.hdr", Render::TextureFileType::HDR);
        // texture.saveToFile("path_trace_furnace_plane.exr", Render::TextureFileType::OpenEXR);

        // The surface of the sphere should not reflect more light than is coming in.
        // However, due to monte carlo noise there may be a small number of very bright pixels.
        // We allow at most 1% of the pixels to be outliers; the rest should be less than the environment light.
        using Pixel = glm::vec4;
        const Pixel* pPixels = (const Pixel*)texture.pixelData.data();
        constexpr size_t numPixels = resolution * resolution;
        size_t numWhitePixels = 0;
        for (size_t i = 0; i < numPixels; ++i) {
            if (pPixels[i].x > 1.0f)
                ++numWhitePixels;
        }
        constexpr size_t numWhitePixelsThreshold = numPixels / 100;
        REQUIRE(numWhitePixels <= numWhitePixelsThreshold);
    }
}
