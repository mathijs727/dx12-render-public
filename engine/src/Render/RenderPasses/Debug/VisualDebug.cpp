#include "Engine/Render/RenderPasses/Debug/VisualDebug.h"
#include "Engine/Core/Keyboard.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/Debug.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/Mesh.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/RenderPasses/Shared.h"
#include "Engine/Render/Scene.h"
#include "Engine/Render/ShaderInputs/inputgroups/StaticMeshVertex.h"
#include "Engine/Render/ShaderInputs/inputgroups/VisualDebugCamera.h"
#include "Engine/Render/ShaderInputs/inputlayouts/DefaultLayout.h"
#include "Engine/RenderAPI/CommandListManager.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/RenderAPI.h"
#include "Engine/RenderAPI/SwapChain.h"
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <imgui.h>
DISABLE_WARNINGS_POP()
#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <tbx/error_handling.h>
#include <vector>

static constexpr uint32_t MaxNumDrawCommands = 1024;
// static uint32_t ConstantsSize = 3 * 4 * 4 * sizeof(float); // Three 4x4 matrices (ModelViewProjection, Model, Normals)
static constexpr uint32_t ConstantsSize = 16 * sizeof(float); // 3x3 matrix (aligned to 12 floats) + float3 + float.
static constexpr uint32_t ConstantBufferSize = MaxNumDrawCommands * ConstantsSize;
static constexpr uint32_t CommandCountAddress = 0;
static constexpr uint32_t CommandStartAddress = sizeof(uint64_t); // Aligned to uint64_t to prevent alignment issues with DrawCommand.

struct DrawCommand {
    D3D12_GPU_VIRTUAL_ADDRESS cbv;
    D3D12_DRAW_INDEXED_ARGUMENTS drawIndexed;
};

