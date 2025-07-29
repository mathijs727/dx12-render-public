#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <Windows.h> // CommandLineToArgvW
#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_stdlib.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <Engine/Core/Keyboard.h>
#include <Engine/Core/Mouse.h>
#include <Engine/Core/Stopwatch.h>
#include <Engine/Core/Transform.h>
#include <Engine/Core/Window.h>
#include <Engine/Render/Camera.h>
#include <Engine/Render/FrameGraph/FrameGraph.h>
#include <Engine/Render/FrameGraph/Operations.h>
#include <Engine/Render/GPUProfiler.h>
#include <Engine/Render/Light.h>
#include <Engine/Render/RenderContext.h>
#include <Engine/Render/RenderPasses/Debug/RandomDebug.h>
#include <Engine/Render/RenderPasses/Debug/RasterDebug.h>
#include <Engine/Render/RenderPasses/Debug/RayTraceDebug.h>
#include <Engine/Render/RenderPasses/Debug/RayTracePipelineDebug.h>
#include <Engine/Render/RenderPasses/Debug/VisualDebug.h>
#include <Engine/Render/RenderPasses/PostProcessing/ColorCorrection.h>
#include <Engine/Render/RenderPasses/PostProcessing/NvidiaDenoise.h>
#include <Engine/Render/RenderPasses/PostProcessing/TAAResolve.h>
#include <Engine/Render/RenderPasses/Rasterization/Deferred.h>
#include <Engine/Render/RenderPasses/Rasterization/DepthOnlyPass.h>
#include <Engine/Render/RenderPasses/Rasterization/Forward.h>
#include <Engine/Render/RenderPasses/Rasterization/ForwardShadowRT.h>
#include <Engine/Render/RenderPasses/Rasterization/MeshShading.h>
#include <Engine/Render/RenderPasses/Rasterization/VisibilityBuffer.h>
#include <Engine/Render/RenderPasses/RayTracing/PathTracing.h>
#include <Engine/Render/RenderPasses/Shared.h>
#include <Engine/Render/RenderPasses/Util/ImguiPass.h>
#include <Engine/Render/RenderPasses/Util/Printf.h>
#include <Engine/Render/Scene.h>
#include <Engine/Render/ShaderHotReload.h>
#include <Engine/Util/ErrorHandling.h>
#include <Engine/Util/FilePicker.h>
#include <Engine/Util/ImguiHelpers.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <stdlib.h> // __argc __argv
#include <tbx/variant_helper.h>
#include <thread>
#include <variant>

#define ENABLE_CRT_LEAK_DETECTION 1

using namespace RenderAPI;

struct AppArguments {
    std::filesystem::path sceneFilePath;
    bool imguiDocking { true };
};

static AppArguments parseApplicationArguments();
static void setupSpdlog();

#pragma warning(disable : 4702)

struct RandomNoisePipeline {
    inline static const char* guiName = "Random Noise";

    void buildFrameGraph(
        Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
        Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard) const
    {
        frameGraphBuilder.addOperation<Render::RandomDebugPass>()
            .bind<"out">(frameBuffer)
            .finalize();
    }
};
struct RasterDebugPipeline {
    inline static const char* guiName = "Raster Debug";

    void buildFrameGraph(
        Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
        Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard) const
    {
        const auto resolution = frameGraphBuilder.getTextureResolution(frameBuffer);

        auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, resolution.x, resolution.y);
        depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        auto depthBuffer = frameGraphBuilder.createTransientResource(depthBufferDesc);
        frameGraphBuilder.clearDepthBuffer(depthBuffer);
        frameGraphBuilder.clearFrameBuffer(frameBuffer);
        auto* pRasterDebug = frameGraphBuilder.addOperation<Render::RasterDebugPass>({ pScene })
                                 .bind<"framebuffer">(frameBuffer)
                                 .bind<"depthbuffer">(depthBuffer)
                                 .finalize();
        auto* pVisualDebug = frameGraphBuilder.addOperation<Render::VisualDebugPass>({ pScene, pKeyboard })
                                 .bind<"framebuffer">(frameBuffer)
                                 .bind<"depthbuffer">(depthBuffer)
                                 .finalize();
        pRasterDebug->settings.pVisualDebugPass = pVisualDebug;
    }
};
struct MeshShadingPipeline {
    inline static const char* guiName = "Mesh Shading";
    inline static bool bindless = true;

