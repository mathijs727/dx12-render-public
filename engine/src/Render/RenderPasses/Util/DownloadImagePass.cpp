#include "Engine/Render/RenderPasses/Util/DownloadImagePass.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Util/ErrorHandling.h"
#include <array>
#include <cassert>

namespace Render {
void DownloadImagePass::execute(const FrameGraphRegistry<DownloadImagePass>& resources, const FrameGraphExecuteArgs& args)
{
    ID3D12Resource* pSource = resources.getTextureSRV<"image">().pResource;

    if (!m_pReadbackBuffer) {
        // Create the GPU upload buffer
        constexpr size_t maxMipLevel = 1;
        uint32_t numRows;
        uint64_t bytesPerRow;
        uint64_t totalBytes = 0;
        // args.pRenderContext->pDevice->GetCopyableFootprints(&fgResource.desc, 0, fgResource.desc.MipLevels, 0, layouts.data(), numRows.data(), bytesPerRow.data(), &totalBytes);
        const auto desc = pSource->GetDesc();
        args.pRenderContext->pDevice->GetCopyableFootprints(&desc, 0, 1, 0, nullptr, &numRows, &bytesPerRow, &totalBytes);

        // Round up the srcPitch to multiples of 256
        const auto dstBytesPerRow = (bytesPerRow + 255) & ~0xFFu;

        const auto readbackBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dstBytesPerRow * desc.Height, D3D12_RESOURCE_FLAG_NONE, 0);
        m_pReadbackBuffer = args.pRenderContext->createResource(D3D12_HEAP_TYPE_READBACK, readbackBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST);
        m_pReadbackBuffer->SetName(L"DownloadImagePass::m_pReadbackBuffer");

        // Initialize the downloaded image with the correct resolution and format.
        m_emptyDownloadedImage.resolution = glm::uvec2(desc.Width, desc.Height);
        m_emptyDownloadedImage.textureFormat = desc.Format;
        m_emptyDownloadedImage.isOpague = true;
        m_emptyDownloadedImage.mipLevels.push_back(TextureCPU::MipLevel {
            .mipLevelStart = 0,
            .rowPitch = (uint32_t)bytesPerRow });

        m_footprint = D3D12_SUBRESOURCE_FOOTPRINT {
            .Format = desc.Format,
            .Width = (UINT)desc.Width,
            .Height = desc.Height,
            .Depth = 1,
            .RowPitch = (UINT)dstBytesPerRow
        };
    }

    const auto src = CD3DX12_TEXTURE_COPY_LOCATION(pSource, 0);
    const auto dst = CD3DX12_TEXTURE_COPY_LOCATION(m_pReadbackBuffer, { .Offset = 0, .Footprint = m_footprint });
    args.pCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}

Render::TextureCPU DownloadImagePass::syncAndGetTexture(Render::RenderContext& renderContext) const
{
    renderContext.waitForIdle();

    D3D12_RANGE readRange { 0, m_footprint.Width };
    void* pMappedBuffer = nullptr;
    RenderAPI::ThrowIfFailed(m_pReadbackBuffer->Map(0, &readRange, &pMappedBuffer));

    Render::TextureCPU downloadedImage = m_emptyDownloadedImage;
    downloadedImage.pixelData.resize(m_footprint.RowPitch * m_footprint.Height);
    std::memcpy((void*)downloadedImage.pixelData.data(), pMappedBuffer, downloadedImage.pixelData.size());

    m_pReadbackBuffer->Unmap(0, nullptr);
    return downloadedImage;
}

}
