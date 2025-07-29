#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec2.hpp>
DISABLE_WARNINGS_POP()
#include <string_View>

namespace Render {

class ImGuiViewportPass {
public:
    struct Settings {
        const char* pTitle;
        glm::uvec2* pSize;
        bool* pSizeChanged;
        Scene* pScene;
    } settings;

public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;
    static constexpr std::string_view name = "ImGUI (Viewport)";

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"viewport">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    };
    void execute(const FrameGraphRegistry<ImGuiViewportPass>& registry, const FrameGraphExecuteArgs& args);
};

class ImGuiPass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;
    static constexpr std::string_view name = "ImGUI";

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useRenderTarget<"framebuffer">();
    };
    void execute(const FrameGraphRegistry<ImGuiPass>& registry, const FrameGraphExecuteArgs& args);
};

}
