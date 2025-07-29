#include "Engine/Memory/FixedSizePoolAllocator.h"
#include <cassert>

namespace Memory {

FixedSizePoolAllocator::FixedSizePoolAllocator(std::span<std::byte> memory, size_t allocationSize)
    : PoolAllocator(allocationSize)
    , m_poolSize(static_cast<size_t>(memory.size()))
{
    assert(allocationSize > sizeof(std::byte*));

    std::byte* pMemory = memory.data();
    for (size_t i = 0; i < m_poolSize; i += allocationSize) {
        auto* pHeader = reinterpret_cast<Header*>(pMemory + i);
        pHeader->pNext = reinterpret_cast<Header*>(pMemory + i + allocationSize);

        if (i + allocationSize >= m_poolSize)
            pHeader->pNext = nullptr;
    }

    m_pHead = reinterpret_cast<Header*>(pMemory);
}

void* FixedSizePoolAllocator::allocate()
{
    Header* pHeader = m_pHead;
    m_pHead = pHeader->pNext;
    return reinterpret_cast<void*>(pHeader);
}

void FixedSizePoolAllocator::deallocate(void* pMemory)
{
    auto* pHeader = reinterpret_cast<Header*>(pMemory);
    pHeader->pNext = m_pHead;
    m_pHead = pHeader;
}

}
