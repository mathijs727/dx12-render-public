#pragma once
#include "../Internal/D3D12Includes.h"
#include "Engine/Util/Align.h"
#include <cassert>
#include <cstddef>
#include <span>
#include <tbx/move_only.h>

namespace RenderAPI {

template <typename BufferDesc>
class CPUBufferLinearAllocator {
public:
    inline CPUBufferLinearAllocator(ID3D12Device5* pDevice, size_t size, size_t minAlignment = 1)
        : m_size(size)
        , m_minAlignment(minAlignment)
        , m_currentOffset(0)
    {
        const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE);
        ThrowIfFailed(pDevice->CreateCommittedResource(
            &heapProperties, D3D12_HEAP_FLAG_NONE,
            &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pBuffer)));

        const CD3DX12_RANGE range { 0, 0 };
        m_pBuffer->Map(0, &range, reinterpret_cast<void**>(&m_pMappedBuffer));
        m_baseAddress = m_pBuffer->GetGPUVirtualAddress();
    }
    NO_COPY(CPUBufferLinearAllocator);
    DEFAULT_MOVE(CPUBufferLinearAllocator);

    template <typename T>
    inline auto allocate(const T& data) -> std::enable_if_t<std::is_same_v<BufferDesc, D3D12_CONSTANT_BUFFER_VIEW_DESC>, BufferDesc>
    {
        const std::span<const std::byte> dataBytes { reinterpret_cast<const std::byte*>(&data), sizeof(T) };
        const size_t offset = allocateInternal(dataBytes, std::alignment_of_v<T>);
        return D3D12_CONSTANT_BUFFER_VIEW_DESC {
            .BufferLocation = m_baseAddress + offset,
            .SizeInBytes = (UINT)Util::roundUpToClosestMultiple(dataBytes.size(), m_minAlignment)
        };
    }

    inline void reset()
    {
        m_currentOffset = 0;
    }

private:
    inline size_t allocateInternal(std::span<const std::byte> data, size_t desiredAlignment)
    {
        const size_t alignment = Util::roundUpToClosestMultiple(desiredAlignment, m_minAlignment);
        const size_t offset = Util::roundUpToClosestMultiple(m_currentOffset, alignment);
        m_currentOffset = offset + data.size();
        std::memcpy(m_pMappedBuffer + offset, data.data(), data.size());
        return offset;
    }

private:
    WRL::ComPtr<ID3D12Resource> m_pBuffer;
    D3D12_GPU_VIRTUAL_ADDRESS m_baseAddress;
    std::byte* m_pMappedBuffer { nullptr };

    const size_t m_size;
    const size_t m_minAlignment;
    size_t m_currentOffset;
};

}