    void buildFrameGraph(
        Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
        Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard) const
    {
        const auto resolution = frameGraphBuilder.getTextureResolution(frameBuffer);

        auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, resolution.x, resolution.y);
        depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        auto depthBuffer = frameGraphBuilder.createTransientResource(depthBufferDesc);
        frameGraphBuilder.clearDepthBuffer(depthBuffer);
        frameGraphBuilder.clearFrameBuffer(frameBuffer);
        frameGraphBuilder.addOperation<Render::MeshShadingPass>({ .pScene = pScene, .bindless = bindless })
            .bind<"framebuffer">(frameBuffer)
            .bind<"depthbuffer">(depthBuffer)
            .finalize();
    }

    void displayGUI(bool& changed)
    {
        changed |= ImGui::Checkbox("Bindless", &bindless);
    }
};
struct RayTraceDebugInlinePipeline {
    inline static const char* guiName = "Ray Trace Debug (Inline)";

    void buildFrameGraph(
        Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
        Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard) const
    {
        // Inline ray trace from compute shader.
        frameGraphBuilder.addOperation<Render::RayTraceDebugPass>({ pScene })
            .bind<"out">(frameBuffer)
            .finalize();
    }
};
struct RayTraceDebugPipeline {
    inline static const char* guiName = "Ray Trace Debug";

    void buildFrameGraph(
        Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
        Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard) const
    {
        frameGraphBuilder.addOperation<Render::RayTracePipelineDebugPass>({ pScene })
            .bind<"out">(frameBuffer)
            .finalize();
    }
};
struct ForwardPipeline {
    inline static const char* guiName = "Forward";

    void buildFrameGraph(
        Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
        Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard) const
    {
        const auto resolution = frameGraphBuilder.getTextureResolution(frameBuffer);

        auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, resolution.x, resolution.y);
        depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        auto depthBuffer = frameGraphBuilder.createTransientResource(depthBufferDesc, { .dsvFormat = DXGI_FORMAT_D32_FLOAT, .srvFormat = DXGI_FORMAT_R32_FLOAT });

        auto renderBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, resolution.x, resolution.y);
        renderBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto renderBuffer = frameGraphBuilder.createTransientResource(renderBufferDesc);

        frameGraphBuilder.clearFrameBuffer(renderBuffer);
        frameGraphBuilder.clearDepthBuffer(depthBuffer);
        frameGraphBuilder.addOperation<Render::ForwardPass<false>>({ pScene })
            .bind<"framebuffer">(renderBuffer)
            .bind<"depthbuffer">(depthBuffer)
            .finalize();
        frameGraphBuilder.addOperation<Render::ColorCorrectionPass>()
            .bind<"input">(renderBuffer)
            .bind<"output">(frameBuffer)
            .finalize();
    }
};
struct ForwardTAAPipeline {
    inline static const char* guiName = "Forward with TAA";

    void buildFrameGraph(
        Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
        Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard) const
    {
        const auto resolution = frameGraphBuilder.getTextureResolution(frameBuffer);

        // Forward render showing material base color.
        auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, resolution.x, resolution.y);
        depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        auto depthBuffer = frameGraphBuilder.createTransientResource(depthBufferDesc, { .dsvFormat = DXGI_FORMAT_D32_FLOAT, .srvFormat = DXGI_FORMAT_R32_FLOAT });

