#pragma once
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <EASTL/fixed_vector.h>
DISABLE_WARNINGS_POP()
#include <array>
#include <cstddef>
#include <span>
#include <tuple>
#include <vector>

// Inspired by nv_helpers_dx12
// https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial-Part-2

namespace RenderAPI {

struct ShaderBindingTableInfo {
    size_t rayGenOffset { 0 };
    size_t rayGenSize { 0 };

    size_t missOffset { 0 };
    size_t missStride { 0 };
    size_t missSize { 0 };

    size_t hitGroupsOffset { 0 };
    size_t hitGroupsStride { 0 };
    size_t hitGroupsSize { 0 };

public:
    void fillDispatchRays(D3D12_GPU_VIRTUAL_ADDRESS baseAddress, D3D12_DISPATCH_RAYS_DESC& dispatchRays) const;
};

class ShaderBindingTableBuilder {
public:
    ShaderBindingTableBuilder(ID3D12StateObject* pPipelineStateObject);

    void addRayGenerationProgram(LPCWSTR exportName, std::span<const CD3DX12_GPU_DESCRIPTOR_HANDLE> descriptorTables = {});
    void addMissProgram(LPCWSTR exportName, std::span<const CD3DX12_GPU_DESCRIPTOR_HANDLE> descriptorTables = {});
    void addHitGroup(LPCWSTR exportName, std::span<const CD3DX12_GPU_DESCRIPTOR_HANDLE> descriptorTables = {});
    void setHitGroup(uint32_t hitGroupIndex, LPCWSTR exportName, std::span<const CD3DX12_GPU_DESCRIPTOR_HANDLE> descriptorTables = {});

    std::pair<std::vector<std::byte>, ShaderBindingTableInfo> compile();

private:
    struct SBTEntry;
    static size_t copyShaderData(const SBTEntry& entry, std::vector<std::byte>& out);

    static SBTEntry createEntry(const void* pShaderIdentifier, std::span<const CD3DX12_GPU_DESCRIPTOR_HANDLE> descriptorTables);
    static size_t computeMaxEntrySize(std::span<const SBTEntry> entries);
    static size_t computeEntrySize(const SBTEntry& entry);

private:
    WRL::ComPtr<ID3D12StateObjectProperties> m_pStateObjectProperties; // Used to look up shader identifiers.

    struct SBTEntry {
        std::array<std::byte, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES> shaderIdentifier;
        eastl::fixed_vector<uint64_t, 4, false> localRootSignatureBindings;
    };
    SBTEntry m_rayGen {};
    std::vector<SBTEntry> m_misses;
    std::vector<SBTEntry> m_hitGroups;
};

}
