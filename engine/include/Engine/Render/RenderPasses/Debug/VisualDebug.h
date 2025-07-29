#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include "Engine/Render/Debug.h"
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/Render/ShaderInputs/groups/VisualDebug.h"
#include "Engine/RenderAPI/Internal/D3D12MAHelpers.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()

namespace Render {

struct DebugArrow {
    glm::vec3 from, to;
    glm::vec3 color;
};

class VisualDebugPass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Graphics;

    struct Settings {
        const Scene* pScene;
        const Core::Keyboard* pKeyboard { nullptr };
    } settings;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useRenderTarget<"framebuffer">();
        builder.depthStencilWrite<"depthbuffer">();
    }
    void initialize(RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc);
    void execute(const FrameGraphRegistry<VisualDebugPass>& registry, const FrameGraphExecuteArgs& args);
    void displayGUI();

    ShaderInputs::VisualDebug getShaderInputs() const;
    void togglePause();

private:
    struct Drawable {
        RenderAPI::D3D12MAResource pVertexBuffer, pIndexBuffer;
        D3D12_INDEX_BUFFER_VIEW indexBufferView;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
        uint32_t numIndices;
    };
    static Drawable createDrawable(std::span<const uint32_t> indices, std::span<const ShaderInputs::Vertex> vertices, RenderContext& renderContext);
    void clearCommandBuffer(RenderContext& renderContext, ID3D12GraphicsCommandList* pCommandList);

private:
    WRL::ComPtr<ID3D12CommandSignature> m_pCommandSignature;
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;

    Drawable m_arrow;
    bool m_firstFrame { true };

    RenderAPI::D3D12MAResource m_pCommandBuffer;
    RenderAPI::D3D12MAResource m_pConstantsBuffer;

    ShaderInputs::VisualDebug m_shaderInputs;

    inline static bool m_paused { false };
    bool m_shouldPause { false };
};

}