        auto velocityBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32_FLOAT, resolution.x, resolution.y);
        velocityBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto velocityBuffer = frameGraphBuilder.createTransientResource(velocityBufferDesc);

        auto offscreenBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, resolution.x, resolution.y);
        offscreenBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto renderBuffer = frameGraphBuilder.createTransientResource(offscreenBufferDesc);
        auto taaResolveBuffer = frameGraphBuilder.createTransientResource(offscreenBufferDesc);

        auto historyBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, resolution.x, resolution.y);
        historyBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto historyBuffer = frameGraphBuilder.createPersistentResource(historyBufferDesc);

        auto historyDepthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_FLOAT, resolution.x, resolution.y);
        historyDepthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto historyDepthBuffer = frameGraphBuilder.createPersistentResource(historyDepthBufferDesc);

        frameGraphBuilder.clearDepthBuffer(depthBuffer);
        frameGraphBuilder.clearFrameBuffer(renderBuffer);
        frameGraphBuilder.clearFrameBuffer(velocityBuffer);
        frameGraphBuilder.addOperation<Render::ForwardPass<true>>({ pScene })
            .bind<"framebuffer">(renderBuffer)
            .bind<"velocity">(velocityBuffer)
            .bind<"depthbuffer">(depthBuffer)
            .finalize();

        // Apply TAA and update the history data.
        frameGraphBuilder.addOperation<Render::TAAResolvePass>({ pScene })
            .bind<"frameBuffer">(renderBuffer)
            .bind<"depth">(depthBuffer)
            .bind<"velocity">(velocityBuffer)
            .bind<"history">(historyBuffer)
            .bind<"historyDepth">(historyDepthBuffer)
            .bind<"output">(taaResolveBuffer)
            .finalize();
        frameGraphBuilder.addOperation<Render::CopyTexture>()
            .bind<"source">(taaResolveBuffer)
            .bind<"dest">(historyBuffer)
            .finalize();
        frameGraphBuilder.addOperation<Render::ShaderCopyTexture>()
            .bind<"source">(depthBuffer)
            .bind<"dest">(historyDepthBuffer)
            .finalize();

        frameGraphBuilder.addOperation<Render::ColorCorrectionPass>()
            .bind<"input">(taaResolveBuffer)
            .bind<"output">(frameBuffer)
            .finalize();
    }
};
struct ForwardShadowRTPipeline {
    inline static const char* guiName = "Forward with RT shadows";

    void buildFrameGraph(
        Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
        Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard) const
    {
        const auto resolution = frameGraphBuilder.getTextureResolution(frameBuffer);

        auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, resolution.x, resolution.y);
        depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        auto depthBuffer = frameGraphBuilder.createTransientResource(depthBufferDesc, { .dsvFormat = DXGI_FORMAT_D32_FLOAT, .srvFormat = DXGI_FORMAT_R32_FLOAT });

        auto renderBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, resolution.x, resolution.y);
        renderBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto renderBuffer = frameGraphBuilder.createTransientResource(renderBufferDesc);

        // Clear initial buffers.
        frameGraphBuilder.clearDepthBuffer(depthBuffer);
        frameGraphBuilder.clearFrameBuffer(renderBuffer);

        frameGraphBuilder.addOperation<Render::ForwardShadowRTPass>({ pScene })
            .bind<"depthbuffer">(depthBuffer)
            .bind<"framebuffer">(renderBuffer)
            .finalize();

        frameGraphBuilder.addOperation<Render::ColorCorrectionPass>()
            .bind<"input">(renderBuffer)
            .bind<"output">(frameBuffer)
            .finalize();
    }
};
struct DeferredPipeline {
    inline static const char* guiName = "Deferred";
    enum class Channel {
        VisibilityBuffer,
        BaseColor,
        Position,
        Metallic,
        Normal,
        Roughness,
        SunVisibility,
        Final
    };
    inline static Channel displayChannel = Channel::Final;
    inline static bool useVisibilityBuffer = true;

    void buildFrameGraph(
        Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
        Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard) const
    {
        const auto resolution = frameGraphBuilder.getTextureResolution(frameBuffer);

        auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, resolution.x, resolution.y);
        depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        auto depthBuffer = frameGraphBuilder.createTransientResource(depthBufferDesc, { .dsvFormat = DXGI_FORMAT_D32_FLOAT, .srvFormat = DXGI_FORMAT_R32_FLOAT });

