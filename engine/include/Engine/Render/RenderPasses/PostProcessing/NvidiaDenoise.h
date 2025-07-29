#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <memory>
#include <string_view>
#include <tbx/disable_all_warnings.h>
#include <vector>
DISABLE_WARNINGS_PUSH()
#include <glm/vec2.hpp>
DISABLE_WARNINGS_POP()

namespace nrd {
struct Instance;
}

namespace Render {

class NvidiaDenoisePass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Compute;
    static constexpr std::string_view name = "NVIDIA Real-Time Denoiser (NRD)";

    struct Settings {
        const Render::Scene* pScene;
        glm::ivec2 resolution;
    } settings;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"in_mv">(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        builder.useResource<"in_normal_roughness">(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        builder.useResource<"in_viewz">(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        builder.useResource<"in_diff_radiance_hitdist">(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        builder.useResource<"out_diff_radiance_hitdist">(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        builder.useResource<"out_validation">(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        //builder.useResource<"output">(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    void initialize(Render::RenderContext& renderContext);
    void destroy(RenderContext& renderContext);
    void execute(const FrameGraphRegistry<NvidiaDenoisePass>& registry, const FrameGraphExecuteArgs& args);
    void displayGUI();

private:
    nrd::Instance* m_pNrdInstance;

    struct DispatchData {
        WRL::ComPtr<ID3D12RootSignature> pRootSignature;
        WRL::ComPtr<ID3D12PipelineState> pPipelineState;
        std::unique_ptr<wchar_t[]> pName;

        uint32_t texturesOffsetInDescriptorTable = 0, storageTexturesOffsetInDescriptorTable = 0, descriptorTableSize = 0;
    };
    std::vector<DispatchData> m_dispatchDatas;

    struct Texture {
        RenderAPI::D3D12MAResource pResource;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        D3D12_RESOURCE_STATES resourceState;
    };
    std::vector<Texture> m_persistentTextures;
    std::vector<Texture> m_transientTextures;

    WRL::ComPtr<ID3D12RootSignature> m_pDecodeRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pDecodePipelineState;

    uint16_t m_mouseX = 0, m_mouseY = 0;
    uint32_t m_frameIndex = 0;
};

class NvidiaDenoiseDecodePass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Compute;
    static constexpr std::string_view name = "NRD Decode";

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"in_diff_radiance_hitdist">(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        builder.useResource<"framebuffer">(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    void initialize(Render::RenderContext& renderContext);
    void execute(const FrameGraphRegistry<NvidiaDenoiseDecodePass>& registry, const FrameGraphExecuteArgs& args);

private:
    WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
    WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
};

}
