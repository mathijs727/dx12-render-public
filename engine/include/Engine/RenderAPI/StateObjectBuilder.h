#pragma once
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/Shader.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <EASTL/fixed_vector.h>
DISABLE_WARNINGS_POP()
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace RenderAPI {

class StateObjectBuilder {
public:
    struct HitGroupShaders {
        LPCWSTR anyHit = nullptr;
        LPCWSTR closestHit = nullptr;
        LPCWSTR intersection = nullptr;
    };

    void addLibrary(const Shader& shader, LPCWSTR symbol);
    void addLibrary(const Shader& shader, std::span<LPCWSTR> symbols);

    void addRayGenShader(LPCWSTR shaderName, ID3D12RootSignature* pLocalRootSignature = nullptr);
    void addMissShader(LPCWSTR shaderName, ID3D12RootSignature* pLocalRootSignature = nullptr);
    void addHitGroup(LPCWSTR hitGroupName, const HitGroupShaders& hitGroupShaders, ID3D12RootSignature* pLocalRootSignature = nullptr);

    WRL::ComPtr<ID3D12StateObject> compile(
        ID3D12Device5* pDevice,
        ID3D12RootSignature* pGlobalRootSignature,
        const D3D12_RAYTRACING_SHADER_CONFIG& rayTracingShaderConfig,
        const D3D12_RAYTRACING_PIPELINE_CONFIG1& rayTracingPipelineConfig);

private:
    struct Library {
        D3D12_DXIL_LIBRARY_DESC desc;
        eastl::fixed_vector<D3D12_EXPORT_DESC, 4, true> exports;
    };
    std::vector<Library> m_libraries;

    struct ShaderExport {
        LPCWSTR shaderName;
        std::optional<D3D12_LOCAL_ROOT_SIGNATURE> optLocalRootSignature;
    };
    std::vector<ShaderExport> m_rayGenShaders;
    std::vector<ShaderExport> m_missShader;

    struct HitGroup {
        D3D12_HIT_GROUP_DESC desc;
        std::optional<D3D12_LOCAL_ROOT_SIGNATURE> optLocalRootSignature;
    };
    std::vector<HitGroup> m_hitGroups;
};

}
