#pragma once
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/MaResource.h"
#include <cstring>
#include <tbx/error_handling.h>
#include <vector>

namespace Render {

class DebugBufferReader {
public:
    DebugBufferReader(ID3D12Resource* pResource, const D3D12_RESOURCE_DESC& resourceDesc, RenderContext& renderContext);

    template <typename T>
    std::vector<T> readBackNextFrame(ID3D12GraphicsCommandList* pCommandList, size_t sizeInItems, D3D12_RESOURCE_STATES resourceState)
    {
        const auto& pCurrentReadBackBuffer = m_readBackBuffers[m_currentFrameIdx];
        std::vector<T> out(sizeInItems);

        // Copy to std::vector<>
        const auto sizeInBytes = sizeInItems * sizeof(T);
        Tbx::assert_always(sizeInBytes <= bufferSize);
        D3D12_RANGE readRange { 0, sizeInBytes };
        void* pMappedData;
        pCurrentReadBackBuffer->Map(0, &readRange, &pMappedData);
        std::memcpy(out.data(), pMappedData, sizeInBytes);
        pCurrentReadBackBuffer->Unmap(0, nullptr);

        // Enqueue next copy operation.
        const auto fromBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pResource, resourceState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        const auto toBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pResource, D3D12_RESOURCE_STATE_COPY_SOURCE, resourceState);
        pCommandList->ResourceBarrier(1, &fromBarrier);
        pCommandList->CopyResource(pCurrentReadBackBuffer, m_pResource);
        m_currentFrameIdx = (m_currentFrameIdx + 1) % m_readBackBuffers.size();
        pCommandList->ResourceBarrier(1, &toBarrier);

        return out;
    }

private:
    uint32_t m_currentFrameIdx { 0 };
    size_t bufferSize;
    std::vector<RenderAPI::D3D12MAResource> m_readBackBuffers;
    ID3D12Resource* m_pResource;
};

}
