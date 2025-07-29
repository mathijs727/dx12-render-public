#include "Engine/Render/Material.h"
#include "Engine/Render/Image.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/ShaderInputs/inputgroups/LambertMaterial.h"
#include "Engine/Render/ShaderInputs/inputgroups/LambertMaterialInstance.h"
#include "Engine/Render/ShaderInputs/inputgroups/PbrMaterial.h"
#include "Engine/Render/ShaderInputs/inputgroups/PbrMaterialInstance.h"
#include <filesystem>
#include <memory>
#include <tbx/hashmap_helper.h>
#include <tbx/variant_helper.h>
#include <unordered_map>
#include <variant>

namespace Render {

MaterialManager::MaterialManager(RenderContext* pRenderContext)
    : m_pRenderContext(pRenderContext)
{
    const auto whiteImageCPU = TypedImage<glm::u8vec4>(glm::vec2(8, 8), glm::u8vec4(255)).toRenderImage();
    m_pWhiteTexture = std::make_shared<TextureGPU>(TextureGPU::fromImage(whiteImageCPU, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, *pRenderContext));
}

MaterialGPU MaterialManager::fromMaterialCPU(const MaterialCPU& materialCPU)
{
    return std::visit(
        Tbx::make_visitor(
            [&](const LambertMaterialDesc& lambertDesc) {
                return createShaderInputs(lambertDesc);
            },
            [&](const PBRMaterialDesc& pbrDesc) {
                LambertMaterialDesc xxx;
                xxx.baseColor = pbrDesc.baseColor;
                xxx.optBaseColorTextureFilePath = pbrDesc.optBaseColorTextureFilePath;
                return createShaderInputs(xxx);
            }),
        materialCPU);
}

MaterialGPU MaterialManager::createShaderInputs(const LambertMaterialDesc& lambertDesc)
{
    MaterialGPU out {
        .materialType = MaterialType::Lambert,
        .pMaterialStatic = &m_staticLambertMaterialInputs
    };

    LambertMaterialInstance instanceDesc {};
    instanceDesc.setDiffuseColor(lambertDesc.baseColor);
    if (lambertDesc.optBaseColorTextureFilePath) {
        const auto texture = getTexture(*lambertDesc.optBaseColorTextureFilePath);
        out.textures.push_back(texture); // Owning pointer.
        instanceDesc.setDiffuseTexture(*texture);
    } else {
        out.textures.push_back(m_pWhiteTexture);
        instanceDesc.setDiffuseTexture(*m_pWhiteTexture);
    }

    out.material = instanceDesc.generateTransientBindings(*m_pRenderContext);
    return out;
}

std::shared_ptr<Render::TextureGPU> MaterialManager::getTexture(const std::filesystem::path& filePath)
{
    if (auto iter = m_textureCache.find(filePath); iter != std::end(m_textureCache)) {
        if (auto pTextureGPU = iter->second.lock())
            return pTextureGPU;
    }

    const auto imageCPU = Image::readFromFile(filePath);
    auto pTextureGPU = std::make_shared<TextureGPU>(TextureGPU::fromImage(imageCPU, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, *m_pRenderContext));
    m_textureCache[filePath] = pTextureGPU;
    return pTextureGPU;
}
}