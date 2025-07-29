#include "Engine/Render/Scene.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/GPUProfiler.h"
#include "Engine/Render/Mesh.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/ShaderInputs/inputgroups/BindlessScene.h"
#include "Engine/Render/ShaderInputs/inputgroups/RTMesh.h"
#include "Engine/Render/ShaderInputs/inputgroups/SinglePBRMaterial.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/Render/ShaderInputs/structs/BindlessMesh.h"
#include "Engine/Render/ShaderInputs/structs/BindlessMeshInstance.h"
#include "Engine/Render/ShaderInputs/structs/BindlessSubMesh.h"
#include "Engine/Render/Texture.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/Internal/D3D12MAHelpers.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include "Engine/Util/BinaryReader.h"
#include "Engine/Util/BinaryWriter.h"
#include "Engine/Util/IsOfType.h"
#include "Engine/Util/Math.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <cppitertools/enumerate.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>
#include <mio/mmap.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <execution>
#include <fstream>
#include <functional>
#include <random>
#include <span>
#include <string>
#include <tbx/error_handling.h>
#include <tbx/format/fmt_glm.h>
#include <tbx/template_meta.h>
#include <tbx/vector_size.h>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

static constexpr uint64_t binaryFileVersionNumber = 6;

namespace Render {

template <size_t W, size_t H>
static std::array<glm::vec2, W * H> generateTAAJitterArray()
{
    std::uniform_real_distribution<float> dist { 0, 1 };
    std::random_device rd {};

    const glm::vec2 stratumSize { 1.0f / W, 1.0f / H };
    std::array<glm::vec2, W * H> out;
    for (size_t y = 0; y < H; ++y) {
        for (size_t x = 0; x < W; ++x) {
            out[y * W + x] = (glm::vec2(x, y) + glm::vec2(dist(rd), dist(rd))) * stratumSize;
        }
    }
    std::shuffle(std::begin(out), std::end(out), rd);
    return out;
}

static auto taaJitterArray = generateTAAJitterArray<3, 3>();

void Scene::transitionVertexBuffers(ID3D12GraphicsCommandList6* pCommandList, D3D12_RESOURCE_STATES desiredState)
{
    if (vertexBufferState == desiredState)
        return;

    std::vector<D3D12_RESOURCE_BARRIER> barriers(meshes.size());
    std::transform(std::begin(meshes), std::end(meshes), std::begin(barriers),
        [&](const Mesh& mesh) {
            return CD3DX12_RESOURCE_BARRIER::Transition(mesh.vertexBuffer.Get(), vertexBufferState, desiredState);
        });
    pCommandList->ResourceBarrier((UINT)barriers.size(), barriers.data());
    vertexBufferState = desiredState;
}

void Scene::updateHistoricalTransformMatrices()
{
    ++frameIdx;

    camera.previousTransform = camera.transform;
    for (auto& meshInstance : meshInstances)
        meshInstance.previousTransform = meshInstance.transform;

    cameraJitterTAA = taaJitterArray[frameIdx % taaJitterArray.size()];
}

void Scene::buildRayTracingAccelerationStructure(Render::RenderContext& renderContext)
{
    auto pCommandList = renderContext.commandListManager.acquireCommandList();
    GPUProfiler gpuProfiler { &renderContext };
    const uint32_t buildBotLevelTask = gpuProfiler.startTask(pCommandList.Get(), "build bot level");

    // Build the Bottom Level Acceleration Structures for the meshes in the scene.
    uint32_t instanceContributionToHitGroupIndex = 0;
    std::vector<RenderAPI::D3D12MAResource> scratchBuffers;
    for (auto& mesh : meshes) {
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometries;
        for (size_t subMeshIdx = 0; subMeshIdx < mesh.subMeshes.size(); ++subMeshIdx) {
            const auto& subMesh = mesh.subMeshes[subMeshIdx];
            const auto& material = mesh.materials[subMeshIdx];

            D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc {};
            geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geometryDesc.Flags = material.isOpague ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
            geometryDesc.Triangles = D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC {
                .Transform3x4 = 0,
                .IndexFormat = DXGI_FORMAT_R32_UINT,
                .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                .IndexCount = static_cast<UINT>(subMesh.numIndices),
                .VertexCount = static_cast<UINT>(subMesh.numVertices),
                .IndexBuffer = mesh.indexBufferView.BufferLocation + subMesh.indexStart * sizeof(uint32_t)
            };
            geometryDesc.Triangles.VertexBuffer.StartAddress = mesh.vertexBufferView.BufferLocation + subMesh.baseVertex * mesh.vertexStride;
            geometryDesc.Triangles.VertexBuffer.StrideInBytes = mesh.vertexStride;
            geometries.push_back(geometryDesc);
        }

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
            .NumDescs = static_cast<UINT>(geometries.size()),
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .pGeometryDescs = geometries.data()
        };

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;
        renderContext.pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);
        assert(prebuildInfo.ResultDataMaxSizeInBytes > 0llu);

