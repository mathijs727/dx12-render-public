#include "Engine/RenderAPI/Buffer/CpuBufferRingAllocator.h"
#include <tbx/error_handling.h>

namespace RenderAPI {

CPUBufferRingAllocator::CPUBufferRingAllocator(const WRL::ComPtr<ID3D12Device5>& pDevice, size_t desiredSize, int numFrames)
    : m_pDevice(pDevice)
    , m_size(desiredSize)
    , m_writeOffset(0)
{
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_size, D3D12_RESOURCE_FLAG_NONE);
    ThrowIfFailed(pDevice->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE,
        &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pBuffer)));
    m_pBuffer->SetName(L"CPUBufferRingAllocator");

    const CD3DX12_RANGE range { 0, m_size };
    m_pBuffer->Map(0, &range, reinterpret_cast<void**>(&m_pMappedBuffer));
    m_baseAddress = m_pBuffer->GetGPUVirtualAddress();

#ifndef NDEBUG
    for (int i = 0; i < numFrames; ++i)
        m_markers.push(m_size);
#endif
}

void CPUBufferRingAllocator::newFrame()
{
#ifndef NDEBUG
    m_markers.pop();
    m_markers.push(m_writeOffset);
#endif
}

size_t CPUBufferRingAllocator::allocateInternal(std::span<const std::byte> data, size_t alignment)
{
#ifndef NDEBUG
    //  Check if write might potentially fit (little over conservative to simplify code).
    if (m_writeOffset < m_markers.front())
        Tbx::assert_always(m_writeOffset + alignment + data.size_bytes() < m_markers.front());
#endif

    m_writeOffset = Util::roundUpToClosestMultiple(m_writeOffset, alignment);
    if (m_writeOffset + data.size_bytes() > m_size)
        m_writeOffset = 0;

#ifndef NDEBUG
    //  Check again after wrapping.
    if (m_writeOffset < m_markers.front())
        Tbx::assert_always(m_writeOffset + data.size_bytes() < m_markers.front());
#endif

    const auto out = m_writeOffset;
    std::memcpy(m_pMappedBuffer + out, data.data(), data.size_bytes());
    m_writeOffset += data.size_bytes();
    return out;
}

}
