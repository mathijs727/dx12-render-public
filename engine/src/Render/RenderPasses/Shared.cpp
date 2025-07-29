#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Core/Transform.h"
#include "Engine/Render/Light.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/StaticMeshVertex.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <cmath>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <magic_enum/magic_enum.hpp>
DISABLE_WARNINGS_POP()

using namespace RenderAPI;

namespace Render {

ShaderInputs::RTScreenCamera getRayTracingCamera(const Transformable<Render::Camera>& camera)
{
    const glm::vec3 position = camera.transform.position;
    const glm::vec3 forward = camera.transform.rotation * glm::vec3(0, 0, -1);

    const float screenScaleY = std::tan(camera.fovY / 2.0f);
    const float screenScaleX = screenScaleY * camera.aspectRatio;
    const glm::vec3 screenU = camera.transform.rotation * glm::vec3(screenScaleX, 0, 0);
    const glm::vec3 screenV = camera.transform.rotation * glm::vec3(0, screenScaleY, 0);

    return {
        .origin = position,
        .forward = forward,
        .screenU = screenU,
        .screenV = screenV
    };
}

/* template <bool BindMaterials>
void drawVisibleScene(
    RenderContext& renderContext,
    ID3D12GraphicsCommandList5* pCommandList,
    const RenderAPI::PipelineState& pipelineState,
    const glm::mat4& viewProjection,
    const CullingResult& cullingResult)
{
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    MeshGPU const* pCurrentMesh = nullptr;
    for (const auto& visibleMesh : cullingResult) {
        ShaderInputs::StaticMeshVertex instanceInput {};
        instanceInput.setModelViewProjectionMatrix(viewProjection * visibleMesh.pTransformComponent->matrix());
        instanceInput.setModelMatrix(visibleMesh.pTransformComponent->matrix());
        instanceInput.setModelNormalMatrix(visibleMesh.pTransformComponent->normalMatrix());
        const auto compiledInstanceInputs = instanceInput.generateTransientBindings(renderContext);
        ShaderInputs::DefaultLayout::bindInstanceGraphics(pCommandList, compiledInstanceInputs);

        const auto& meshGPU = visibleMesh.pMeshComponent->mesh;
        pCommandList->IASetIndexBuffer(&meshGPU.indexBufferView);
        pCommandList->IASetVertexBuffers(0, 1, &meshGPU.vertexBufferView);

        for (const auto subMeshIdx : visibleMesh.visibleSubMeshes) {
            // TODO: materials...
            if constexpr (BindMaterials) {
                const auto& material = meshGPU.materials[subMeshIdx];
                ShaderInputs::DefaultLayout::bindMaterialGraphics(pCommandList, material.shaderInputs);
            }
            const auto& subMesh = meshGPU.subMeshes[subMeshIdx];
            pCommandList->DrawIndexedInstanced(subMesh.numIndices, 1, subMesh.indexStart, subMesh.baseVertex, 0);
        }
    }
}*/

glm::mat4 getCameraViewProjection(const Transformable<Camera>& camera)
{
    return camera.projectionMatrix() * camera.transform.viewMatrix();
}

glm::mat4 getShadowMapViewProjection(const DirectionalLight& light)
{
    const glm::vec3 up = glm::normalize(glm::cross(light.direction, glm::vec3(1, 0, 0)));
    const glm::vec3 left = glm::normalize(glm::cross(light.direction, up));
    return glm::mat4(glm::mat3(left, up, light.direction));
}

/*void drawVisibleSceneWithMaterials(RenderContext& renderContext, ID3D12GraphicsCommandList5* pCommandList, std::span<const RenderAPI::PipelineState, numMaterials> pipelineStates, const glm::mat4& viewProjection, const CullingResult& cullingResult)
{
    // Set an initial graphics pipeline state & root signature.
    pCommandList->SetGraphicsRootSignature(pipelineStates[0].pRootSignature.Get());
    pCommandList->SetPipelineState(pipelineStates[0].pPipelineState.Get());
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    int currentMaterialTypeIdx = -1;
    for (const auto& visibleMesh : cullingResult) {
        sig::StaticMeshVertex instanceInput {};
        instanceInput.setModelViewProjectionMatrix(viewProjection * visibleMesh.pTransformComponent->matrix());
        instanceInput.setModelMatrix(visibleMesh.pTransformComponent->matrix());
        instanceInput.setModelNormalMatrix(visibleMesh.pTransformComponent->normalMatrix());
        const auto compiledInstanceInputs = instanceInput.compileTransient(renderContext);
        compiledInstanceInputs.bindGraphics(pCommandList); // Rebind in case of root signature change.

        const auto& meshGPU = visibleMesh.pMeshComponent->mesh;
        pCommandList->IASetIndexBuffer(&meshGPU.indexBufferView);
        pCommandList->IASetVertexBuffers(0, 1, &meshGPU.vertexBufferView);

        for (const auto subMeshIdx : visibleMesh.visibleSubMeshes) {
            const auto& subMesh = meshGPU.subMeshes[subMeshIdx];
            const auto& material = visibleMesh.pMeshComponent->materials[subMeshIdx];
            const auto materialTypeIdx = magic_enum::enum_integer(material.materialType);
            if (materialTypeIdx != currentMaterialTypeIdx) {
                pCommandList->SetGraphicsRootSignature(pipelineStates[materialTypeIdx].pRootSignature.Get());
                pCommandList->SetPipelineState(pipelineStates[materialTypeIdx].pPipelineState.Get());
                material.pStaticMaterialTypeInputs->graphics.bindGraphics(pCommandList);
                compiledInstanceInputs.bindGraphics(pCommandList); // Rebind in case of root signature change.
                currentMaterialTypeIdx = materialTypeIdx;
            }
            material.instanceInputs.bindGraphics(pCommandList);
            pCommandList->DrawIndexedInstanced(subMesh.numIndices, 1, subMesh.indexStart, subMesh.baseVertex, 0);
        }
    }
} */

void setViewportAndScissor(ID3D12GraphicsCommandList5* pCommandList, const glm::ivec2& resolution)
{
    const auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)resolution.x, (float)resolution.y);
    pCommandList->RSSetViewports(1, &viewport);

    const auto scissorRect = CD3DX12_RECT(0, 0, resolution.x, resolution.y);
    pCommandList->RSSetScissorRects(1, &scissorRect);
}

void setFullScreenPassPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC& pipelineStateDesc)
{
    pipelineStateDesc.DepthStencilState.DepthEnable = false;
    pipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
}

RenderAPI::Shader loadEngineShader(ID3D12Device5* pDevice, const std::filesystem::path& filePath)
{
    //static const std::filesystem::path basePath = ENGINE_SHADER_BINARY_DIR;
    //return RenderAPI::loadShader(pDevice, basePath / filePath);
    return RenderAPI::loadShader(pDevice, "shaders" / filePath);
}

void setDefaultVertexLayout(D3D12_GRAPHICS_PIPELINE_STATE_DESC& pipelineStateDesc, std::span<D3D12_INPUT_ELEMENT_DESC> inputElements)
{
    inputElements[0] = RenderAPI::sensibleDefaultsInputElementDesc();
    inputElements[0].SemanticName = "POSITION";
    inputElements[0].SemanticIndex = 0;
    inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    static_assert(sizeof(ShaderInputs::Vertex::pos) == 12);

    inputElements[1] = RenderAPI::sensibleDefaultsInputElementDesc();
    inputElements[1].SemanticName = "NORMAL";
    inputElements[1].SemanticIndex = 0;
    inputElements[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    static_assert(sizeof(ShaderInputs::Vertex::normal) == 12);

    inputElements[2] = RenderAPI::sensibleDefaultsInputElementDesc();
    inputElements[2].SemanticName = "TEXCOORD";
    inputElements[2].SemanticIndex = 0;
    inputElements[2].Format = DXGI_FORMAT_R32G32_FLOAT;
    static_assert(sizeof(ShaderInputs::Vertex::texCoord) == 8);

    /* inputElements[3] = RenderAPI::sensibleDefaultsInputElementDesc();
    inputElements[3].SemanticName = "TEXCOORD";
    inputElements[3].SemanticIndex = 1;
    inputElements[3].Format = DXGI_FORMAT_R32G32_FLOAT; */

    pipelineStateDesc.InputLayout.pInputElementDescs = inputElements.data();
    pipelineStateDesc.InputLayout.NumElements = 3;
}

// template void drawVisibleScene<true>(RenderContext&, ID3D12GraphicsCommandList5*, const RenderAPI::PipelineState&, const glm::mat4&, const CullingResult&);
// template void drawVisibleScene<false>(RenderContext&, ID3D12GraphicsCommandList5*, const RenderAPI::PipelineState&, const glm::mat4&, const CullingResult&);

}