        auto scratchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto mainBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto scratchBuffer = renderContext.createResource(D3D12_HEAP_TYPE_DEFAULT, scratchBufferDesc, D3D12_RESOURCE_STATE_COMMON);
        mesh.blas = renderContext.createResource(D3D12_HEAP_TYPE_DEFAULT, mainBufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

        // Compute the required sizes of the buffers that store the acceleration structure and scratch buffer used during
        // generation of the acceleration structure.
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc {
            .DestAccelerationStructureData = mesh.blas->GetGPUVirtualAddress(),
            .Inputs = buildInputs,
            .ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress()
        };
        scratchBuffers.emplace_back(std::move(scratchBuffer)); // Keep it alive until after BVH construction is done.
        pCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
        const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(mesh.blas.Get());
        pCommandList->ResourceBarrier(1, &barrier);

        for (const auto& subMesh : mesh.subMeshes) {
            // Create a binding to the mesh index & vertex buffers so we can decode materials from the hitgroup shader.
            RenderAPI::SRVDesc indexBufferDesc;
            indexBufferDesc.desc.Format = DXGI_FORMAT_UNKNOWN;
            indexBufferDesc.desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            indexBufferDesc.desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            indexBufferDesc.desc.Buffer.FirstElement = subMesh.indexStart;
            indexBufferDesc.desc.Buffer.NumElements = subMesh.numIndices;
            indexBufferDesc.desc.Buffer.StructureByteStride = sizeof(uint32_t);
            indexBufferDesc.desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            indexBufferDesc.pResource = mesh.indexBuffer;

            RenderAPI::SRVDesc vertexBufferDesc;
            vertexBufferDesc.desc.Format = DXGI_FORMAT_UNKNOWN;
            vertexBufferDesc.desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            vertexBufferDesc.desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            vertexBufferDesc.desc.Buffer.FirstElement = subMesh.baseVertex;
            vertexBufferDesc.desc.Buffer.NumElements = subMesh.numVertices;
            vertexBufferDesc.desc.Buffer.StructureByteStride = sizeof(ShaderInputs::Vertex);
            vertexBufferDesc.desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            vertexBufferDesc.pResource = mesh.vertexBuffer;

            ShaderInputs::RTMesh rayTraceMeshInputs;
            rayTraceMeshInputs.setIndices(indexBufferDesc);
            rayTraceMeshInputs.setVertices(vertexBufferDesc);
            mesh.subMeshProperties.push_back(rayTraceMeshInputs.generatePersistentBindings(renderContext));
        }
    }
    gpuProfiler.endTask(pCommandList.Get(), buildBotLevelTask);

    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances;
    for (uint32_t instanceID = 0; instanceID < meshInstances.size(); ++instanceID) {
        auto& instance = meshInstances[instanceID];
        const auto& mesh = meshes[instance.meshIdx];

        // Add the new mesh to ShaderBindingTable(s).
        instance.instanceContributionToHitGroupIndex = instanceContributionToHitGroupIndex;
        instanceContributionToHitGroupIndex += (uint32_t)mesh.subMeshes.size();

        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc {
            .InstanceID = instanceID,
            .InstanceMask = 0xFF,
            .InstanceContributionToHitGroupIndex = instance.instanceContributionToHitGroupIndex,
            .Flags = 0,
            .AccelerationStructure = mesh.blas->GetGPUVirtualAddress()
        };

        const glm::mat3x4 transform = glm::transpose(instance.transform.matrix());
        std::memcpy(&instanceDesc.Transform, glm::value_ptr(transform), sizeof(transform));
        instances.push_back(instanceDesc);
    }

    // ==== Build the Top Level Acceleration Structure ====
    // Copy the instance descs to the GPU.
    const size_t instancesSizeInBytes = Tbx::vectorSizeInBytes(instances);
    const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(instancesSizeInBytes, D3D12_RESOURCE_FLAG_NONE);
    const auto instanceBuffer = renderContext.createResource(D3D12_HEAP_TYPE_UPLOAD, bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ);
    // Copy from std::vector to GPU visible buffer (pinned memory).
    const D3D12_RANGE range { 0, instancesSizeInBytes };
    D3D12_RAYTRACING_INSTANCE_DESC* pInstanceBuffer;
    instanceBuffer->Map(0, &range, (void**)&pInstanceBuffer);
    std::memcpy(pInstanceBuffer, instances.data(), instancesSizeInBytes);
    instanceBuffer->Unmap(0, &range);