        auto gbufferDesc32 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, resolution.x, resolution.y);
        auto gbufferDesc16 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, resolution.x, resolution.y);
        auto gbufferDesc8 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, resolution.x, resolution.y);
        gbufferDesc32.Flags = gbufferDesc16.Flags = gbufferDesc8.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto position_metallicBuffer = frameGraphBuilder.createTransientResource(gbufferDesc32);
        auto normal_alphaBuffer = frameGraphBuilder.createTransientResource(gbufferDesc16);
        auto baseColorBuffer = frameGraphBuilder.createTransientResource(gbufferDesc16);

        frameGraphBuilder.clearDepthBuffer(depthBuffer);

        auto* pDebugPrint = frameGraphBuilder.addOperation<Render::PrintfPass>().finalize();

        if (useVisibilityBuffer) {
            // Render to an intermediate visibility buffer, then sample the textures to fill the GBuffer.
            auto visibilityBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32_UINT, resolution.x, resolution.y);
            visibilityBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            auto visibilityBuffer = frameGraphBuilder.createTransientResource(visibilityBufferDesc);
            frameGraphBuilder.clearFrameBuffer(visibilityBuffer);
            frameGraphBuilder.addOperation<Render::VisiblityBufferRenderPass>({ pScene, pDebugPrint })
                .bind<"depthbuffer">(depthBuffer)
                .bind<"visibilityBuffer">(visibilityBuffer)
                .finalize();
            frameGraphBuilder.addOperation<Render::VisiblityToGBufferPass>({ pScene, pDebugPrint })
                .bind<"visibilityBuffer">(visibilityBuffer)
                .bind<"position_metallic">(position_metallicBuffer)
                .bind<"normal_alpha">(normal_alphaBuffer)
                .bind<"baseColor">(baseColorBuffer)
                .finalize();

            if (displayChannel == Channel::VisibilityBuffer) {
                frameGraphBuilder.addOperation<Render::CopyTextureChannels>()
                    .bind<"source">(visibilityBuffer)
                    .bind<"dest">(frameBuffer)
                    .finalize();
                return;
            }
        } else {
            // Render directly into the GBuffer
            frameGraphBuilder.clearFrameBuffer(baseColorBuffer);
            frameGraphBuilder.addOperation<Render::DeferredRenderPass>({ pScene })
                .bind<"position_metallic">(position_metallicBuffer)
                .bind<"normal_alpha">(normal_alphaBuffer)
                .bind<"baseColor">(baseColorBuffer)
                .bind<"depthbuffer">(depthBuffer)
                .finalize();
        }

        if (displayChannel == Channel::BaseColor) {
            frameGraphBuilder.addOperation<Render::ShaderCopyTexture>()
                .bind<"source">(baseColorBuffer)
                .bind<"dest">(frameBuffer)
                .finalize();
            return;
        } else if (displayChannel == Channel::Position) {
            frameGraphBuilder.addOperation<Render::CopyTextureChannels>({ .r = 0u, .g = 1u, .b = 2u })
                .bind<"source">(position_metallicBuffer)
                .bind<"dest">(frameBuffer)
                .finalize();
            return;
        } else if (displayChannel == Channel::Metallic) {
            frameGraphBuilder.addOperation<Render::CopyTextureChannels>({ .r = 3u, .g = 3u, .b = 3u })
                .bind<"source">(position_metallicBuffer)
                .bind<"dest">(frameBuffer)
                .finalize();
            return;
        } else if (displayChannel == Channel::Normal) {
            frameGraphBuilder.addOperation<Render::CopyTextureChannels>({ .r = 0u, .g = 1u, .b = 2u, .offset = 0.5f, .scaling = 0.5f })
                .bind<"source">(normal_alphaBuffer)
                .bind<"dest">(frameBuffer)
                .finalize();
            return;
        } else if (displayChannel == Channel::Roughness) {
            frameGraphBuilder.addOperation<Render::CopyTextureChannels>({ .r = 3u, .g = 3u, .b = 3u })
                .bind<"source">(normal_alphaBuffer)
                .bind<"dest">(frameBuffer)
                .finalize();
            return;
        }

        auto sunVisibilityBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8_UNORM, resolution.x, resolution.y);
        sunVisibilityBufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        auto sunVisibilityBuffer = frameGraphBuilder.createTransientResource(sunVisibilityBufferDesc);
        // frameGraphBuilder.clearFrameBuffer(sunVisibilityBuffer);
        frameGraphBuilder.addOperation<Render::SunVisibilityRTPass>({ pScene })
            .bind<"position_metallic">(position_metallicBuffer)
            .bind<"sunVisibility">(sunVisibilityBuffer)
            .finalize();

        if (displayChannel == Channel::SunVisibility) {
            frameGraphBuilder.addOperation<Render::CopyTextureChannels>({ .r = 0u, .g = 0u, .b = 0u })
                .bind<"source">(sunVisibilityBuffer)
                .bind<"dest">(frameBuffer)
                .finalize();
            return;
        }

        auto renderBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, resolution.x, resolution.y);
        renderBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto renderBuffer = frameGraphBuilder.createTransientResource(renderBufferDesc);
        frameGraphBuilder.clearFrameBuffer(renderBuffer);
        frameGraphBuilder.addOperation<Render::DeferredShadingPass>({ pScene })
            .bind<"position_metallic">(position_metallicBuffer)
            .bind<"normal_alpha">(normal_alphaBuffer)
            .bind<"baseColor">(baseColorBuffer)
            .bind<"sunVisibility">(sunVisibilityBuffer)
            .bind<"framebuffer">(renderBuffer)
            .finalize();

        frameGraphBuilder.addOperation<Render::ColorCorrectionPass>()
            .bind<"input">(renderBuffer)
            .bind<"output">(frameBuffer)
            .finalize();
    }

    void displayGUI(bool& changed)
    {
        changed |= Util::imguiCombo("Display", displayChannel);
        changed |= ImGui::Checkbox("Visibility Buffer", &useVisibilityBuffer);
        // Cannot display buffer when it is not used.
        if (displayChannel == Channel::VisibilityBuffer && !useVisibilityBuffer)
            displayChannel = Channel::Final;
    }
};
struct PathTracingPipeline {
    inline static const char* guiName = "Path Tracing";

