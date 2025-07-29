#include "Engine/RenderAPI/ShaderBindingTableBuilder.h"
#include "Engine/Util/Align.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <cppitertools/enumerate.hpp>
DISABLE_WARNINGS_POP()
#include <algorithm> // std::max
#include <algorithm> // std::copy
#include <cassert>
#include <cstring> // std::memcpy

// Inspired by nv_helpers_dx12
// https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial-Part-2

namespace RenderAPI {

ShaderBindingTableBuilder::ShaderBindingTableBuilder(ID3D12StateObject* pPipelineStateObject)
{
    // See:
    // https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingHelloWorld/D3D12RaytracingHelloWorld.cpp
    RenderAPI::ThrowIfFailed(
        pPipelineStateObject->QueryInterface(IID_PPV_ARGS(&m_pStateObjectProperties)));
}

void ShaderBindingTableBuilder::addRayGenerationProgram(
    LPCWSTR exportName, std::span<const CD3DX12_GPU_DESCRIPTOR_HANDLE> descriptors)
{
    void* pShaderIdentifier = m_pStateObjectProperties->GetShaderIdentifier(exportName);
    m_rayGen = createEntry(pShaderIdentifier, descriptors);
}

void ShaderBindingTableBuilder::addMissProgram(
    LPCWSTR exportName, std::span<const CD3DX12_GPU_DESCRIPTOR_HANDLE> descriptors)
{
    void* pShaderIdentifier = m_pStateObjectProperties->GetShaderIdentifier(exportName);
    m_misses.push_back(createEntry(pShaderIdentifier, descriptors));
}

void ShaderBindingTableBuilder::addHitGroup(
    LPCWSTR exportName, std::span<const CD3DX12_GPU_DESCRIPTOR_HANDLE> descriptors)
{
    void* pShaderIdentifier = m_pStateObjectProperties->GetShaderIdentifier(exportName);
    m_hitGroups.push_back(createEntry(pShaderIdentifier, descriptors));
}

void ShaderBindingTableBuilder::setHitGroup(uint32_t hitGroupIndex, LPCWSTR exportName, std::span<const CD3DX12_GPU_DESCRIPTOR_HANDLE> descriptors)
{
    void* pShaderIdentifier = m_pStateObjectProperties->GetShaderIdentifier(exportName);
    if (m_hitGroups.size() <= hitGroupIndex)
        m_hitGroups.resize(hitGroupIndex + 1);
    m_hitGroups[hitGroupIndex] = createEntry(pShaderIdentifier, descriptors);
}

std::pair<std::vector<std::byte>, ShaderBindingTableInfo> ShaderBindingTableBuilder::compile()
{
    ShaderBindingTableInfo info {};
    std::vector<std::byte> shaderBindingTable;

    // Ray gen shader.
    info.rayGenOffset = shaderBindingTable.size();
    info.rayGenSize = copyShaderData(m_rayGen, shaderBindingTable);

    // Miss shader.
    shaderBindingTable.resize(Util::roundUpToClosestMultiple(shaderBindingTable.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
    info.missOffset = shaderBindingTable.size();
    info.missStride = Util::roundUpToClosestMultiple(computeMaxEntrySize(m_misses), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    for (const auto& miss : m_misses) {
        copyShaderData(miss, shaderBindingTable);
        shaderBindingTable.resize(Util::roundUpToClosestMultiple(shaderBindingTable.size(), info.missStride));
    }
    info.missSize = shaderBindingTable.size() - info.missOffset;

    // Hit groups.
    shaderBindingTable.resize(Util::roundUpToClosestMultiple(shaderBindingTable.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
    info.hitGroupsOffset = shaderBindingTable.size();
    info.hitGroupsStride = Util::roundUpToClosestMultiple(computeMaxEntrySize(m_hitGroups), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    for (const auto& hitGroup : m_hitGroups) {
        copyShaderData(hitGroup, shaderBindingTable);
        shaderBindingTable.resize(Util::roundUpToClosestMultiple(shaderBindingTable.size(), info.hitGroupsStride));
    }
    info.hitGroupsSize = shaderBindingTable.size() - info.hitGroupsOffset;
    return { shaderBindingTable, info };
}

size_t ShaderBindingTableBuilder::copyShaderData(const SBTEntry& entry, std::vector<std::byte>& out)
{
    const size_t sbtSizeBefore = out.size();
    std::copy(std::begin(entry.shaderIdentifier), std::end(entry.shaderIdentifier), std::back_inserter(out));
    for (uint64_t binding : entry.localRootSignatureBindings) {
        std::array<std::byte, 8> bindingInBytes;
        std::memcpy(bindingInBytes.data(), &binding, sizeof(binding));
        std::copy(std::begin(bindingInBytes), std::end(bindingInBytes), std::back_inserter(out));
    }
    return out.size() - sbtSizeBefore;
}

ShaderBindingTableBuilder::SBTEntry ShaderBindingTableBuilder::createEntry(const void* pShaderIdentifier, std::span<const CD3DX12_GPU_DESCRIPTOR_HANDLE> descriptors)
{
    SBTEntry entry {};
    std::memcpy(entry.shaderIdentifier.data(), pShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    for (const CD3DX12_GPU_DESCRIPTOR_HANDLE descriptor : descriptors) {
        entry.localRootSignatureBindings.push_back(descriptor.ptr);
    }
    return entry;
}

size_t ShaderBindingTableBuilder::computeMaxEntrySize(std::span<const SBTEntry> entries)
{
    size_t maxEntrySizeInBytes = 0;
    for (const SBTEntry& entry : entries) {
        const size_t entrySizeInBytes = computeEntrySize(entry);
        maxEntrySizeInBytes = std::max(maxEntrySizeInBytes, entrySizeInBytes);
    }
    return maxEntrySizeInBytes;
}

size_t ShaderBindingTableBuilder::computeEntrySize(const SBTEntry& entry)
{
    return D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + entry.localRootSignatureBindings.size() * sizeof(uint64_t);
}

void ShaderBindingTableInfo::fillDispatchRays(D3D12_GPU_VIRTUAL_ADDRESS baseAddress, D3D12_DISPATCH_RAYS_DESC& dispatchRays) const
{
    dispatchRays.RayGenerationShaderRecord = D3D12_GPU_VIRTUAL_ADDRESS_RANGE {
        .StartAddress = baseAddress + rayGenOffset,
        .SizeInBytes = rayGenSize
    };
    dispatchRays.MissShaderTable = D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE {
        .StartAddress = baseAddress + missOffset,
        .SizeInBytes = missSize,
        .StrideInBytes = missStride
    };
    dispatchRays.HitGroupTable = D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE {
        .StartAddress = baseAddress + hitGroupsOffset,
        .SizeInBytes = hitGroupsSize,
        .StrideInBytes = hitGroupsStride
    };
}

}
