#include "Engine/RenderAPI/StateObjectBuilder.h"
#include <algorithm> // std::copy
#include <tbx/error_handling.h>
#include <unordered_map>

namespace RenderAPI {

void StateObjectBuilder::addLibrary(const Shader& shader, LPCWSTR symbol)
{
    addLibrary(shader, std::span(&symbol, 1));
}

void StateObjectBuilder::addLibrary(const Shader& shader, std::span<LPCWSTR> symbols)
{
    auto& library = m_libraries.emplace_back();
    for (const LPCWSTR symbol : symbols) {
        library.exports.push_back(D3D12_EXPORT_DESC {
            .Name = symbol,
            .ExportToRename = nullptr,
            .Flags = D3D12_EXPORT_FLAG_NONE });
    }
    library.desc.DXILLibrary.pShaderBytecode = shader.pBlob->GetBufferPointer();
    library.desc.DXILLibrary.BytecodeLength = shader.pBlob->GetBufferSize();
    library.desc.NumExports = UINT(library.exports.size());
    library.desc.pExports = library.exports.data();
}

void StateObjectBuilder::addRayGenShader(LPCWSTR shaderName, ID3D12RootSignature* pLocalRootSignature)
{
    auto& rayGen = m_rayGenShaders.emplace_back();
    rayGen.shaderName = shaderName;
    if (pLocalRootSignature)
        rayGen.optLocalRootSignature = D3D12_LOCAL_ROOT_SIGNATURE { .pLocalRootSignature = pLocalRootSignature };
}

void StateObjectBuilder::addMissShader(LPCWSTR shaderName, ID3D12RootSignature* pLocalRootSignature)
{
    auto& miss = m_missShader.emplace_back();
    miss.shaderName = shaderName;
    if (pLocalRootSignature)
        miss.optLocalRootSignature = D3D12_LOCAL_ROOT_SIGNATURE { .pLocalRootSignature = pLocalRootSignature };
}

void StateObjectBuilder::addHitGroup(LPCWSTR hitGroupName, const HitGroupShaders& hitGroupShaders, ID3D12RootSignature* pLocalRootSignature)
{
    auto& hitGroup = m_hitGroups.emplace_back();
    hitGroup.desc.HitGroupExport = hitGroupName;
    hitGroup.desc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroup.desc.AnyHitShaderImport = hitGroupShaders.anyHit;
    hitGroup.desc.ClosestHitShaderImport = hitGroupShaders.closestHit;
    hitGroup.desc.IntersectionShaderImport = hitGroupShaders.intersection;
    if (pLocalRootSignature)
        hitGroup.optLocalRootSignature = D3D12_LOCAL_ROOT_SIGNATURE { .pLocalRootSignature = pLocalRootSignature };
}

WRL::ComPtr<ID3D12StateObject> StateObjectBuilder::compile(
    ID3D12Device5* pDevice, ID3D12RootSignature* pGlobalRootSignature, const D3D12_RAYTRACING_SHADER_CONFIG& rayTracingShaderConfig, const D3D12_RAYTRACING_PIPELINE_CONFIG1& rayTracingPipelineConfig)
{
    std::unordered_map<ID3D12RootSignature*, std::vector<LPCWSTR>> localRootSignatureMappings;
    for (const ShaderExport& shader : m_rayGenShaders) {
        if (shader.optLocalRootSignature) {
            const auto pRootSignature = shader.optLocalRootSignature->pLocalRootSignature;
            localRootSignatureMappings[pRootSignature].push_back(shader.shaderName);
        }
    }
    for (const ShaderExport& shader : m_missShader) {
        if (shader.optLocalRootSignature) {
            const auto pRootSignature = shader.optLocalRootSignature->pLocalRootSignature;
            localRootSignatureMappings[pRootSignature].push_back(shader.shaderName);
        }
    }
    for (const HitGroup& hitGroup : m_hitGroups) {
        if (hitGroup.optLocalRootSignature) {
            const auto pRootSignature = hitGroup.optLocalRootSignature->pLocalRootSignature;
            localRootSignatureMappings[pRootSignature].push_back(hitGroup.desc.HitGroupExport);
        }
    }

    // Reserve space to prevent re-allocation which would invalidate the pointers in TO_EXPORT_ASSOCIATION.
    // clang-format off
    const size_t numExpectedSubObjects = 
        m_libraries.size() + // DXIL_LIBRARIES
        m_hitGroups.size() + // HIT_GROUPS
        1 +  // GLOBAL_ROOT_SIGNATURE
        2 * localRootSignatureMappings.size() + // LOCAL_ROOT_SIGNATURE + TO_EXPORT_ASSOCIATION
        2 + // RAYTRACING_SHADER_CONFIG + TO_EXPORT_ASSOCIATION
        1 // RAYTRACING_PIPELINE
    ;
    // clang-format on

    std::vector<D3D12_STATE_SUBOBJECT> subObjects;
    subObjects.reserve(numExpectedSubObjects);

    // DXIL_LIBRARIES
    for (Library& library : m_libraries) {
        library.desc.pExports = library.exports.data(); // Must do in the compile phase due to small-vector optimization.
        subObjects.push_back({ .Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, .pDesc = &library.desc });
    }
    // HIT_GROUPS
    for (const HitGroup& hitGroup : m_hitGroups) {
        subObjects.push_back({ .Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, .pDesc = &hitGroup.desc });
    }

    // GLOBAL_ROOT_SIGNATURE
    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSignature { .pGlobalRootSignature = pGlobalRootSignature };
    subObjects.push_back({ .Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, .pDesc = &globalRootSignature });

    // LOCAL_ROOT_SIGNATURE + TO_EXPORT_ASSOCIATION
    const auto* pPointerBefore = subObjects.data();
    std::vector<D3D12_LOCAL_ROOT_SIGNATURE> localRootSignatures(localRootSignatureMappings.size());
    std::vector<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> localRootSignaturesAssociations(localRootSignatureMappings.size());
    size_t localRootSignatureIdx = 0;
    for (auto& [pLocalRootSignature, exports] : localRootSignatureMappings) {
        D3D12_LOCAL_ROOT_SIGNATURE& localRootSignature = localRootSignatures[localRootSignatureIdx];
        localRootSignature.pLocalRootSignature = pLocalRootSignature;
        subObjects.push_back({ .Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, .pDesc = &localRootSignature });

        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION& association = localRootSignaturesAssociations[localRootSignatureIdx];
        association.pSubobjectToAssociate = &subObjects.back();
        association.NumExports = UINT(exports.size());
        association.pExports = exports.data();
        subObjects.push_back({ .Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, .pDesc = &association });
        ++localRootSignatureIdx;
    }
    // RAYTRACING_SHADER_CONFIG
    subObjects.push_back({ .Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, .pDesc = &rayTracingShaderConfig });

    // TO_EXPORT_ASSOCIATION
    std::vector<LPCWSTR> allExportedSymbols;
    for (const auto& shaderExport : m_rayGenShaders)
        allExportedSymbols.push_back(shaderExport.shaderName);
    for (const auto& shaderExport : m_missShader)
        allExportedSymbols.push_back(shaderExport.shaderName);
    for (const auto& hitGroup : m_hitGroups)
        allExportedSymbols.push_back(hitGroup.desc.HitGroupExport);
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderConfigAssociation {
        .pSubobjectToAssociate = &subObjects.back(),
        .NumExports = UINT(allExportedSymbols.size()),
        .pExports = allExportedSymbols.data()
    };
    subObjects.push_back({ .Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, .pDesc = &shaderConfigAssociation });

    // RAYTRACING_PIPELINE
    subObjects.push_back({ .Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1, .pDesc = &rayTracingPipelineConfig });

    // Check that we did not accidentaly reallocate during one of the push_backs().
    // This would have the effect that the pointers are invalidated, thus causing the TO_EXPORT_ASSOCIATION's to contain broken pointers.
    const auto* pPointerAfter = subObjects.data();
    Tbx::assert_always(pPointerAfter == pPointerBefore);
    Tbx::assert_always(numExpectedSubObjects == subObjects.size());

    const D3D12_STATE_OBJECT_DESC pipelineDesc {
        .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
        .NumSubobjects = UINT(subObjects.size()),
        .pSubobjects = subObjects.data()
    };

    WRL::ComPtr<ID3D12StateObject> pStateObject;
    RenderAPI::ThrowIfFailed(
        pDevice->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&pStateObject)));
    return pStateObject;
}

} // namespace RenderAPI