    // Compute the required sizes of the buffers that store the acceleration structure and scratch buffer used during
    // generation of the acceleration structure.
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags {};
    buildFlags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = buildFlags,
        .NumDescs = static_cast<UINT>(instances.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .InstanceDescs = instanceBuffer->GetGPUVirtualAddress()
    };
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;
    renderContext.pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);
    assert(prebuildInfo.ResultDataMaxSizeInBytes > 0llu);

    // const size_t requiredTlasScratchBufferSize = m_tlasBuffer ? prebuildInfo.UpdateScratchDataSizeInBytes : prebuildInfo.ScratchDataSizeInBytes;
    const size_t requiredTlasScratchBufferSize = prebuildInfo.ScratchDataSizeInBytes;
    const auto tlasScratchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredTlasScratchBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    const auto tlasScratchBuffer = renderContext.createResource(D3D12_HEAP_TYPE_DEFAULT, tlasScratchBufferDesc, D3D12_RESOURCE_STATE_COMMON);

    // Buffers cannot be created in the D3D12_RESOURCE_STATE_UNORDERED_ACCESS state, so we need to transition it.
    const auto tlasScratchBufferTransitionUAV = CD3DX12_RESOURCE_BARRIER::Transition(tlasScratchBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pCommandList->ResourceBarrier(1, &tlasScratchBufferTransitionUAV);

    const auto tlasBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    tlas = renderContext.createResource(D3D12_HEAP_TYPE_DEFAULT, tlasBufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc {
        .DestAccelerationStructureData = tlas->GetGPUVirtualAddress(),
        .Inputs = buildInputs,
        .SourceAccelerationStructureData = 0,
        .ScratchAccelerationStructureData = tlasScratchBuffer->GetGPUVirtualAddress()
    };
    const uint32_t buildTopLevelTask = gpuProfiler.startTask(pCommandList.Get(), "build top level");
    pCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    gpuProfiler.endTask(pCommandList.Get(), buildTopLevelTask);

    // Execute RayTracingAccelerationStructure build.
    renderContext.cbvSrvUavDescriptorStaticAllocator.flush();
    renderContext.submitGraphicsQueue(pCommandList);
    renderContext.waitForIdle(); // Ensure that BVH has been build before releasing the scratch buffers.
}

RenderAPI::SRVDesc Render::Scene::tlasBinding() const
{
    return {
        .desc = D3D12_SHADER_RESOURCE_VIEW_DESC {
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .RaytracingAccelerationStructure = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV {
                .Location = tlas->GetGPUVirtualAddress() } },
        .pResource = nullptr
    };
}

template <typename T>
static T readJson(const nlohmann::json& json);
template <>
float readJson<float>(const nlohmann::json& json)
{
    return (float)json;
}
template <>
int readJson<int>(const nlohmann::json& json)
{
    return (int)json;
}
template <>
glm::vec2 readJson<glm::vec2>(const nlohmann::json& json)
{
    return glm::vec2(json[0], json[1]);
}
template <>
glm::vec3 readJson<glm::vec3>(const nlohmann::json& json)
{
    return glm::vec3(json[0], json[1], json[2]);
}
template <>
glm::quat readJson<glm::quat>(const nlohmann::json& json)
{
    return glm::quat(json[3], json[0], json[1], json[2]);
}

template <typename T>
static T readJson(const nlohmann::json& json, const char* name, T defaultValue)
{
    if (json.contains(name))
        return readJson<T>(json[name]);
    else
        return defaultValue;
}

static constexpr uint32_t GLTF_Byte = 5120;
static constexpr uint32_t GLTF_UnsignedByte = 5121;
static constexpr uint32_t GLTF_SignedShort = 5122;
static constexpr uint32_t GLTF_UnsignedShort = 5123;
static constexpr uint32_t GLTF_UnsignedInt = 5125;
static constexpr uint32_t GLTF_Float = 5126;

struct GLTFBuffers {
    std::vector<mio::mmap_source> mappedFiles;
    std::vector<std::span<const std::byte>> buffers;
};

template <typename T>
class Iterator {
public:
    Iterator(const T& defaultValue)
        : m_defaultValue(defaultValue)
        , m_count(std::numeric_limits<size_t>::max())
        , m_stride(0)
    {
    }
    Iterator(std::span<const std::byte> data, size_t count, size_t stride, size_t sourceItemSize = sizeof(T))
        : m_data(data)
        , m_count(count)
        , m_stride(stride == 0 ? sourceItemSize : stride)
        , m_sourceItemSize(sourceItemSize)
    {
    }

    T nextValue()
    {
        if (m_stride == 0) {
            return m_defaultValue;
        } else {
            T out {};
            std::memcpy(&out, &m_data[m_current * m_stride], m_sourceItemSize);
            ++m_current;
            return out;
        }
    }
    bool isValid() const { return m_current < m_count; }
    size_t count() const { return m_count; }

private:
    T m_defaultValue;
    std::span<const std::byte> m_data;
    size_t m_current = 0, m_count, m_stride, m_sourceItemSize;
};

struct BufferView {
    std::span<const std::byte> buffer;
    size_t stride = 0;
};
BufferView readBufferView(const nlohmann::json& jsonBufferView, const GLTFBuffers& buffers)
{
    // Parse the buffer view being pointed to by the accessor.
    const int bufferIdx = jsonBufferView["buffer"];
    const size_t bufferViewByteOffset = jsonBufferView.value<size_t>("byteOffset", 0);
    const size_t bufferViewByteLength = jsonBufferView["byteLength"];
    const size_t bufferViewByteStride = jsonBufferView.value<size_t>("byteStride", 0);

    return BufferView {
        .buffer = buffers.buffers[bufferIdx].subspan(bufferViewByteOffset, bufferViewByteLength),
        .stride = bufferViewByteStride
    };
}

template <typename T>
static Iterator<T> readAccessor(const nlohmann::json& jsonData, const GLTFBuffers& buffers, int accessorIdx, const std::filesystem::path& basePath)
{
    const auto& jsonAccessor = jsonData["accessors"][accessorIdx];

    // Parse the buffer view being pointed to by the accessor.
    const int bufferViewIdx = jsonAccessor["bufferView"];
    const auto jsonBufferView = jsonData["bufferViews"][bufferViewIdx];
    const auto bufferView = readBufferView(jsonBufferView, buffers);

    // Parse the accessor.
    const size_t accessorByteOffset = jsonAccessor.value<size_t>("byteOffset", 0);
    const int componentType = jsonAccessor["componentType"];
    const bool normalized = jsonAccessor.value<bool>("normalized", false);
    Tbx::assert_always(!normalized); // Normalization not supported yet.
    const size_t count = jsonAccessor["count"];
    const std::string type = jsonAccessor["type"];
    // Sparse not supported yet.
    Tbx::assert_always(jsonAccessor.find("sparse") == std::end(jsonAccessor));

    // Get the subset of the buffer.
    const auto subBuffer = bufferView.buffer.subspan(accessorByteOffset);

    if constexpr (std::is_same_v<T, glm::vec2>) {
        Tbx::assert_always(type == "VEC2");
        Tbx::assert_always(componentType == GLTF_Float);
        return Iterator<T>(subBuffer, count, bufferView.stride);
    } else if constexpr (std::is_same_v<T, glm::vec3>) {
        Tbx::assert_always(type == "VEC3");
        Tbx::assert_always(componentType == GLTF_Float);
        return Iterator<T>(subBuffer, count, bufferView.stride);
    } else if constexpr (std::is_same_v<T, glm::vec4>) {
        Tbx::assert_always(type == "VEC4");
        Tbx::assert_always(componentType == GLTF_Float);
        return Iterator<T>(subBuffer, count, bufferView.stride);
    } else if constexpr (std::is_same_v<T, float>) {
        Tbx::assert_always(type == "SCALAR");
        Tbx::assert_always(componentType == GLTF_Float);
        return Iterator<T>(subBuffer, count, bufferView.stride);
    } else if constexpr (std::is_same_v<T, unsigned int>) {
        Tbx::assert_always(type == "SCALAR");
        if (componentType == GLTF_Byte) {
            return Iterator<T>(subBuffer, count, bufferView.stride, sizeof(char));
        } else if (componentType == GLTF_UnsignedByte) {
            return Iterator<T>(subBuffer, count, bufferView.stride, sizeof(unsigned char));
        } else if (componentType == GLTF_SignedShort) {
            return Iterator<T>(subBuffer, count, bufferView.stride, sizeof(short));
        } else if (componentType == GLTF_UnsignedShort) {
            return Iterator<T>(subBuffer, count, bufferView.stride, sizeof(unsigned short));
        } else if (componentType == GLTF_UnsignedInt) {
            return Iterator<T>(subBuffer, count, bufferView.stride, sizeof(unsigned));
        } else {
            spdlog::error("Unknown GLTF component type {}", componentType);
            return Iterator<T>({});
        }
    } else {
        spdlog::error("Unknown GLTF type {}", type);
        return {};
    }
}

static MeshCPU readMeshCPU(const nlohmann::json& jsonData, const GLTFBuffers& buffers, const nlohmann::json& jsonMesh, const std::filesystem::path& basePath, int dummyTextureIdx)
{
    Render::MeshCPU out {};
    for (const auto& jsonSubMesh : jsonMesh["primitives"]) {
        const auto& jsonAttributes = jsonSubMesh["attributes"];
        auto indices = readAccessor<uint32_t>(jsonData, buffers, (int)jsonSubMesh["indices"], basePath);
        auto positions = readAccessor<glm::vec3>(jsonData, buffers, (int)jsonAttributes["POSITION"], basePath);
        auto normals = readAccessor<glm::vec3>(jsonData, buffers, (int)jsonAttributes["NORMAL"], basePath);
        Iterator<glm::vec2> texCoords { glm::vec2(0) };
        if (auto iterTexCoordsIdx = jsonAttributes.find("TEXCOORD_0"); iterTexCoordsIdx != std::end(jsonAttributes))
            texCoords = readAccessor<glm::vec2>(jsonData, buffers, (int)jsonAttributes["TEXCOORD_0"], basePath);

        out.subMeshes.push_back(SubMesh {
            .indexStart = (uint32_t)out.indices.size(),
            .numIndices = (uint32_t)indices.count(),
            .baseVertex = (uint32_t)out.vertices.size(),
            .numVertices = (uint32_t)positions.count() });

        const int materialIdx = jsonSubMesh.value<int>("material", -1);
        MaterialCPU material;
        float roughnessFactor = 1.0f;
        if (materialIdx == -1) {
            // Assign default material if GLTF did not specify a material.
            material.baseColor = glm::vec3(0.8f);
            material.metallic = 0.0f;
            material.baseColorTextureIdx = dummyTextureIdx;
        } else {
            const auto& jsonMaterial = jsonData["materials"][materialIdx];
            const auto& jsonPBR = jsonMaterial["pbrMetallicRoughness"];
            material.baseColor = readJson<glm::vec3>(jsonPBR, "baseColorFactor", glm::vec3(1.0f));
            roughnessFactor = readJson<float>(jsonPBR, "roughnessFactor", 1.0f);
            material.metallic = readJson<float>(jsonPBR, "metallicFactor", 1.0f);
            if (auto iterBaseColorTexture = jsonPBR.find("baseColorTexture"); iterBaseColorTexture != std::end(jsonPBR)) {
                const int textureIdx = (*iterBaseColorTexture)["index"];
                material.baseColorTextureIdx = textureIdx;
            } else {
                material.baseColorTextureIdx = dummyTextureIdx;
            }
        }
        material.alpha = roughnessFactor * roughnessFactor;
        out.materials.push_back(material);

        const auto indexStart = out.indices.size();
        out.indices.reserve(out.indices.size() + indices.count());
        while (indices.isValid())
            out.indices.push_back(indices.nextValue());
        out.vertices.reserve(out.vertices.size() + positions.count());
        while (positions.isValid()) {
            out.vertices.push_back({ 
                .pos = positions.nextValue(),
                .normal = normals.nextValue(),
                .texCoord = texCoords.nextValue() });
        }
    }
    return out;
}

static Camera readCamera(const nlohmann::json& jsonCamera)
{
    if (jsonCamera["type"] == "perspective") {
        const auto& jsonPerspective = jsonCamera["perspective"];
        return Camera {
            .aspectRatio = jsonPerspective["aspectRatio"],
            .fovY = jsonPerspective["yfov"],
            .zNear = jsonPerspective["znear"],
            .zFar = jsonPerspective["zfar"]
        };
    } else {
        spdlog::warn("orthographic camera not supported! Replacing with perspective camera.");
        return {};
    }
}

static std::unordered_set<int> gatherSrgbTextureIds(const nlohmann::json& jsonData)
{
    if (!jsonData.contains("materials"))
        return {};

    std::unordered_set<int> srgbTextures;
    for (const auto& jsonMaterial : jsonData["materials"]) {
        if (auto jsonMetallicRoughness = jsonMaterial.find("pbrMetallicRoughness"); jsonMetallicRoughness != std::end(jsonMaterial)) {
            if (auto jsonBaseColorTexture = jsonMetallicRoughness->find("baseColorTexture"); jsonBaseColorTexture != std::end(*jsonMetallicRoughness)) {
                srgbTextures.insert((int)(*jsonBaseColorTexture)["index"]);
            }
        }
    }
    return srgbTextures;
}

static void validateGLTF(const nlohmann::json& jsonData)
{
    static std::unordered_set<std::string> supportedExtensions { "KHR_texture_basisu", "KHR_lights_punctual", "MATHIJS_optimized_assets", "MATHIJS_environment_light" };

    const auto& jsonAsset = jsonData["asset"];
    const std::string gltfVersion = jsonAsset["version"];

    if (gltfVersion != "2.0")
        spdlog::warn("GLTF version {} is not be suppored; continue at your own risk.", gltfVersion);

    if (const auto iterExtensionsUsed = jsonData.find("extensionsUsed"); iterExtensionsUsed != std::end(jsonData)) {
        for (const std::string extension : *iterExtensionsUsed) {
            if (!supportedExtensions.contains(extension)) {
                spdlog::warn("GLTF loader does not support extension {}. Some features may be missing.", extension);
            }
        }
    }
    if (const auto iterExtensionsRequired = jsonData.find("extensionsRequired"); iterExtensionsRequired != std::end(jsonData)) {
        for (const std::string extension : *iterExtensionsRequired) {
            if (!supportedExtensions.contains(extension)) {
                spdlog::error("GLTF loader does not support REQUIRED extension {}. The scene will likely fail to load.", extension);
            }
        }
    }
}

static void loadFromGLX(const nlohmann::json& jsonData, std::span<const std::byte> embeddedBuffer, const std::filesystem::path& baseFilePath, Scene& scene, std::vector<MeshCPU>& meshes, std::vector<TextureCPU>& textures)
{
    spdlog::info("Scene load starting");
    validateGLTF(jsonData);

    spdlog::info("Memory mapping data");
    GLTFBuffers buffers;
    if (auto iterJsonBuffers = jsonData.find("buffers"); iterJsonBuffers != std::end(jsonData)) {
        for (const auto& jsonBuffer : *iterJsonBuffers) {
            if (auto iterBufferURI = jsonBuffer.find("uri"); iterBufferURI != std::end(jsonBuffer)) {
                const std::filesystem::path fullFilePath = baseFilePath / *iterBufferURI;
                mio::mmap_source mappedFile { fullFilePath.c_str() };
                buffers.buffers.emplace_back(std::span<const std::byte>((const std::byte*)mappedFile.data(), mappedFile.size()));
                buffers.mappedFiles.emplace_back(std::move(mappedFile));
            } else {
                buffers.buffers.push_back(embeddedBuffer);
            }
        }
    }

    spdlog::info("Loading textures");
    const auto srgbTextures = gatherSrgbTextureIds(jsonData);
    int dummyTextureIdx = -1;
    {
        // Construct a list of functions that load the textures.
        std::vector<std::function<TextureCPU()>> textureLoadFuncs;
        if (const auto iterJsonTextures = jsonData.find("textures"); iterJsonTextures != std::end(jsonData)) {
            auto iterJsonImages = jsonData["images"];
            for (const auto& [textureIdx, jsonTexture] : iter::enumerate(*iterJsonTextures)) {
                int imageIdx = -1;
                if (auto iterTextureSource = jsonTexture.find("source"); iterTextureSource != std::end(jsonTexture)) {
                    imageIdx = int(*iterTextureSource);
                } else if (auto iterExtensions = jsonTexture.find("extensions"); iterExtensions != std::end(jsonTexture)) {
                    if (auto iterKHR_texture_basisu = iterExtensions->find("KHR_texture_basisu"); iterKHR_texture_basisu != std::end(*iterExtensions)) {
                        imageIdx = int((*iterKHR_texture_basisu)["source"]);
                    }
                }
                Tbx::assert_always(imageIdx >= 0);
                const auto jsonImage = iterJsonImages[imageIdx];

                const std::string mimeType = jsonImage["mimeType"];
                TextureCPU::TextureReadSettings readSettings {
                    .fileType = TextureFileType::Uknown,
                    .colorSpaceHint = srgbTextures.find((int)textureIdx) != std::end(srgbTextures) ? ColorSpace::Srgb : ColorSpace::Unknown
                };
                if (mimeType == "image/png")
                    readSettings.fileType = TextureFileType::PNG;
                else if (mimeType == "image/jpeg")
                    readSettings.fileType = TextureFileType::JPG;
                else if (mimeType == "image/ktx2")
                    readSettings.fileType = TextureFileType::KTX2;

                if (auto iterURI = jsonImage.find("uri"); iterURI != std::end(jsonImage)) {
                    const auto imagePath = baseFilePath / std::string(*iterURI);
                    textureLoadFuncs.emplace_back([=]() {
                        return TextureCPU::readFromFile(imagePath, readSettings);
                    });
                } else {
                    const int bufferViewIdx = jsonImage["bufferView"];
                    const auto bufferView = readBufferView(jsonData["bufferViews"][bufferViewIdx], buffers);
                    Tbx::assert_always(bufferView.stride == 0);
                    textureLoadFuncs.emplace_back([=]() {
                        return TextureCPU::readFromBuffer(bufferView.buffer, readSettings);
                    });
                }
            }
        }
        // Load the textures in parallel on multiple threads.
        textures.resize(textureLoadFuncs.size());
        std::transform(std::execution::par, std::begin(textureLoadFuncs), std::end(textureLoadFuncs), std::begin(textures), [](const auto& func) { return func(); });

        // Add a final "dummy" white texture which can be used by materials that do not have a diffuse texture.
        TextureCPU dummyTexture;
        dummyTexture.resolution = glm::ivec2(8);
        dummyTexture.isOpague = true;
        dummyTexture.textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        dummyTexture.pixelData.resize(8 * 8 * sizeof(uint32_t), (std::byte)0xFF);
        dummyTexture.mipLevels.push_back({ .mipLevelStart = 0, .rowPitch = 8 * sizeof(uint32_t) });
        dummyTextureIdx = (int)textures.size();
        textures.push_back(dummyTexture);
    }

    spdlog::info("Loading meshes");
    // Read all meshes into CPU memory so that the memory remains valid until the end of the function.
    if (const auto iterJsonMeshes = jsonData.find("meshes"); iterJsonMeshes != std::end(jsonData)) {
        std::vector<nlohmann::json> jsonMeshes(std::begin(*iterJsonMeshes), std::end(*iterJsonMeshes));
        meshes.resize(jsonMeshes.size());
        std::transform(std::execution::par, std::begin(jsonMeshes), std::end(jsonMeshes), std::begin(meshes),
            [&](const nlohmann::json& jsonMesh) {
                return readMeshCPU(jsonData, buffers, jsonMesh, baseFilePath, dummyTextureIdx);
            });
    }

    spdlog::info("Optimize mesh");
    // std::for_each(std::execution::seq, std::begin(meshes), std::end(meshes), [](MeshCPU& mesh) {
    //     mesh.removeDuplicateVertices();
    //     mesh.optimizeIndexVertexOrder();
    // });

    // spdlog::info("Generating LODs");
    // std::for_each(std::execution::seq, std::begin(meshes), std::end(meshes), [](MeshCPU& mesh) { mesh.generateSubMeshLODs(); });
    spdlog::info("Generating meshlets");
    std::for_each(std::execution::seq, std::begin(meshes), std::end(meshes), [](MeshCPU& mesh) { mesh.generateMeshlets(); });

    // Recursively visit the GLTF scene graph and flatten it into a one-dimensional array.
    scene.sun.intensity = glm::vec3(1.0f);
    scene.sun.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    const auto& jsonNodes = jsonData["nodes"];
    const std::function<void(int, const Core::Transform&)> traverseNodes = [&](int nodeIdx, const Core::Transform& parentTransform) {
        const auto& jsonNode = jsonNodes[nodeIdx];

        Core::Transform transform {
            .position = readJson<glm::vec3>(jsonNode, "translation", glm::vec3(0.0f)),
            .rotation = readJson<glm::quat>(jsonNode, "rotation", glm::quat()),
            .scale = readJson<glm::vec3>(jsonNode, "scale", glm::vec3(1.0f))
        };
        transform = parentTransform * transform;

        if (auto iterChildren = jsonNode.find("children"); iterChildren != std::end(jsonNode)) {
            for (int childNodeIdx : *iterChildren) {
                traverseNodes(childNodeIdx, transform);
            }
        }

        if (auto iterMeshIdx = jsonNode.find("mesh"); iterMeshIdx != std::end(jsonNode)) {
            const int meshIdx = *iterMeshIdx;
            Transformable<MeshInstance> meshInstance;
            meshInstance.meshIdx = meshIdx;
            meshInstance.transform = transform;
            scene.meshInstances.emplace_back(meshInstance);
        }

        if (auto iterCameraIdx = jsonNode.find("camera"); iterCameraIdx != std::end(jsonNode)) {
            const int cameraIdx = *iterCameraIdx;
            scene.camera = readCamera(jsonData["cameras"][cameraIdx]);
            scene.camera.transform = transform;
        }

        if (auto iterExtensions = jsonNode.find("extensions"); iterExtensions != std::end(jsonNode)) {
            const auto jsonExtensions = *iterExtensions;
            if (auto iterPunctualLight = jsonExtensions.find("KHR_lights_punctual"); iterPunctualLight != std::end(jsonExtensions)) {
                const int punctualLightIdx = (*iterPunctualLight)["light"];
                const auto jsonLight = jsonData["extensions"]["KHR_lights_punctual"]["lights"][punctualLightIdx];
                const std::string lightType = jsonLight["type"];
                if (lightType == "directional") {
                    // scene.sun.intensity = readJson<glm::vec3>(jsonLight, "color", glm::vec3(1.0f)) * jsonLight.value<float>("intensity", 1.0f);
                    scene.sun.intensity = readJson<glm::vec3>(jsonLight, "color", glm::vec3(1.0f));
                    scene.sun.direction = transform.rotation * glm::vec3(0, 0, -1);
                } else {
                    spdlog::warn("Ignoring unsupported light type \"{}\"", lightType);
                }
            }
        }
    };

    const int activeScene = jsonData["scene"];
    const auto& jsonScene = jsonData["scenes"][activeScene];
    for (const int nodeIdx : jsonScene["nodes"]) {
        traverseNodes(nodeIdx, Core::Transform());
    }
    spdlog::info("Scene load finished");
}

static void loadFromGLTF_CPU(const std::filesystem::path& filePath, Scene& scene, std::vector<MeshCPU>& meshes, std::vector<TextureCPU>& textures)
{
    Tbx::assert_always(std::filesystem::exists(filePath));
    std::ifstream file { filePath };
    const auto jsonData = nlohmann::json::parse(file);

    loadFromGLX(jsonData, {}, filePath.parent_path(), scene, meshes, textures);
}
static void loadFromGLB_CPU(const std::filesystem::path& filePath, Scene& scene, std::vector<MeshCPU>& meshes, std::vector<TextureCPU>& textures)
{
    struct GLBHeader {
        uint32_t magic;
        uint32_t version;
        uint32_t length;
    };
    struct GLBChunkHeader {
        uint32_t length;
        uint32_t chunkType;
    };

    Tbx::assert_always(std::filesystem::exists(filePath));
    const auto file = mio::mmap_source(filePath.c_str());
    std::span<const std::byte> fileContent { (const std::byte*)file.data(), file.size() };
    const auto read = [&]<typename T>(T& out) { std::memcpy(&out, fileContent.data(), sizeof(T)); fileContent = fileContent.subspan(sizeof(T)); };
    const auto readRange = [&]<typename T>(std::span<const T>& out, size_t size) { out = std::span((const T*)fileContent.data(), size); fileContent = fileContent.subspan(size*sizeof(T)); };
    const auto readString = [&](std::string& out, size_t size) { const auto* pStart = (const unsigned char*)fileContent.data();  out = std::string(pStart, pStart + size); fileContent = fileContent.subspan(size); };

    GLBHeader header;
    read(header);
    Tbx::assert_always(header.magic == 0x46546C67);

    nlohmann::json jsonData;
    std::span<const std::byte> buffer;
    while (!fileContent.empty()) {
        GLBChunkHeader chunkHeader;
        read(chunkHeader);

        if (chunkHeader.chunkType == 0x4E4F534A) { // JSON
            std::string jsonString;
            readString(jsonString, chunkHeader.length);
            jsonData = nlohmann::json::parse(jsonString);
        } else if (chunkHeader.chunkType == 0x004E4942) { // BINARY
            readRange(buffer, chunkHeader.length);
        } else {
            throw std::runtime_error("Unknown GLB chunk type");
        }
    }

    loadFromGLX(jsonData, buffer, filePath.parent_path(), scene, meshes, textures);
}

static void uploadToGPU(Scene& scene, std::span<const MeshCPU> meshesCPU, std::span<const TextureCPU> texturesCPU, RenderContext& renderContext);

void Scene::loadFromGLTF(const std::filesystem::path& filePath, RenderContext& renderContext)
{
    std::vector<MeshCPU> meshesCPU;
    std::vector<TextureCPU> texturesCPU;
    loadFromGLTF_CPU(filePath, *this, meshesCPU, texturesCPU);
    uploadToGPU(*this, meshesCPU, texturesCPU, renderContext);
}

void Scene::loadFromGLB(const std::filesystem::path& filePath, RenderContext& renderContext)
{
    std::vector<MeshCPU> meshesCPU;
    std::vector<TextureCPU> texturesCPU;
    loadFromGLB_CPU(filePath, *this, meshesCPU, texturesCPU);
    uploadToGPU(*this, meshesCPU, texturesCPU, renderContext);
}

void Scene::loadFromBinary(const std::filesystem::path& filePath, RenderContext& renderContext)
{
    Util::BinaryReader reader { filePath };

    uint64_t binaryFileVersionNumberCheck;
    reader.read(binaryFileVersionNumberCheck);
    Tbx::assert_always(binaryFileVersionNumberCheck == binaryFileVersionNumber);

    reader.read(this->sun);
    reader.read(this->meshInstances);
    reader.read(this->camera);

    std::vector<MeshCPU> meshesCPU;
    std::vector<TextureCPU> texturesCPU;
    reader.read(meshesCPU);
    reader.read(texturesCPU);
    uploadToGPU(*this, meshesCPU, texturesCPU, renderContext);
}

void Scene::loadFromMeshes(std::span<const MeshCPU> meshesCPU, std::span<const TextureCPU> texturesCPU, RenderContext& renderContext)
{
    uploadToGPU(*this, meshesCPU, texturesCPU, renderContext);
}

static void storeBinary(Util::BinaryWriter& writer, const Scene& scene, std::span<const MeshCPU> meshes, std::span<const TextureCPU> textures)
{
    writer.write(binaryFileVersionNumber);

    writer.write(scene.sun);
    writer.write(scene.meshInstances);
    writer.write(scene.camera);

    writer.write(meshes);
    writer.write(textures);
}

void Scene::glb2binary(const std::filesystem::path& inFilePath, const std::filesystem::path& outFilePath)
{
    Tbx::assert_always(std::filesystem::exists(inFilePath));

    Scene scene;
    std::vector<MeshCPU> meshes;
    std::vector<TextureCPU> textures;
    loadFromGLB_CPU(inFilePath, scene, meshes, textures);

    Util::BinaryWriter writer { outFilePath };
    storeBinary(writer, scene, meshes, textures);
}

void Scene::gltf2binary(const std::filesystem::path& inFilePath, const std::filesystem::path& outFilePath)
{
    Tbx::assert_always(std::filesystem::exists(inFilePath));

    Scene scene;
    std::vector<MeshCPU> meshes;
    std::vector<TextureCPU> textures;
    loadFromGLTF_CPU(inFilePath, scene, meshes, textures);

    Util::BinaryWriter writer { outFilePath };
    storeBinary(writer, scene, meshes, textures);
}

static void createBindlessScene(Scene& scene, std::span<const MeshCPU> meshesCPU, std::span<const TextureCPU> texturesCPU, RenderContext& renderContext)
{
    Tbx::assert_always(scene.meshes.size() == meshesCPU.size());
    Tbx::assert_always(scene.textures.size() == texturesCPU.size());

    std::vector<RenderAPI::SRVDesc> indexBuffers;
    std::vector<RenderAPI::SRVDesc> meshletBuffers;
    std::vector<RenderAPI::SRVDesc> vertexBuffers;
    std::vector<ShaderInputs::BindlessMesh> meshes;
    std::vector<ShaderInputs::BindlessSubMesh> subMeshes;
    for (size_t meshIdx = 0; meshIdx < scene.meshes.size(); ++meshIdx) {
        const auto& mesh = scene.meshes[meshIdx];
        const auto& meshCPU = meshesCPU[meshIdx];

        indexBuffers.push_back(RenderAPI::createSRVDesc<uint32_t>(mesh.indexBuffer, 0, mesh.numIndices));
        meshletBuffers.push_back(RenderAPI::createSRVDesc<Meshlet>(mesh.meshletBuffer, 0, mesh.numMeshlets));
        vertexBuffers.push_back(RenderAPI::createSRVDesc<ShaderInputs::Vertex>(mesh.vertexBuffer, 0, mesh.numVertices));

        ShaderInputs::BindlessMesh bindlessMesh {
            .subMeshStart = (uint32_t)subMeshes.size(),
            .numSubMeshes = (uint32_t)mesh.subMeshes.size(),
            .numMeshlets = mesh.numMeshlets
        };
        meshes.push_back(bindlessMesh);

        for (size_t subMeshIdx = 0; subMeshIdx < mesh.subMeshes.size(); ++subMeshIdx) {
            const auto& subMesh = mesh.subMeshes[subMeshIdx];
            const auto& material = meshCPU.materials[subMeshIdx];

            ShaderInputs::BindlessSubMesh bindlessSubMesh {
                .indexStart = subMesh.indexStart,
                .numIndices = subMesh.numIndices,
                .baseVertex = subMesh.baseVertex,
                .numVertices = subMesh.numVertices,
                .meshletStart = subMesh.meshletStart,
                .numMeshlets = subMesh.numMeshlets,
                .material = material
            };
            subMeshes.push_back(bindlessSubMesh);
        }
    }

    std::vector<ShaderInputs::BindlessMeshInstance> meshInstances(scene.meshInstances.size());
    std::transform(std::begin(scene.meshInstances), std::end(scene.meshInstances), std::begin(meshInstances),
        [&](const Transformable<MeshInstance>& instance) {
            return ShaderInputs::BindlessMeshInstance {
                .modelMatrix = instance.transform.matrix(),
                .normalMatrix = instance.transform.normalMatrix(),
                .meshIdx = instance.meshIdx
            };
        });

    scene.bindlessSubMeshes = renderContext.createBufferWithArrayData<ShaderInputs::BindlessSubMesh>(subMeshes, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    scene.bindlessSubMeshes->SetName(L"BindlessSubMeshes");
    scene.bindlessMeshes = renderContext.createBufferWithArrayData<ShaderInputs::BindlessMesh>(meshes, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    scene.bindlessMeshes->SetName(L"BindlessMeshes");
    scene.bindlessMeshInstances = renderContext.createBufferWithArrayData<ShaderInputs::BindlessMeshInstance>(meshInstances, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    scene.bindlessMeshInstances->SetName(L"BindlessMeshInstances");

    std::vector<RenderAPI::SRVDesc> baseColorTextures(scene.textures.size());
    std::transform(std::begin(scene.textures), std::end(scene.textures), std::begin(baseColorTextures),
        [](const Render::Texture& texture) { return (RenderAPI::SRVDesc)texture; });

    ShaderInputs::BindlessScene inputs;
    inputs.setIndexBuffers(indexBuffers);
    inputs.setMeshlets(meshletBuffers);
    inputs.setVertexBuffers(vertexBuffers);
    inputs.setSubMeshes(RenderAPI::createSRVDesc<ShaderInputs::BindlessSubMesh>(scene.bindlessSubMeshes, 0, (uint32_t)subMeshes.size()));
    inputs.setMeshes(RenderAPI::createSRVDesc<ShaderInputs::BindlessMesh>(scene.bindlessMeshes, 0, (uint32_t)meshes.size()));
    inputs.setMeshInstances(RenderAPI::createSRVDesc<ShaderInputs::BindlessMeshInstance>(scene.bindlessMeshInstances, 0, (uint32_t)meshInstances.size()));
    inputs.setBaseColorTextures(baseColorTextures);
    scene.bindlessScene = inputs.generatePersistentBindings(renderContext);
}

static void uploadToGPU(Scene& scene, std::span<const MeshCPU> meshesCPU, std::span<const TextureCPU> texturesCPU, RenderContext& renderContext)
{
    // Upload all textures to the GPU.
    spdlog::info("Uploading textures");
    size_t textureMemoryUsage = 0;
    scene.textures.clear();
    for (const auto& textureCPU : texturesCPU) {
        if (textureCPU.pixelData.empty()) {
            scene.textures.emplace_back();
            continue;
        }

        textureMemoryUsage += textureCPU.pixelData.size();
        scene.textures.push_back(Texture::uploadToGPU(textureCPU, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, renderContext));
    }
    spdlog::info("Texture memory usage: {}MiB", textureMemoryUsage >> 20);

    // Ensure all material descriptors have been copied to the GPU.
    renderContext.cbvSrvUavDescriptorStaticAllocator.flush();
    renderContext.waitForIdle();

    // Upload all meshes to the GPU.
    spdlog::info("Uploading meshes");
    size_t numOpague = 0, numTransparent = 0;
    size_t meshMemoryUsage = 0;
    for (const auto& meshCPU : meshesCPU) {
        Render::Mesh meshGPU;
        meshGPU.indexBuffer = renderContext.createBufferWithArrayData<uint32_t>(meshCPU.indices, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_INDEX_BUFFER);
        meshGPU.indexBuffer->SetName(L"IndexBuffer");
        meshGPU.indexBufferView = D3D12_INDEX_BUFFER_VIEW {
            .BufferLocation = meshGPU.indexBuffer->GetGPUVirtualAddress(),
            .SizeInBytes = (unsigned)(meshCPU.indices.size() * sizeof(uint32_t)),
            .Format = DXGI_FORMAT_R32_UINT
        };
        meshGPU.vertexBuffer = renderContext.createBufferWithArrayData<ShaderInputs::Vertex>(meshCPU.vertices, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        meshGPU.vertexBuffer->SetName(L"VertexBuffer");
        meshGPU.vertexBufferView = D3D12_VERTEX_BUFFER_VIEW {
            .BufferLocation = meshGPU.vertexBuffer->GetGPUVirtualAddress(),
            .SizeInBytes = (unsigned)(meshCPU.vertices.size() * sizeof(ShaderInputs::Vertex)),
            .StrideInBytes = sizeof(ShaderInputs::Vertex)
        };

        meshGPU.meshletBuffer = renderContext.createBufferWithArrayData<Meshlet>(meshCPU.meshlets, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        meshGPU.meshletBuffer->SetName(L"MeshletBuffer");

        meshGPU.numIndices = (uint32_t)meshCPU.indices.size();
        meshGPU.numMeshlets = (uint32_t)meshCPU.meshlets.size();
        meshGPU.numVertices = (uint32_t)meshCPU.vertices.size();
        meshGPU.vertexStride = (uint32_t)sizeof(ShaderInputs::Vertex);
        // meshGPU.bounds = meshCPU.bounds;
        meshGPU.subMeshes = meshCPU.subMeshes;
        meshMemoryUsage += meshGPU.indexBufferView.SizeInBytes + meshGPU.vertexBufferView.SizeInBytes;

        for (const MaterialCPU& materialCPU : meshCPU.materials) {
            const auto isOpague = texturesCPU[materialCPU.baseColorTextureIdx].isOpague;

            if (isOpague)
                ++numOpague;
            else
                ++numTransparent;

            ShaderInputs::SinglePBRMaterial shaderMaterial;
            shaderMaterial.setMaterial(materialCPU);
            shaderMaterial.setBaseColorTexture(scene.textures[materialCPU.baseColorTextureIdx]);
            meshGPU.materials.push_back(Material {
                .shaderInputs = shaderMaterial.generatePersistentBindings(renderContext),
                .isOpague = isOpague });
        }
        scene.meshes.push_back(std::move(meshGPU));
    }
    scene.vertexBufferState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

    // Create a bindless version of the scene
    createBindlessScene(scene, meshesCPU, texturesCPU, renderContext);

    spdlog::info("Mesh memory usage: {}MiB", meshMemoryUsage >> 20);
    Tbx::assert_always(scene.meshes.size() == meshesCPU.size());
    spdlog::info("{} opague; {} transparent", numOpague, numTransparent);

    // Ensure all material descriptors have been copied to the GPU.
    renderContext.cbvSrvUavDescriptorStaticAllocator.flush();
    renderContext.waitForIdle();
}

}
