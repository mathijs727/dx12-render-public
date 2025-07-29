#include "Engine/Render/RenderPasses/Util/Printf.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/RenderAPI/CommandListManager.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/RenderAPI.h"
DISABLE_WARNINGS_PUSH()
#include <fmt/format.h>
#include <imgui.h>
#include <sstream>
DISABLE_WARNINGS_POP()
#include <cstddef>
#include <cstring> // std::memcpy
#include <tbx/error_handling.h>
#include <tbx/template_meta.h>

namespace Render {

void PrintfPass::initialize(RenderContext& renderContext)
{
    // Create GPU buffer to write to.
    const auto commandBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) + settings.bufferSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0);
    m_pPrintBuffer = renderContext.createResource(D3D12_HEAP_TYPE_DEFAULT, commandBufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    // Create multiple readback buffers so we can have multiple frames in flight.
    auto readBackBufferDesc = commandBufferDesc;
    readBackBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    const auto numParallelFrames = RenderAPI::SwapChain::s_parallelFrames;
    for (int parallelFrameIdx = 0; parallelFrameIdx < numParallelFrames; ++parallelFrameIdx) {
        m_readBackBuffers.emplace_back(renderContext.createResource(D3D12_HEAP_TYPE_READBACK, readBackBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST));
    }

    // Create a UAV description of the GPU buffer.
    m_shaderInputs = ShaderInputs::PrintSink {
        .printBuffer = RenderAPI::UAVDesc {
            .desc = {
                .Format = DXGI_FORMAT_R32_TYPELESS,
                .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                .Buffer = D3D12_BUFFER_UAV {
                    .FirstElement = 0,
                    .NumElements = (UINT)commandBufferDesc.Width / sizeof(uint32_t),
                    .StructureByteStride = 0,
                    .CounterOffsetInBytes = 0,
                    .Flags = D3D12_BUFFER_UAV_FLAG_RAW },
            },
            .pResource = m_pPrintBuffer.Get(),
        },
        .bufferSize = settings.bufferSizeInBytes,
    };
}

enum TypeID {
    typeID_float = 0,
    typeID_uint,
    typeID_int
};
struct CommandHeader {
    uint32_t stringLength;
    uint32_t commandLength; // Includes header.
};

void PrintfPass::execute(const FrameGraphRegistry<PrintfPass>& resources, const FrameGraphExecuteArgs& args)
{
    if (m_paused)
        return;

    auto& currentReadbackBuffer = m_readBackBuffers[m_currentFrameIdx];
    // Print the buffer read to the CPU a couple frames ago.
    {
        D3D12_RANGE readRange { 0, settings.bufferSizeInBytes };
        void* pMappedData;
        currentReadbackBuffer->Map(0, &readRange, &pMappedData);
        uint32_t end;
        std::memcpy(&end, pMappedData, sizeof(uint32_t));
        if (end > settings.bufferSizeInBytes) {
            // pEnd will lie outside the buffer and we cannot accurately determine where the stream ends.
            spdlog::error("DebugPrint buffer overflow");
        } else {
            end = std::min(end, settings.bufferSizeInBytes);
            std::byte const* pCursor = (std::byte const*)pMappedData + 4;
            std::byte const* pEnd = pCursor + end;
            while (pCursor < pEnd) {
                CommandHeader header;
                std::memcpy(&header, pCursor, sizeof(header));

                std::byte const* pFormatCursor = pCursor + sizeof(header);
                std::byte const* pArgumentCursor = pFormatCursor + header.stringLength;
                std::string_view formatString { (const char*)pFormatCursor, header.stringLength };
                const auto parseNextArgument = [&](auto&& f) {
                    const auto read = [&]<typename T>(T& item) {
                        std::memcpy(&item, pArgumentCursor, sizeof(T));
                        pArgumentCursor += sizeof(T);
                    };
                    const auto parse = [&]<typename T>(Tbx::TypeForward<T>) {
                        T out;
                        read(out);
                        f(out);
                    };

                    uint32_t typeID;
                    read(typeID);
                    switch (typeID) {
                    case typeID_float: {
                        parse(Tbx::TypeForward<float>());
                    } break;
                    case typeID_uint: {
                        parse(Tbx::TypeForward<uint32_t>());
                    } break;
                    case typeID_int: {
                        parse(Tbx::TypeForward<int32_t>());
                    } break;
                    };
                };

                // Parse the string and apply format strings.
                std::ostringstream s;
                uint32_t formatCursor = 0;
                while (formatCursor < formatString.size()) {
                    if (formatString[formatCursor] == '{') {
                        uint32_t formatEnd = formatCursor;
                        for (; formatString[formatEnd] != '}'; ++formatEnd)
                            ;

                        const auto format = formatString.substr(formatCursor, formatEnd - formatCursor + 1);
                        parseNextArgument([&]<typename T>(T v) {
                            s << fmt::format(fmt::runtime(format), v);
                        });

                        formatCursor = formatEnd;
                    } else {
                        s << formatString[formatCursor];
                    }
                    ++formatCursor;
                }
                spdlog::info("{}", s.str());

                pCursor += header.commandLength;
            }
            assert(pCursor == pEnd);
        }
        currentReadbackBuffer->Unmap(0, nullptr);
    }

    // Create a UAV descriptor
    auto& descriptorAllocator = args.pRenderContext->getCurrentCbvSrvUavDescriptorTransientAllocator();
    const auto commandBufferDescriptor = descriptorAllocator.allocate(1);
    args.pRenderContext->pDevice->CreateUnorderedAccessView(m_pPrintBuffer, nullptr, &m_shaderInputs.printBuffer.desc, commandBufferDescriptor.firstCPUDescriptor);
    descriptorAllocator.flush();

    // Copy from the GPU to the (CPU) readback buffer.
    const auto toCopySourceBarier = CD3DX12_RESOURCE_BARRIER::Transition(m_pPrintBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    args.pCommandList->ResourceBarrier(1, &toCopySourceBarier);
    args.pCommandList->CopyResource(currentReadbackBuffer.Get(), m_pPrintBuffer.Get());
    const auto toResourceStateBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pPrintBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    args.pCommandList->ResourceBarrier(1, &toResourceStateBarrier);

    // Clear command buffer before next frame.
    std::array<UINT, 4> clearValue { 0, 0, 0, 0 };
    args.pCommandList->ClearUnorderedAccessViewUint(commandBufferDescriptor.firstGPUDescriptor, commandBufferDescriptor.firstCPUDescriptor, m_pPrintBuffer, clearValue.data(), 0, nullptr);
    const auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_pPrintBuffer);
    args.pCommandList->ResourceBarrier(1, &uavBarrier);
    // Go to the next buffer.
    m_currentFrameIdx = (m_currentFrameIdx + 1) % m_readBackBuffers.size();
}

void PrintfPass::displayGUI()
{
    ImGui::Checkbox("Paused", &m_paused);
}

ShaderInputs::PrintSink PrintfPass::getShaderInputs() const
{
    auto out = m_shaderInputs;
    out.paused = m_paused;
    return out;
}

}