namespace Render {

void VisualDebugPass::execute(const FrameGraphRegistry<VisualDebugPass>& resources, const FrameGraphExecuteArgs& args)
{
    auto pCommandList = args.pCommandList;
    if (m_firstFrame) {
        clearCommandBuffer(*args.pRenderContext, pCommandList);
        m_firstFrame = false;
    }

    // Transition / barrier resources.
    {
        std::array barriers1 {
            CD3DX12_RESOURCE_BARRIER::Transition(m_pCommandBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
            CD3DX12_RESOURCE_BARRIER::Transition(m_pConstantsBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
        };
        pCommandList->ResourceBarrier((UINT)barriers1.size(), barriers1.data());
    }

    // Perform indirect draw.
    setViewportAndScissor(pCommandList, resources.getTextureResolution<"framebuffer">());
    pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    pCommandList->SetPipelineState(m_pPipelineState.Get());

    ShaderInputs::VisualDebugCamera passInputs;
    passInputs.setCameraPosition(settings.pScene->camera.transform.position);
    passInputs.setViewProjectionMatrix(settings.pScene->camera.projectionMatrix() * settings.pScene->camera.transform.viewMatrix());
    auto compiledPassInputs = passInputs.generateTransientBindings(*args.pRenderContext);
    ShaderInputs::DefaultLayout::bindPassGraphics(pCommandList, compiledPassInputs);

    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->IASetIndexBuffer(&m_arrow.indexBufferView);
    pCommandList->IASetVertexBuffers(0, 1, &m_arrow.vertexBufferView);
    pCommandList->ExecuteIndirect(
        m_pCommandSignature.Get(), MaxNumDrawCommands,
        m_pCommandBuffer.Get(), CommandStartAddress,
        m_pCommandBuffer.Get(), CommandCountAddress);

    // Transition / barrier resources.
    {
        std::array barriers2 {
            CD3DX12_RESOURCE_BARRIER::Transition(m_pCommandBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            CD3DX12_RESOURCE_BARRIER::Transition(m_pConstantsBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        };
        pCommandList->ResourceBarrier((UINT)barriers2.size(), barriers2.data());
    }

    m_paused = m_shouldPause;
    if (!m_paused)
        clearCommandBuffer(*args.pRenderContext, pCommandList);
}

void VisualDebugPass::displayGUI()
{
    ImGui::Checkbox("Paused", &m_shouldPause);
    if (settings.pKeyboard && settings.pKeyboard->isKeyPress(Core::Key::P))
        this->togglePause();
}

ShaderInputs::VisualDebug VisualDebugPass::getShaderInputs() const
{
    auto out = m_shaderInputs;
    out.paused = m_paused;
    return out;
}

void VisualDebugPass::togglePause()
{
    m_shouldPause = !m_shouldPause;
}

VisualDebugPass::Drawable VisualDebugPass::createDrawable(std::span<const uint32_t> indices, std::span<const ShaderInputs::Vertex> vertices, RenderContext& renderContext)
{
    Drawable out;
    out.pIndexBuffer = renderContext.createBufferWithArrayData<uint32_t>(indices, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    out.pVertexBuffer = renderContext.createBufferWithArrayData<ShaderInputs::Vertex>(vertices, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    out.pIndexBuffer->SetName(L"VisualDebug IndexBuffer");
    out.pVertexBuffer->SetName(L"VisualDebug VertexBuffer");
    out.indexBufferView = D3D12_INDEX_BUFFER_VIEW {
        .BufferLocation = out.pIndexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = (unsigned)(indices.size() * sizeof(uint32_t)),
        .Format = DXGI_FORMAT_R32_UINT
    };
    out.vertexBufferView = D3D12_VERTEX_BUFFER_VIEW {
        .BufferLocation = out.pVertexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = (unsigned)(vertices.size() * sizeof(ShaderInputs::Vertex)),
        .StrideInBytes = sizeof(ShaderInputs::Vertex)
    };
    out.numIndices = (uint32_t)indices.size();
    return out;
}

void VisualDebugPass::clearCommandBuffer(RenderContext& renderContext, ID3D12GraphicsCommandList* pCommandList)
{
    auto& descriptorAllocator = renderContext.getCurrentCbvSrvUavDescriptorTransientAllocator();
    const auto commandBufferDescriptor = descriptorAllocator.allocate(1);
    renderContext.pDevice->CreateUnorderedAccessView(m_pCommandBuffer, nullptr, &m_shaderInputs.commandBuffer.desc, commandBufferDescriptor.firstCPUDescriptor);
    descriptorAllocator.flush();

    // Clear command buffer before next frame.
    std::array<UINT, 4> clearValue { 0, 0, 0, 0 };
    pCommandList->ClearUnorderedAccessViewUint(commandBufferDescriptor.firstGPUDescriptor, commandBufferDescriptor.firstCPUDescriptor, m_pCommandBuffer, clearValue.data(), 0, nullptr);
    const auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_pCommandBuffer);
    pCommandList->ResourceBarrier(1, &uavBarrier);
}

void VisualDebugPass::initialize(RenderContext& renderContext, D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc)
{
    const auto vertexShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Debug/visual_debug_vs.dxil");
    const auto pixelShader = Render::loadEngineShader(renderContext.pDevice.Get(), "Engine/Debug/visual_debug_ps.dxil");

    m_pRootSignature = ShaderInputs::DefaultLayout::getRootSignature(renderContext.pDevice.Get());

    pipelineStateDesc.pRootSignature = m_pRootSignature.Get();
    std::array<D3D12_INPUT_ELEMENT_DESC, 6> inputElements;
    setDefaultVertexLayout(pipelineStateDesc, inputElements);
    pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipelineStateDesc.DepthStencilState.DepthEnable = true;
    pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pipelineStateDesc.VS = vertexShader;
    pipelineStateDesc.PS = pixelShader;
    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pPipelineState)));

    std::array<D3D12_INDIRECT_ARGUMENT_DESC, 2> commandArguments;
    commandArguments[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
    commandArguments[0].ConstantBufferView.RootParameterIndex = ShaderInputs::DefaultLayout::getVisualDebugCBVRootParameterIndex();
    commandArguments[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    const D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc {
        .ByteStride = sizeof(DrawCommand),
        .NumArgumentDescs = (UINT)commandArguments.size(),
        .pArgumentDescs = commandArguments.data(),
        .NodeMask = 0
    };
    RenderAPI::ThrowIfFailed(
        renderContext.pDevice->CreateCommandSignature(&commandSignatureDesc, m_pRootSignature.Get(), IID_PPV_ARGS(&m_pCommandSignature)));

    const auto commandBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(CommandStartAddress + MaxNumDrawCommands * sizeof(DrawCommand), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0);
    const auto constantsBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MaxNumDrawCommands * ConstantsSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0);
    m_pCommandBuffer = renderContext.createResource(D3D12_HEAP_TYPE_DEFAULT, commandBufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_pConstantsBuffer = renderContext.createResource(D3D12_HEAP_TYPE_DEFAULT, constantsBufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // m_pCommandBufferReadBack = std::make_unique<DebugBufferReader>(m_pCommandBuffer, commandBufferDesc, renderContext);

    m_shaderInputs.commandBuffer = RenderAPI::UAVDesc {
        .desc = D3D12_UNORDERED_ACCESS_VIEW_DESC {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
            .Buffer = D3D12_BUFFER_UAV {
                .FirstElement = 0,
                .NumElements = (UINT)commandBufferDesc.Width / sizeof(uint32_t),
                .StructureByteStride = 0,
                .CounterOffsetInBytes = 0,
                .Flags = D3D12_BUFFER_UAV_FLAG_RAW } },
        .pResource = m_pCommandBuffer
    };
    m_shaderInputs.constantsBuffer = RenderAPI::UAVDesc {
        .desc = D3D12_UNORDERED_ACCESS_VIEW_DESC {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
            .Buffer = D3D12_BUFFER_UAV {
                .FirstElement = 0,
                .NumElements = (UINT)constantsBufferDesc.Width / sizeof(uint32_t),
                .StructureByteStride = 0,
                .CounterOffsetInBytes = 0,
                .Flags = D3D12_BUFFER_UAV_FLAG_RAW } },
        .pResource = m_pConstantsBuffer
    };
    m_shaderInputs.constantsBufferAddress = m_pConstantsBuffer->GetGPUVirtualAddress();

    {
        constexpr uint32_t numRotationSteps = 32;
        constexpr float cylinderWidth = 0.05f;
        constexpr float cylinderHeight = 0.7f;
        constexpr float arrowPointHeight = 1.0f - cylinderHeight;
        constexpr float arrowPointWidth = 0.15f;

        std::vector<uint32_t> indices;
        std::vector<ShaderInputs::Vertex> vertices;
        vertices.push_back({ .pos = glm::vec3(0), .normal = glm::vec3(0, -1, 0) }); // Center of the cylinder base.
        for (uint32_t i = 0; i < numRotationSteps; ++i) {
            const float normalAngle = (i + 0.5f) / float(numRotationSteps) * glm::two_pi<float>();
            const glm::vec3 cylinderNormal { glm::cos(normalAngle), 0, glm::sin(normalAngle) };

            for (int j = 0; j < 2; ++j) {
                const float angle = float(i + j) / float(numRotationSteps) * glm::two_pi<float>();

                // Bottom of the cylinder.
                const glm::vec3 cylinderBasePos { glm::cos(angle) * cylinderWidth, 0, glm::sin(angle) * cylinderWidth };
                const glm::vec3 cylinderBaseNormal { 0, -1, 0 };

                // Top of the cylinder.
                const glm::vec3 cylinderTopPos = cylinderBasePos + glm::vec3(0, cylinderHeight, 0);

                // Bottom of the arrow point.
                const glm::vec3 arrowPointBasePos { glm::cos(angle) * arrowPointWidth, cylinderHeight, glm::sin(angle) * arrowPointWidth };
                const glm::vec3 arrowPointBaseNormal { 0, -1, 0 };

                vertices.push_back({ .pos = cylinderBasePos, .normal = cylinderBaseNormal });
                vertices.push_back({ .pos = cylinderBasePos, .normal = cylinderNormal });
                vertices.push_back({ .pos = cylinderTopPos, .normal = cylinderNormal });
                vertices.push_back({ .pos = cylinderTopPos, .normal = arrowPointBaseNormal });
                vertices.push_back({ .pos = arrowPointBasePos, .normal = arrowPointBaseNormal });
                vertices.push_back({ .pos = arrowPointBasePos });
            }

            constexpr glm::vec3 arrowPointTopPos { 0, 1, 0 };
            const auto arrowPointEdge1 = glm::normalize(vertices[vertices.size() - 1].pos - vertices[vertices.size() - 7].pos);
            const auto arrowPointEdge2 = glm::normalize(arrowPointTopPos - vertices[vertices.size() - 1].pos);
            const auto arrowPointNormal = glm::cross(arrowPointEdge2, arrowPointEdge1);
            vertices[vertices.size() - 1].normal = arrowPointNormal;
            vertices[vertices.size() - 7].normal = arrowPointNormal;
            vertices.push_back({ .pos = arrowPointTopPos, .normal = arrowPointNormal });
            // Top of the arrow point.

            const uint32_t v0 = 1 + 13 * i;

            // Cylinder floor.
            indices.push_back(0);
            indices.push_back(v0 + 6);
            indices.push_back(v0 + 0);

            // Cylinder face.
            indices.push_back(v0 + 1);
            indices.push_back(v0 + 7);
            indices.push_back(v0 + 2);
            indices.push_back(v0 + 2);
            indices.push_back(v0 + 7);
            indices.push_back(v0 + 8);

            // Bottom of the arrow point.
            indices.push_back(v0 + 3);
            indices.push_back(v0 + 10);
            indices.push_back(v0 + 9);
            indices.push_back(v0 + 3);
            indices.push_back(v0 + 4);
            indices.push_back(v0 + 10);

            // Arrow point.
            indices.push_back(v0 + 5);
            indices.push_back(v0 + 11);
            indices.push_back(v0 + 12);
        }

        spdlog::info("Num arrow indices: {}", indices.size());
        m_arrow = createDrawable(indices, vertices, renderContext);
    }
}

}