    inline static bool enableVisualDebug = false;

    void buildFrameGraph(
        Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
        Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard) const
    {
        const auto resolution = frameGraphBuilder.getTextureResolution(frameBuffer);

        // Ray tracing pipeline showing material base color.
        auto hdrBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, resolution.x, resolution.y);
        hdrBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        auto hdrBuffer = frameGraphBuilder.createPersistentResource(hdrBufferDesc);
        auto* pPathTracing = frameGraphBuilder.addOperation<Render::PathTracingPass>({ .pScene = pScene })
                                 .bind<"out">(hdrBuffer)
                                 .finalize();
        frameGraphBuilder.addOperation<Render::ColorCorrectionPass>({ .pSampleCount = &pPathTracing->sampleCount })
            .bind<"input">(hdrBuffer)
            .bind<"output">(frameBuffer)
            .finalize();

        auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, resolution.x, resolution.y);
        depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        auto depthBuffer = frameGraphBuilder.createTransientResource(depthBufferDesc);

        frameGraphBuilder.clearDepthBuffer(depthBuffer);
        //  Use the raster debug pass to fill the depth buffer.
        frameGraphBuilder.addOperation<Render::DepthOnlyPass>({ pScene, Render::getCameraViewProjection(pScene->camera) })
            .bind<"depthbuffer">(depthBuffer)
            .finalize();

        if (enableVisualDebug) {
            auto* pVisualDebug = frameGraphBuilder.addOperation<Render::VisualDebugPass>({ pScene, pKeyboard })
                                     .bind<"framebuffer">(frameBuffer)
                                     .bind<"depthbuffer">(depthBuffer)
                                     .finalize();
            pPathTracing->settings.pVisualDebugPass = pVisualDebug;
        }
    }

    void displayGUI(bool& changed)
    {
        changed |= ImGui::Checkbox("Visual Debugging", &enableVisualDebug);
    }
};

using RenderPipeline = std::variant<
    RandomNoisePipeline, RasterDebugPipeline, MeshShadingPipeline, RayTraceDebugInlinePipeline, RayTraceDebugPipeline,
    ForwardPipeline, ForwardTAAPipeline, ForwardShadowRTPipeline, DeferredPipeline, PathTracingPipeline>;

template <size_t i>
std::array<const char*, std::variant_size_v<RenderPipeline>> getRenderPipelineNames()
{
    std::array<const char*, std::variant_size_v<RenderPipeline>> out;
    if constexpr (i < std::variant_size_v<RenderPipeline>) {
        out = getRenderPipelineNames<i + 1>();
        out[i] = std::variant_alternative_t<i, RenderPipeline>::guiName;
    }
    return out;
}

