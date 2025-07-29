#include "Engine/Render/RenderPasses/Rasterization/MeshShading.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Debug/VisualDebug.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/MeshShadingBindless.h"
#include "Engine/Render/ShaderInputs/inputgroups/MeshShading.h"
#include "Engine/Render/ShaderInputs/inputgroups/StaticMeshVertex.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/Util/Align.h"

namespace Render {

static RenderAPI::SRVDesc createIndexBufferDesc(const Mesh& mesh, const SubMesh& subMesh)
{
    // Create a binding to the mesh index & vertex buffers so we can decode materials from the hitgroup shader.
    RenderAPI::SRVDesc indexBufferDesc;
    indexBufferDesc.desc.Format = DXGI_FORMAT_UNKNOWN;
    indexBufferDesc.desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    indexBufferDesc.desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    Tbx::assert_always(subMesh.indexStart % 3 == 0);
    Tbx::assert_always(subMesh.numIndices % 3 == 0);
    indexBufferDesc.desc.Buffer.FirstElement = subMesh.indexStart / 3;
    indexBufferDesc.desc.Buffer.NumElements = subMesh.numIndices / 3;
    indexBufferDesc.desc.Buffer.StructureByteStride = 3 * sizeof(uint32_t);
    indexBufferDesc.desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    indexBufferDesc.pResource = mesh.indexBuffer;
    return indexBufferDesc;
}

void MeshShadingPass::execute(const FrameGraphRegistry<MeshShadingPass>& resources, const FrameGraphExecuteArgs& args)
{
    auto pCommandList = args.pCommandList;
    setViewportAndScissor(pCommandList, resources.getTextureResolution<"framebuffer">());
    pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    pCommandList->SetPipelineState(m_pPipelineState.Get());

    settings.pScene->transitionVertexBuffers(pCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    const auto viewMatrix = settings.pScene->camera.transform.viewMatrix();
    const auto viewProjectionMatrix = settings.pScene->camera.projectionMatrix() * viewMatrix;
    if (settings.bindless) {
        ShaderInputs::MeshShadingBindless passInput;
        passInput.setViewProjectionMatrix(viewProjectionMatrix);
        const auto compiledInputs = passInput.generateTransientBindings(*args.pRenderContext);
        ShaderInputs::DefaultLayout::bindInstanceGraphics(pCommandList, compiledInputs);
        ShaderInputs::DefaultLayout::bindPassGraphics(pCommandList, settings.pScene->bindlessScene);

        pCommandList->DispatchMesh((uint32_t)settings.pScene->meshes.size(), 1, 1);
    } else {
        for (const auto& instance : settings.pScene->meshInstances) {
            const auto modelMatrix = instance.transform.matrix();
            // const auto modelMatrix = glm::identity<glm::mat4>();
            const auto& mesh = settings.pScene->meshes[instance.meshIdx];

            ShaderInputs::MeshShading instanceInput {};
            instanceInput.setMeshlets(RenderAPI::createSRVDesc<Render::Meshlet>(mesh.meshletBuffer, 0, mesh.numMeshlets));
            instanceInput.setVertices(RenderAPI::createSRVDesc<ShaderInputs::Vertex>(mesh.vertexBuffer, 0, mesh.numVertices));
            instanceInput.setModelViewProjectionMatrix(viewProjectionMatrix * modelMatrix);
            instanceInput.setModelMatrix(modelMatrix);
            instanceInput.setModelViewMatrix(viewMatrix * modelMatrix);
            instanceInput.setModelNormalMatrix(instance.transform.normalMatrix());
            for (size_t i = 0; i < mesh.subMeshes.size(); ++i) {
                const auto& subMesh = mesh.subMeshes[i];
                const auto& material = mesh.materials[i];

                constexpr unsigned maxBatchSize = 65535;
                for (unsigned batchStart = 0; batchStart < subMesh.numMeshlets; batchStart += maxBatchSize) {
                    const unsigned batchEnd = std::min(batchStart + maxBatchSize, subMesh.numMeshlets);
                    const unsigned batchSize = batchEnd - batchStart;
                    instanceInput.setMeshletStart(subMesh.meshletStart + batchStart);
                    const auto compiledInputs = instanceInput.generateTransientBindings(*args.pRenderContext);
                    ShaderInputs::DefaultLayout::bindInstanceGraphics(pCommandList, compiledInputs);
                    pCommandList->DispatchMesh(batchSize, 1, 1);
                }
            }
        }
    }
}

void MeshShadingPass::initialize(const Render::RenderContext& renderContext, D3DX12_MESH_SHADER_PIPELINE_STATE_DESC pipelineStateDesc)
{
    std::optional<RenderAPI::Shader> optAmplificationShader;
    RenderAPI::Shader meshShader;
    if (settings.bindless) {
        optAmplificationShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Rasterization/mesh_shading_bindless_as.dxil");
        meshShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Rasterization/mesh_shading_bindless_ms.dxil");
    } else {
        meshShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Rasterization/mesh_shading_ms.dxil");
    }
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Rasterization/mesh_shading_ps.dxil");
    m_pRootSignature = ShaderInputs::DefaultLayout::getRootSignature(renderContext.pDevice.Get());

    pipelineStateDesc.pRootSignature = m_pRootSignature.Get();
    pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipelineStateDesc.DepthStencilState.DepthEnable = true;
    pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    if (optAmplificationShader)
        pipelineStateDesc.AS = *optAmplificationShader;
    pipelineStateDesc.MS = meshShader;
    pipelineStateDesc.PS = pixelShader;

    auto psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(pipelineStateDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
    streamDesc.pPipelineStateSubobjectStream = &psoStream;
    streamDesc.SizeInBytes = sizeof(psoStream);
    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&m_pPipelineState)));
}
}
