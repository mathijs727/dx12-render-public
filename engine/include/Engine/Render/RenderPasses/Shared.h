#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/Render/ShaderInputs/structs/RTScreenCamera.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
DISABLE_WARNINGS_POP()
#include <filesystem>
#include <span>

namespace Render {
struct CullingResult;
struct DirectionalLight;
template <bool BindMaterials>
void drawVisibleScene(
    RenderContext& renderContext,
    ID3D12GraphicsCommandList5* pCommandList,
    const RenderAPI::PipelineState& pipelineState,
    const glm::mat4& viewProjectionMatrix,
    const CullingResult& cullingResult);

ShaderInputs::RTScreenCamera getRayTracingCamera(const Transformable<Render::Camera>& camera);
glm::mat4 getCameraViewProjection(const Transformable<Render::Camera>& camera);
glm::mat4 getShadowMapViewProjection(const DirectionalLight& light);

void setViewportAndScissor(ID3D12GraphicsCommandList5* pCommandList, const glm::ivec2& resolution);
void setFullScreenPassPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC&);
void setDefaultVertexLayout(D3D12_GRAPHICS_PIPELINE_STATE_DESC&, std::span<D3D12_INPUT_ELEMENT_DESC>);
RenderAPI::Shader loadEngineShader(ID3D12Device5* pDevice, const std::filesystem::path& filePath);

}