struct FrameGraphSettings {
    RenderPipeline pipeline = DeferredPipeline {};
};
void displayFrameGraphSettings(FrameGraphSettings& frameGraphSettings, bool& changed)
{
    const auto names = getRenderPipelineNames<0>();
    int pipelineIdx = (int)frameGraphSettings.pipeline.index();
    if (Util::imguiCombo<struct RenderPipelineCombo>("Render Pipeline", std::span(names), pipelineIdx)) {
        Tbx::setVariantIndex(frameGraphSettings.pipeline, pipelineIdx);
        changed |= true;
    }

    std::visit(
        Tbx::make_visitor([&]<typename T>(T& pipeline) {
            if constexpr (requires { pipeline.displayGUI(changed); })
                return pipeline.displayGUI(changed);
        }),
        frameGraphSettings.pipeline);
}

static void buildFrameGraph(
    const FrameGraphSettings& settings, Render::FrameGraphBuilder& frameGraphBuilder, uint32_t frameBuffer,
    Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard)
{
    std::visit(
        Tbx::make_visitor(
            [&](const auto& renderPipeline) {
                renderPipeline.buildFrameGraph(frameGraphBuilder, frameBuffer, renderContext, pScene, pKeyboard);
            }),
        settings.pipeline);
}

[[maybe_unused]] static Render::FrameGraph buildFrameGraph(
    const FrameGraphSettings& settings, Render::RenderContext& renderContext, Render::Scene* pScene, const Core::Keyboard* pKeyboard)
{
    Render::FrameGraphBuilder frameGraphBuilder { &renderContext };
    uint32_t finalFrameBuffer = frameGraphBuilder.getSwapChainResource();
    buildFrameGraph(settings, frameGraphBuilder, finalFrameBuffer, renderContext, pScene, pKeyboard);
    return frameGraphBuilder.compile();
}

struct ShouldUpdate {
    bool resizeSwapChain;
    bool rebuildFrameGraph;
    glm::uvec2 viewportResolution;
};

static Render::FrameGraph buildFrameGraphImGuiWindowed(
    const FrameGraphSettings& settings,
    Render::RenderContext& renderContext,
    Render::Scene* pScene,
    const Core::Keyboard* pKeyboard,
    ShouldUpdate* pShouldUpdate)
{
    Render::FrameGraphBuilder frameGraphBuilder { &renderContext };
    const auto finalFrameBuffer = frameGraphBuilder.getSwapChainResource();

    auto intermediateFrameBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, pShouldUpdate->viewportResolution.x, pShouldUpdate->viewportResolution.y);
    intermediateFrameBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    auto intermediateFrameBuffer = frameGraphBuilder.createTransientResource(intermediateFrameBufferDesc);

    frameGraphBuilder.clearFrameBuffer(finalFrameBuffer);
    buildFrameGraph(settings, frameGraphBuilder, intermediateFrameBuffer, renderContext, pScene, pKeyboard);
    frameGraphBuilder.addOperation<Render::ImGuiViewportPass>({ .pTitle = "Render", .pSize = &pShouldUpdate->viewportResolution, .pSizeChanged = &pShouldUpdate->rebuildFrameGraph, .pScene = pScene })
        .bind<"viewport">(intermediateFrameBuffer)
        .finalize();
    frameGraphBuilder.addOperation<Render::ImGuiPass>()
        .bind<"framebuffer">(finalFrameBuffer)
        .finalize();
    return frameGraphBuilder.compile();
}

