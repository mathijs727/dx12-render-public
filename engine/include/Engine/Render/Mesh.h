#pragma once
#include "Engine/Core/Bounds.h"
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/ShaderInputs/bindpoints/Material.h"
#include "Engine/Render/ShaderInputs/bindpoints/RayTraceMesh.h"
#include "Engine/Render/ShaderInputs/constants.h"
#include "Engine/Render/ShaderInputs/structs/Meshlet.h"
#include "Engine/Render/ShaderInputs/structs/PBRMaterial.h"
#include "Engine/Render/ShaderInputs/structs/Vertex.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/Util/ForwardDeclares.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()
#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace Render {

struct Meshlet : ShaderInputs::Meshlet {
    static constexpr uint32_t MaxNumVertices = MESHLET_MAX_VERTICES;
    static constexpr uint32_t MaxNumPrimitives = MESHLET_MAX_PRIMITIVES;

    Meshlet()
    {
        this->numVertices = 0;
        this->numPrimitives = 0;
    }

    static uint32_t encodePrimitive(uint32_t i0, uint32_t i1, uint32_t i2);
};

struct SubMesh {
    uint32_t indexStart;
    uint32_t numIndices;
    uint32_t baseVertex;
    uint32_t numVertices;
    // Core::Bounds3f bounds;

    uint32_t meshletStart;
    uint32_t numMeshlets;
};
struct Material {
    ShaderInputs::Material shaderInputs;
    bool isOpague;
};
struct Mesh {
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    uint32_t numIndices;
    uint32_t numMeshlets;
    uint32_t numVertices;
    uint32_t vertexStride;
    // Core::Bounds3f bounds;

    std::vector<SubMesh> subMeshes;
    std::vector<Material> materials;

    // Ray tracing information.
    RenderAPI::D3D12MAResource blas;
    std::vector<ShaderInputs::RayTraceMesh> subMeshProperties;

    // Owning pointers to the index- and vertex buffer to keep them alive while the mesh is alive.
    RenderAPI::D3D12MAResource indexBuffer, vertexBuffer, meshletBuffer;
};

struct MaterialCPU : public ShaderInputs::PBRMaterial {
    void writeTo(Util::BinaryWriter& writer) const;
    void readFrom(Util::BinaryReader& reader);
};
struct MeshCPU {
    std::vector<ShaderInputs::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Meshlet> meshlets;
    Core::Bounds3f bounds;

    std::vector<SubMesh> subMeshes;
    std::vector<MaterialCPU> materials;

    void writeTo(Util::BinaryWriter& writer) const;
    void readFrom(Util::BinaryReader& reader);

    // Removes duplicate vertices & improves vertex ordering.
    void removeDuplicateVertices();
    void optimizeIndexVertexOrder();
    void generateMeshlets();
};

}
