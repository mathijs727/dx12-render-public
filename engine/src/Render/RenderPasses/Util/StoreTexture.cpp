#include "Engine/Render/RenderPasses/Util/StoreTexture.h"
#include "Engine/Render/FrameGraph/RenderPassBuilder.h"
#include "Engine/Render/Image.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()

using namespace RenderAPI;

namespace Render {

StoreTexturePass::StoreTexturePass(
    RenderPassBuilder& builder,
    FrameGraphResource& inTexture,
    const std::filesystem::path& outFilePath)
    : m_inTexture(builder.copyFrom(inTexture))
    , m_pImage(std::make_shared<Render::Image>())
    , m_optOutFilePath(outFilePath)
{
}

StoreTexturePass::StoreTexturePass(
    RenderPassBuilder& builder,
    FrameGraphResource& inTexture,
    std::shared_ptr<Render::Image> pOutImage)
    : m_inTexture(builder.copyFrom(inTexture))
    , m_pImage(std::move(pOutImage))
{
}

void StoreTexturePass::execute(
    const FrameGraphRegistry& registry, FrameGraphExecuteContext& renderContext)
{
    renderContext.commandList.downloadTexture(registry.getTexture(m_inTexture), m_pImage.get());
}

void StoreTexturePass::postExecute()
{
    if (!m_optOutFilePath)
        return;

    if (!m_optOutFilePath->has_extension()) {
        if (m_pImage->textureFormat == DXGI_FORMAT::R8G8B8A8_unorm)
            m_optOutFilePath = m_optOutFilePath->replace_extension(".png");
        else
            m_optOutFilePath = m_optOutFilePath->replace_extension(".exr");
    }
    m_pImage->saveToFile(*m_optOutFilePath);
}

void StoreTexturePass::setupStaticMembersImpl(Render::RenderContext& graphicsBackend)
{
}

}