void mainFunc(HINSTANCE hInstance, int nCmdShow)
{
    const auto args = parseApplicationArguments();

    spdlog::info("Create Window and initialize DirectX");
    auto* pImGuiContext = ImGui::CreateContext();
    ImGuiIO& imGuiIO = ImGui::GetIO();
    imGuiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (args.imguiDocking)
        imGuiIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    Core::Window window { "Editor", glm::uvec2(1280, 720), hInstance, nCmdShow };
    Render::RenderContext renderContext { window };
    ImGui_ImplWin32_Init(window.hWnd);

    Core::Mouse mouse { window };
    Core::Keyboard keyboard { window };
    Core::FPSCameraControls cameraControls { &mouse, &keyboard };

    spdlog::info("Load Scene");
    Render::Scene scene {};
    if (args.sceneFilePath.extension() == ".gltf") {
        scene.loadFromGLTF(args.sceneFilePath, renderContext);
    } else if (args.sceneFilePath.extension() == ".glb") {
        scene.loadFromGLB(args.sceneFilePath, renderContext);
    } else if (args.sceneFilePath.extension() == ".bin") {
        scene.loadFromBinary(args.sceneFilePath, renderContext);
    } else {
        Util::ThrowError(fmt::format("Unknown scene file extension \"{}\". Must be either .gltf or .glb", args.sceneFilePath.extension().string()));
    }
    scene.camera.zFar = 100.0f;
    spdlog::info("Build acceleration structure");
    scene.buildRayTracingAccelerationStructure(renderContext);

    std::optional<Render::FrameGraph> frameGraph;
    FrameGraphSettings settings {};

    Render::ShaderHotReload shaderHotReload(
        ENGINE_SHADER_SOURCE_DIR, "shaders", BUILD_DIR, "Editor");
    ShouldUpdate shouldUpdate { .rebuildFrameGraph = true, .viewportResolution = glm::uvec2(1280, 720) };
    window.registerResizeCallback([&](const glm::uvec2& resolution) {
        shouldUpdate.resizeSwapChain = true;
        shouldUpdate.rebuildFrameGraph = true;
    });

    Core::Stopwatch stopwatch;
    Render::GPUFrameProfiler gpuProfiler { renderContext, 32 };
    spdlog::info("Start render");
    while (!window.shouldClose && !keyboard.isKeyPress(Core::Key::ESCAPE)) {
        renderContext.waitForNextFrame();
        renderContext.resetFrameAllocators();
        window.updateInput(true);
        keyboard.setIgnoreImGuiEvents(false);
        scene.updateHistoricalTransformMatrices();

        if (shaderHotReload.shouldReloadShaders()) {
            spdlog::debug("Shaders changed");
            shouldUpdate.rebuildFrameGraph = true;
        }

        // Rebuild the frame graph if necessary (e.g. window resize).
        if (shouldUpdate.rebuildFrameGraph) {
            renderContext.waitForIdle();
            if (shouldUpdate.resizeSwapChain) {
                frameGraph.reset();
                renderContext.resizeSwapChain(window.size);
            }
            scene.camera.aspectRatio = (float)shouldUpdate.viewportResolution.x / (float)shouldUpdate.viewportResolution.y;
            frameGraph = buildFrameGraphImGuiWindowed(settings, renderContext, &scene, &keyboard, &shouldUpdate);
            shouldUpdate.rebuildFrameGraph = false;
            shouldUpdate.resizeSwapChain = false;
        }

        // Control the camera with FPS style camera controls.
        const auto frameTime = stopwatch.restart();
        cameraControls.tick(frameTime.count(), scene.camera.transform);

        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (args.imguiDocking) {
            Util::imguiDockingLayout(
                ImGuiWindowFlags_None,
                [](ImGuiID dockSpaceID) {
                    // Split root node into left/right parts
                    ImGuiID dockLeftTopID, dockLeftBottomID, dockRightID;
                    ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Left, 0.3f, &dockLeftTopID, &dockRightID);
                    ImGui::DockBuilderSplitNode(dockLeftTopID, ImGuiDir_Down, 0.5f, &dockLeftBottomID, &dockLeftTopID);

                    ImGui::DockBuilderDockWindow("Frame Graph", dockLeftTopID);
                    ImGui::DockBuilderDockWindow("Profiler", dockLeftBottomID);
                    ImGui::DockBuilderDockWindow("Render", dockRightID);
                });
        }

        ImGui::Begin("Profiler");
        gpuProfiler.displayVerticalGUI();
        ImGui::End();

        ImGui::Begin("Frame Graph");
        displayFrameGraphSettings(settings, shouldUpdate.rebuildFrameGraph);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::InputFloat3("Sun Intensity", glm::value_ptr(scene.sun.intensity));
        ImGui::InputFloat3("Sun Direction", glm::value_ptr(scene.sun.direction));
        scene.sun.direction = glm::normalize(scene.sun.direction);
        ImGui::Text("Environment Map");
        if (ImGui::Button("Load")) {
            if (auto optEnvironmentMapFilePath = Util::pickOpenFile({ "Image", "exr,hdr" })) {
                const auto textureType = optEnvironmentMapFilePath->extension() == ".exr" ? Render::TextureFileType::OpenEXR : Render::TextureFileType::HDR;
                const auto cpuTexture = Render::TextureCPU::readFromFile(*optEnvironmentMapFilePath, { .fileType = textureType });
                {
                    renderContext.waitForIdle();
                    scene.optEnvironmentMap.reset();
                }
                scene.optEnvironmentMap = Render::EnvironmentMap {
                    .texture = Render::Texture::uploadToGPU(cpuTexture, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, renderContext),
                    .strength = 1.0f
                };
            }
        }
        ImGui::SameLine();
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !scene.optEnvironmentMap.has_value());
        if (ImGui::Button("Clear")) {
            renderContext.waitForIdle();
            scene.optEnvironmentMap.reset();
        }
        ImGui::PopItemFlag();
        if (scene.optEnvironmentMap) {
            ImGui::InputFloat("Strength", &scene.optEnvironmentMap->strength, 0.1f, 0.5f);
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        frameGraph->displayGUI();
        ImGui::End();

        frameGraph->execute(&gpuProfiler);
        renderContext.present();
    }

    RenderAPI::waitForIdle(renderContext.graphicsFence, renderContext.pGraphicsQueue.Get());
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(pImGuiContext);
}

