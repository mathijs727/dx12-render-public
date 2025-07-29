#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/ForwardDeclares.h"
#include "Engine/Render/FrameGraph/RenderPass.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/Render/RenderPasses/ForwardDeclares.h"
#include "Engine/RenderAPI/Internal/D3D12MAHelpers.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/Render/Texture.h"

namespace Render {

class DownloadImagePass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Compute;

    static consteval void declareFrameResources(RenderPassBuilder& builder)
    {
        builder.useResource<"image">(D3D12_RESOURCE_STATE_COPY_SOURCE);
    }

    //void initialize(const Render::RenderContext& renderContext);
    void execute(const FrameGraphRegistry<DownloadImagePass>& registry, const FrameGraphExecuteArgs& args);

    // Waits for the GPU to finish *all* execution (idle) and then returns the last downloaded texture.
    Render::TextureCPU syncAndGetTexture(Render::RenderContext& renderContext) const;

private:
    RenderAPI::D3D12MAResource m_pReadbackBuffer;
    TextureCPU m_emptyDownloadedImage;
    D3D12_SUBRESOURCE_FOOTPRINT m_footprint;
};

}
