#pragma once
#include "Engine/Core/Bounds.h"
#include "Engine/Core/Transform.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/Light.h"
#include "Engine/Render/Mesh.h"
#include "Engine/Render/ShaderInputs/bindpoints/RenderPass.h"
#include "Engine/Render/Texture.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include "Engine/Util/ForwardDeclares.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <nlohmann/json_fwd.hpp>
DISABLE_WARNINGS_POP()
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace Render {

template <typename T>
struct Transformable : public T {
    Transformable() = default;
    Transformable(const T& parent)
        : T(parent)
    {
    }
    Transformable(T&& parent)
        : T(std::move(parent))
    {
    }

    Core::Transform transform;
    Core::Transform previousTransform; // Transform in the last frame.
};

struct MeshInstance {
    uint32_t meshIdx; // Index in meshes array.
    uint32_t instanceContributionToHitGroupIndex; // See DXR specs.
};
struct EnvironmentMap {
    Render::Texture texture;
    float strength = 1.0f;
};

struct Scene {
    std::optional<EnvironmentMap> optEnvironmentMap;
    std::vector<Texture> textures;
    std::vector<Mesh> meshes;
    std::vector<Transformable<MeshInstance>> meshInstances;
    D3D12_RESOURCE_STATES vertexBufferState;

    RenderAPI::D3D12MAResource bindlessSubMeshes;
    RenderAPI::D3D12MAResource bindlessMeshes;
    RenderAPI::D3D12MAResource bindlessMeshInstances;
    ShaderInputs::RenderPass bindlessScene;

    Render::DirectionalLight sun;
    Transformable<Render::Camera> camera;

    int64_t frameIdx = 0;
    glm::vec2 cameraJitterTAA { 0 }; // Measured in pixels
    RenderAPI::D3D12MAResource tlas;
    glm::ivec2 mouseCursorPosition;

public:
    void transitionVertexBuffers(ID3D12GraphicsCommandList6* pCommandList, D3D12_RESOURCE_STATES desiredState);
    void updateHistoricalTransformMatrices(); // Copies transform matrices.

    void buildRayTracingAccelerationStructure(Render::RenderContext& renderContext);
    RenderAPI::SRVDesc tlasBinding() const;

    static void gltf2binary(const std::filesystem::path& inFilePath, const std::filesystem::path& outFilePath);
    static void glb2binary(const std::filesystem::path& inFilePath, const std::filesystem::path& outFilePath);

    void loadFromGLTF(const std::filesystem::path& filePath, RenderContext& renderContext);
    void loadFromGLB(const std::filesystem::path& filePath, RenderContext& renderContext);
    void loadFromBinary(const std::filesystem::path& filePath, RenderContext& renderContext);
    void loadFromMeshes(std::span<const MeshCPU> meshes, std::span<const TextureCPU> textures, RenderContext& renderContext);
};

}
