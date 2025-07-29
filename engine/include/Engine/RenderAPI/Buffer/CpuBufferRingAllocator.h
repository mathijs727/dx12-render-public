#pragma once
#include "../Internal/D3D12Includes.h"
#include "Engine/RenderAPI/ShaderInput.h"
#include "Engine/Util/Align.h"
#include "Engine/Util/Math.h"
#include <bit>
#include <cassert>
#include <cstddef>
#include <span>
#include <tbx/move_only.h>
#ifndef NDEBUG
#include <queue>
#endif

namespace RenderAPI {

class CPUBufferRingAllocator {
public:
    CPUBufferRingAllocator(const WRL::ComPtr<ID3D12Device5>& pDevice, size_t desiredSize, int numFrames);
    NO_COPY(CPUBufferRingAllocator);
    DEFAULT_MOVE(CPUBufferRingAllocator);

    inline D3D12_CONSTANT_BUFFER_VIEW_DESC allocateCBV(void const* pData, size_t sizeInBytes)
    {
        return allocateCBV(std::span((std::byte const*)pData, sizeInBytes));
    }
    template <typename T>
    inline D3D12_CONSTANT_BUFFER_VIEW_DESC allocateCBV(const T& data)
    {
        if constexpr (requires(T tmp) { std::span(tmp); })
            return allocateCBV(std::span(data));
        else
            return allocateCBV(std::span(&data, 1));
    }
    template <typename T>
    inline D3D12_CONSTANT_BUFFER_VIEW_DESC allocateCBV(std::span<const T> data)
    {
        const std::span<const std::byte> dataBytes { reinterpret_cast<const std::byte*>(data.data()), data.size_bytes() };
        static_assert(Util::isPowerOf2(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)); // Must be a power of 2.
        const auto alignment = Util::roundUpToClosestMultiplePowerOf2(std::alignment_of_v<T>, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        const size_t offset = allocateInternal(dataBytes, alignment);
        return D3D12_CONSTANT_BUFFER_VIEW_DESC {
            .BufferLocation = m_baseAddress + offset,
            .SizeInBytes = (UINT)Util::roundUpToClosestMultiple(dataBytes.size(), alignment)
        };
    }

    template <typename T>
    inline SRVDesc allocateSRV(const T& data)
    {
        if constexpr (requires(T tmp) { std::span(tmp); })
            return allocateSRV(std::span(data));
        else
            return allocateSRV(std::span(&data, 1));
    }
    template <typename T>
    inline SRVDesc allocateSRV(std::span<const T> data)
    {
        const std::span<const std::byte> dataBytes { reinterpret_cast<const std::byte*>(data.data()), data.size_bytes() };
        [[maybe_unused]] const T* xxx = reinterpret_cast<const T*>(dataBytes.data());
        const size_t offset = allocateInternal(dataBytes, sizeof(T));
        assert(offset % sizeof(T) == 0);

        const D3D12_BUFFER_SRV srvBuffer {
            .FirstElement = offset / sizeof(T),
            .NumElements = (UINT)data.size(),
            .StructureByteStride = sizeof(T),
            .Flags = D3D12_BUFFER_SRV_FLAG_NONE
        };
        const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer = srvBuffer
        };
        return {
            .desc = srvDesc,
            .pResource = m_pBuffer.Get(),
        };
    }

    void newFrame();

private:
    size_t allocateInternal(std::span<const std::byte> data, size_t alignment);

private:
    WRL::ComPtr<ID3D12Device5> m_pDevice;
    WRL::ComPtr<ID3D12Resource> m_pBuffer;
    D3D12_GPU_VIRTUAL_ADDRESS m_baseAddress;
    std::byte* m_pMappedBuffer { nullptr };

    size_t m_size;
    size_t m_writeOffset;
#ifndef NDEBUG
    std::queue<size_t> m_markers;
#endif
};
}
