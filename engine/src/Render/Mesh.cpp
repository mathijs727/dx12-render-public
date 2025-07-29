#define _USE_MATH_DEFINES 1 // OpenMesh
#include "Engine/Render/Mesh.h"
#include "Engine/Util/BinaryReader.h"
#include "Engine/Util/BinaryWriter.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include <DirectXMesh.h>
#include <fmt/ranges.h>
DISABLE_WARNINGS_POP()
#include <algorithm>
#include <execution>
#include <unordered_map>

namespace Render {

void MaterialCPU::writeTo(Util::BinaryWriter& writer) const
{
    writer.write(baseColor);
    writer.write(metallic);
    writer.write(alpha);
    writer.write(baseColorTextureIdx);
}

void MaterialCPU::readFrom(Util::BinaryReader& reader)
{
    reader.read(baseColor);
    reader.read(metallic);
    reader.read(alpha);
    reader.read(baseColorTextureIdx);
}

void MeshCPU::writeTo(Util::BinaryWriter& writer) const
{
    writer.write(vertices);
    writer.write(indices);
    writer.write(Meshlet::MaxNumPrimitives);
    writer.write(Meshlet::MaxNumVertices);
    writer.write(meshlets);
    writer.write(bounds);
    writer.write(subMeshes);
    writer.write(materials);
}

void MeshCPU::readFrom(Util::BinaryReader& reader)
{
    reader.read(vertices);
    reader.read(indices);

    // Verify that the file was generated with the same Meshlet size as we are using now.
    // If this fails then you need to generate a new *.bin file (from a GLTF/GLB file).
    uint32_t meshletMaxNumPrimitives, meshletMaxNumVertices;
    reader.read(meshletMaxNumPrimitives);
    reader.read(meshletMaxNumVertices);
    Tbx::assert_always(meshletMaxNumPrimitives == Meshlet::MaxNumPrimitives);
    Tbx::assert_always(meshletMaxNumVertices == Meshlet::MaxNumVertices);

    reader.read(meshlets);
    reader.read(bounds);
    reader.read(subMeshes);
    reader.read(materials);
}

void MeshCPU::removeDuplicateVertices()
{
    constexpr static float epsilon = 10e-6f; // Vertices closer than this distance will be merged into a single vertex.

    std::vector<ShaderInputs::Vertex> newVertices;
    std::vector<uint32_t> newIndices;
    std::transform(std::begin(subMeshes), std::end(subMeshes), std::begin(subMeshes),
        [&](const SubMesh& oldSubMesh) {
            spdlog::debug("Gather vertex positions");
            std::vector<uint32_t> subMeshIndices((size_t)oldSubMesh.numIndices);
            for (uint32_t i = 0; i < oldSubMesh.numIndices; ++i)
                subMeshIndices[i] = indices[oldSubMesh.indexStart + i];
            std::vector<ShaderInputs::Vertex> subMeshVertices((size_t)oldSubMesh.numVertices);
            for (uint32_t i = 0; i < oldSubMesh.numVertices; ++i)
                subMeshVertices[i] = vertices[oldSubMesh.baseVertex + i];

            const uint32_t numFaces = oldSubMesh.numIndices / 3;
            std::vector<DirectX::XMFLOAT3> vertexPositions(subMeshVertices.size());
            std::transform(
                std::execution::par_unseq,
                std::begin(subMeshVertices), std::end(subMeshVertices),
                std::begin(vertexPositions),
                [](const ShaderInputs::Vertex& vertex) -> DirectX::XMFLOAT3 {
                    return DirectX::XMFLOAT3(vertex.pos.x, vertex.pos.y, vertex.pos.z);
                });

            // https://github.com/microsoft/DirectXMesh/wiki/DirectXMesh
            // 0. GenerateAdjacencyAndPointReps
            // 1. WeldVertices
            // 2. OptimizeVertices
            // 3. FinalizeIB
            // 4. CompactVB
            spdlog::debug("0. GenerateAdjacencyAndPointReps");
            std::vector<uint32_t> adjacency((size_t)oldSubMesh.numIndices);
            std::vector<uint32_t> pointReps((size_t)oldSubMesh.numVertices);
            RenderAPI::ThrowIfFailed(
                DirectX::GenerateAdjacencyAndPointReps(subMeshIndices.data(), numFaces, vertexPositions.data(), vertexPositions.size(), epsilon, pointReps.data(), adjacency.data()));

            spdlog::debug("1. WeldVertices");
            RenderAPI::ThrowIfFailed(
                DirectX::WeldVertices(subMeshIndices.data(), numFaces, subMeshVertices.size(),
                    pointReps.data(), nullptr, [](uint32_t, uint32_t) { return true; }));

            spdlog::debug("2. OptimizeVertices");
            std::vector<uint32_t> vertexRemap(subMeshVertices.size());
            size_t trailingUnused;
            RenderAPI::ThrowIfFailed(
                DirectX::OptimizeVertices(subMeshIndices.data(), numFaces, subMeshVertices.size(), vertexRemap.data(), &trailingUnused));

            spdlog::debug("3. FinalizeIB");
            RenderAPI::ThrowIfFailed(
                DirectX::FinalizeIB(subMeshIndices.data(), numFaces, vertexRemap.data(), subMeshVertices.size()));

            spdlog::debug("4. CompactVB");
            std::vector<ShaderInputs::Vertex> newSubMeshVertices(subMeshVertices.size() - trailingUnused);
            RenderAPI::ThrowIfFailed(
                DirectX::CompactVB(subMeshVertices.data(), sizeof(ShaderInputs::Vertex), subMeshVertices.size(), trailingUnused, vertexRemap.data(), newSubMeshVertices.data()));


            const SubMesh newSubMesh {
                .indexStart = (uint32_t)newIndices.size(),
                .numIndices = (uint32_t)subMeshIndices.size(),
                .baseVertex = (uint32_t)newVertices.size(),
                .numVertices = (uint32_t)newSubMeshVertices.size()
            };
            newIndices.resize(newIndices.size() + subMeshIndices.size());
            std::copy(std::begin(subMeshIndices), std::end(subMeshIndices), std::begin(newIndices) + newSubMesh.indexStart);
            newVertices.resize(newVertices.size() + newSubMeshVertices.size());
            std::copy(std::begin(newSubMeshVertices), std::end(newSubMeshVertices), std::begin(newVertices) + newSubMesh.baseVertex);

            return newSubMesh;
        });

    spdlog::info("num indices {} -> {}", indices.size(), newIndices.size());
    spdlog::info("num vertices {} -> {}", vertices.size(), newVertices.size());
    this->indices = std::move(newIndices);
    this->vertices = std::move(newVertices);
}

void MeshCPU::optimizeIndexVertexOrder()
{
    constexpr static float epsilon = 10e-3f; // Vertices closer than this distance will be merged into a single vertex.

    std::vector<ShaderInputs::Vertex> newVertices;
    std::vector<uint32_t> newIndices;
    std::transform(std::begin(subMeshes), std::end(subMeshes), std::begin(subMeshes),
        [&](const SubMesh& oldSubMesh) {
            spdlog::debug("Gather vertex positions");
            std::vector<uint32_t> subMeshIndices((size_t)oldSubMesh.numIndices);
            for (uint32_t i = 0; i < oldSubMesh.numIndices; ++i)
                subMeshIndices[i] = indices[oldSubMesh.indexStart + i];
            std::vector<ShaderInputs::Vertex> subMeshVertices((size_t)oldSubMesh.numVertices);
            for (uint32_t i = 0; i < oldSubMesh.numVertices; ++i)
                subMeshVertices[i] = vertices[oldSubMesh.baseVertex + i];

            const uint32_t numFaces = oldSubMesh.numIndices / 3;
            std::vector<DirectX::XMFLOAT3> vertexPositions(subMeshVertices.size());
            std::transform(
                std::execution::par_unseq,
                std::begin(subMeshVertices), std::end(subMeshVertices),
                std::begin(vertexPositions),
                [](const ShaderInputs::Vertex& vertex) -> DirectX::XMFLOAT3 {
                    return DirectX::XMFLOAT3(vertex.pos.x, vertex.pos.y, vertex.pos.z);
                });

            // https://github.com/microsoft/DirectXMesh/wiki/DirectXMesh
            // 0. GenerateAdjacencyAndPointReps
            // 1. Clean
            // 2. AttributeSort
            // 3. ReorderIBAndAdjacency
            // 4. OptimizeFaces
            // 5. ReorderIBAndAdjacency
            // 6. OptimizeVertices
            // 7. FinalizeIB
            // 8. FinalizeVB
            spdlog::debug("0. GenerateAdjacencyAndPointReps");
            std::vector<uint32_t> adjacency((size_t)oldSubMesh.numIndices);
            std::vector<uint32_t> pointReps((size_t)oldSubMesh.numVertices);
            RenderAPI::ThrowIfFailed(
                DirectX::GenerateAdjacencyAndPointReps(subMeshIndices.data(), numFaces, vertexPositions.data(), vertexPositions.size(), epsilon, pointReps.data(), adjacency.data()));

            std::vector<uint32_t> duplicateVertices;
            spdlog::debug("1. Clean");
            RenderAPI::ThrowIfFailed(
                DirectX::Clean(subMeshIndices.data(), (size_t)numFaces, vertexPositions.size(), adjacency.data(), nullptr, duplicateVertices));

            // DirectX::AttributeSort();
            // DirectX::ReorderIBAndAdjacency(subMeshIndices.data(), numFaces, adjacency.data(),

            spdlog::debug("4. OptimizeFaces");
            std::vector<uint32_t> faceRemap((size_t)numFaces * 3);
            std::iota(std::begin(faceRemap), std::end(faceRemap), 0u);
            RenderAPI::ThrowIfFailed(
                DirectX::OptimizeFaces(subMeshIndices.data(), numFaces, adjacency.data(), faceRemap.data()));

            spdlog::debug("5. ReorderIBAndAdjacency");
            std::vector<uint32_t> newSubMeshIndices(subMeshIndices.size());
            std::vector<uint32_t> newAdjacency(newSubMeshIndices.size());
            RenderAPI::ThrowIfFailed(
                DirectX::ReorderIBAndAdjacency(
                    subMeshIndices.data(), numFaces, faceRemap.data(), faceRemap.data(), newSubMeshIndices.data(), newAdjacency.data()));

            spdlog::debug("6. OptimizeVertices");
            std::vector<uint32_t> vertexRemap(subMeshVertices.size());
            RenderAPI::ThrowIfFailed(
                DirectX::OptimizeVertices(newSubMeshIndices.data(), numFaces, subMeshVertices.size(), vertexRemap.data()));

            spdlog::debug("7. FinalizeIB");
            RenderAPI::ThrowIfFailed(
                DirectX::FinalizeIB(newSubMeshIndices.data(), numFaces, vertexRemap.data(), subMeshVertices.size()));

            spdlog::debug("8. FinalizeVB");
            std::vector<ShaderInputs::Vertex> newSubMeshVertices(subMeshVertices.size() + duplicateVertices.size());
            RenderAPI::ThrowIfFailed(
                DirectX::FinalizeVB(
                    subMeshVertices.data(), sizeof(ShaderInputs::Vertex), subMeshVertices.size(),
                    duplicateVertices.data(), duplicateVertices.size(),
                    vertexRemap.data(), newSubMeshVertices.data()));

            const SubMesh newSubMesh {
                .indexStart = (uint32_t)newIndices.size(),
                .numIndices = (uint32_t)newSubMeshIndices.size(),
                .baseVertex = (uint32_t)newVertices.size(),
                .numVertices = (uint32_t)newSubMeshVertices.size()
            };
            newIndices.resize(newIndices.size() + newSubMeshIndices.size());
            std::copy(std::begin(newSubMeshIndices), std::end(newSubMeshIndices), std::begin(newIndices) + newSubMesh.indexStart);
            newVertices.resize(newVertices.size() + newSubMeshVertices.size());
            std::copy(std::begin(newSubMeshVertices), std::end(newSubMeshVertices), std::begin(newVertices) + newSubMesh.baseVertex);

            return newSubMesh;
        });

    spdlog::info("num indices {} -> {}", indices.size(), newIndices.size());
    spdlog::info("num vertices {} -> {}", vertices.size(), newVertices.size());
    this->indices = std::move(newIndices);
    this->vertices = std::move(newVertices);
}

void MeshCPU::generateMeshlets()
{
    constexpr static float epsilon = 10e-6f; // Vertices closer than this distance will be merged into a single vertex.

    const auto numPrimitives = indices.size();
    this->meshlets.clear();

    for (uint32_t subMeshIdx = 0; subMeshIdx < subMeshes.size(); ++subMeshIdx) {
        auto& subMesh = subMeshes[subMeshIdx];
        subMesh.meshletStart = (uint32_t)this->meshlets.size();

#if 1
        spdlog::debug("Gather vertex positions");
        assert(subMesh.numIndices % 3 == 0);
        const uint32_t numFaces = subMesh.numIndices / 3;
        std::vector<DirectX::XMFLOAT3> vertexPositions((size_t)subMesh.numVertices);
        std::transform(
            std::execution::par_unseq,
            std::begin(vertices) + subMesh.baseVertex, std::begin(vertices) + subMesh.baseVertex + subMesh.numVertices,
            std::begin(vertexPositions),
            [](const ShaderInputs::Vertex& vertex) -> DirectX::XMFLOAT3 {
                return DirectX::XMFLOAT3(vertex.pos.x, vertex.pos.y, vertex.pos.z);
            });

        uint32_t const* pIndices = &indices[subMesh.indexStart];
        spdlog::debug("GenerateAdjacencyAndPointReps");
        std::vector<uint32_t> adjacency((size_t)subMesh.numIndices);
        RenderAPI::ThrowIfFailed(
            DirectX::GenerateAdjacencyAndPointReps(pIndices, numFaces, vertexPositions.data(), vertexPositions.size(), epsilon, nullptr, adjacency.data()));

        spdlog::debug("ComputeMeshlets");
        std::vector<DirectX::Meshlet> dxMeshlets;
        std::vector<uint8_t> uniqueVertexIB;
        std::vector<DirectX::MeshletTriangle> primitiveIndices;
        RenderAPI::ThrowIfFailed(
            DirectX::ComputeMeshlets(
                &indices[subMesh.indexStart], numFaces,
                vertexPositions.data(), vertexPositions.size(),
                adjacency.data(), dxMeshlets, uniqueVertexIB, primitiveIndices, Meshlet::MaxNumVertices, Meshlet::MaxNumPrimitives));
        Tbx::assert_always(uniqueVertexIB.size() % sizeof(uint32_t) == 0);
        uint32_t const* pUniqueVertexIB = reinterpret_cast<uint32_t const*>(uniqueVertexIB.data());

        spdlog::debug("Transform meshlets to my format");
        for (const DirectX::Meshlet& dxMeshlet : dxMeshlets) {
            Meshlet meshlet {};
            meshlet.numVertices = dxMeshlet.VertCount;
            meshlet.numPrimitives = dxMeshlet.PrimCount;
            meshlet.subMeshIdx = subMeshIdx;
            for (uint32_t i = 0; i < dxMeshlet.VertCount; ++i) {
                meshlet.vertices[i] = pUniqueVertexIB[dxMeshlet.VertOffset + i] + subMesh.baseVertex;
            }
            for (uint32_t i = 0; i < dxMeshlet.PrimCount; ++i) {
                const auto primitive = primitiveIndices[dxMeshlet.PrimOffset + i];
                meshlet.primitives[i] = Meshlet::encodePrimitive(primitive.i0, primitive.i1, primitive.i2);
            }
            this->meshlets.push_back(meshlet);
        }

#else
        Meshlet meshlet {};
        meshlet.numPrimitives = meshlet.numVertices = 0;
        meshlet.subMeshIdx = subMeshIdx;
        std::unordered_map<uint32_t, uint8_t> globalToLocalMapping;
        const auto tryAddTriangleToMeshlet = [&](uint32_t firstIndexIndex) {
            // If the primitive (index) buffer of the meshlet is full.
            if (meshlet.numPrimitives == Meshlet::MaxNumPrimitives)
                return false;

            uint32_t primitive = 0;
            for (uint32_t i = 0; i < 3; ++i) { // i = [0, 1, 2] -> vertex ID of the triangle.
                const uint32_t globalIndex = subMesh.baseVertex + indices[firstIndexIndex + i]; // Index in the global index buffer

                uint8_t localIndex; // Index into the meshlets vertex buffer (meshlet.vertices) which is an index into the global index buffer.
                if (auto iter = globalToLocalMapping.find(globalIndex); iter != std::end(globalToLocalMapping)) {
                    localIndex = iter->second;
                } else {
                    // If the vertex buffer of the meshlet is full.
                    if (meshlet.numVertices == Meshlet::MaxNumVertices)
                        return false;

                    // Allocate a new vertex in the meshlet.
                    localIndex = (uint8_t)meshlet.numVertices++;
                    meshlet.vertices[localIndex] = globalIndex;
                    globalToLocalMapping[globalIndex] = localIndex; // Store the mapping so it can be reused by other triangles in the meshlet.
                }
                primitive |= uint32_t(localIndex) << (i * 8);
            }
            meshlet.primitives[meshlet.numPrimitives++] = primitive;
            return true;
        };

        // Add all meshlets.
        Tbx::assert_always(subMesh.numIndices % 3 == 0);
        for (uint32_t triangleStart = subMesh.indexStart; triangleStart < subMesh.indexStart + subMesh.numIndices;) {
            if (tryAddTriangleToMeshlet(triangleStart)) {
                triangleStart += 3; // Successfully added triangle to current meshlet.
            } else {
                // Meshlet was full; add to the list of meshlets and start with a new empty meshlet.
                this->meshlets.push_back(meshlet);
                meshlet.numPrimitives = meshlet.numVertices = 0;
                globalToLocalMapping.clear();
            }
        }
        if (meshlet.numPrimitives > 0)
            this->meshlets.push_back(meshlet);

#endif
        subMesh.numMeshlets = uint32_t(this->meshlets.size() - subMesh.meshletStart);

        /* size_t j = subMesh.indexStart;
        for (size_t i = subMesh.meshletStart; i < subMesh.meshletStart + subMesh.numMeshlets; ++i) {
            const auto& meshlet2 = this->meshlets[i];
            for (size_t prim = 0; prim < meshlet2.numPrimitives; ++prim) {
                const uint32_t encodedPrimitive = meshlet2.primitives[prim];
                const uint32_t v0 = meshlet2.vertices[(encodedPrimitive >> 0) & 0xFF];
                const uint32_t v1 = meshlet2.vertices[(encodedPrimitive >> 8) & 0xFF];
                const uint32_t v2 = meshlet2.vertices[(encodedPrimitive >> 16) & 0xFF];

                const uint32_t v0_ref = indices[j++] + subMesh.baseVertex;
                const uint32_t v1_ref = indices[j++] + subMesh.baseVertex;
                const uint32_t v2_ref = indices[j++] + subMesh.baseVertex;

                assert(v0 == v0_ref);
                assert(v1 == v1_ref);
                assert(v2 == v2_ref);
            }
        } */
    }
}

uint32_t Meshlet::encodePrimitive(uint32_t i0, uint32_t i1, uint32_t i2)
{
    return i0 | (i1 << 8) | (i2 << 16);
}
}