static void openOutputConsole()
{
    DISABLE_WARNINGS_PUSH()
    // https://stackoverflow.com/questions/2501968/visual-c-enable-console
    // Alloc Console
    // print some stuff to the console
    // make sure to include #include "stdio.h"
    // note, you must use the #include <iostream>/ using namespace std
    // to use the iostream... #include "iostream.h" didn't seem to work
    // in my VC 6
    AllocConsole();
    freopen("conin$", "r", stdin);
    freopen("conout$", "w", stdout);
    freopen("conout$", "w", stderr);
    DISABLE_WARNINGS_POP()
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
#if !ENABLE_CRT_LEAK_DETECTION
    // Something (I'm suspecting assimp or it's dependencies) is enabling leak detection at shutdown.
    // This is a problem because assimps dependencies have some global variables that are dynamically initialized
    // resulting in memory allocations. The CRT memory leak detector outputs a ton of warnings about these variables
    // even though it's totally irrelevant since they're global variables and I'm not unloading the assimp dll anyways.
    _CrtSetDbgFlag(0);
#endif

    openOutputConsole();
    setupSpdlog();

    mainFunc(hInstance, nCmdShow);
}

static void setupSpdlog()
{
    // auto visualStudioSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    // auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt");
    // visualStudioSink->set_level(spdlog::level::info);
    consoleSink->set_level(spdlog::level::debug);
    // fileSink->set_level(spdlog::level::debug);

    std::vector<spdlog::sink_ptr> sinks;
    // sinks.push_back(visualStudioSink);
    sinks.push_back(consoleSink);
    // sinks.push_back(fileSink);
    auto combinedLogger = std::make_shared<spdlog::logger>("Logger", std::begin(sinks), std::end(sinks));
    combinedLogger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(combinedLogger);
}

AppArguments parseApplicationArguments()
{
    AppArguments out {};
    CLI::App app { "Convert *.si files into *.cpp and *.hlsl files" };
    app.add_option("scene", out.sceneFilePath, "Scene file (must be either .gltf or .glb)")->required();
    app.add_flag("--docking", out.imguiDocking, "Use ImGui docking mode");
    try {
        app.parse(__argc, __argv);
    } catch (const CLI::ParseError& e) {
        app.exit(e);
        Util::ThrowError("Invalid arguments");
    }

    if (!std::filesystem::exists(out.sceneFilePath)) {
        spdlog::warn("File {} does not exist", out.sceneFilePath.string());
        if (const auto optSceneFilePath = Util::pickOpenFile()) {
            out.sceneFilePath = *optSceneFilePath;
        } else {
            exit(1);
        }
    }

    return out;
}
