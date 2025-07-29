#include "Engine/Memory/LinearAllocator.h"
#include "Engine/Memory/PoolAllocator.h"
#include <algorithm>
#include <cassert>
#include <memory>

namespace Memory {

LinearAllocator::LinearAllocator(std::span<std::byte> memory)
{
    initializeNewBlock(memory);
}

LinearAllocator::LinearAllocator(PoolAllocator* pPoolAllocator)
    : m_pPoolAllocator(pPoolAllocator)
{
    // Allocate on first use.
}

LinearAllocator::LinearAllocator(LinearAllocator&& other)
    : m_pPoolAllocator(other.m_pPoolAllocator)
    , m_pBlocksHead(other.m_pBlocksHead)
    , m_pCurrent(other.m_pCurrent)
    , m_remainingBytes(other.m_remainingBytes)
{
    other.m_pPoolAllocator = nullptr;
    other.m_pBlocksHead = nullptr;
    other.m_pCurrent = nullptr;
    other.m_remainingBytes = 0;
}

LinearAllocator::~LinearAllocator()
{
    if (m_pCurrent)
        reset();
}

void* LinearAllocator::allocate(size_t size, size_t alignment)
{
    if (!m_pCurrent)
        allocateNewBlock();

    while (true) {
        if (std::align(alignment, size, reinterpret_cast<void*&>(m_pCurrent), m_remainingBytes)) {
            void* result = m_pCurrent;
            m_pCurrent += size;
            m_remainingBytes -= size;
            return result;
        } else {
            assert(m_pPoolAllocator);
            assert(size < m_pPoolAllocator->allocationSize());
            allocateNewBlock();
        }
    }
}

void LinearAllocator::deallocate(void*)
{
    // noop
}

size_t LinearAllocator::maxAllocationSize()
{
    if (m_pPoolAllocator)
        return m_pPoolAllocator->allocationSize();
    else
        return m_remainingBytes;
}

void LinearAllocator::reset()
{
    if (m_pPoolAllocator) {
        while (m_pBlocksHead) {
            BlockHeader* pNext = m_pBlocksHead->pNext;
            m_pPoolAllocator->deallocate(m_pBlocksHead);
            m_pBlocksHead = pNext;
        }

        m_pCurrent = nullptr;
        m_remainingBytes = 0;
    } else {
        m_remainingBytes = m_pBlocksHead->size;
        assert(!m_pBlocksHead->pNext);
    }
}

void LinearAllocator::allocateNewBlock()
{
    auto* pMemory = reinterpret_cast<std::byte*>(m_pPoolAllocator->allocate());
    std::span<std::byte> memory = std::span(pMemory, m_pPoolAllocator->allocationSize());

    initializeNewBlock(memory);
}

void LinearAllocator::initializeNewBlock(std::span<std::byte> memory)
{
    assert(memory.size() > sizeof(BlockHeader));

    auto* pHeader = new (memory.data()) BlockHeader();
    pHeader->pNext = m_pBlocksHead;
    pHeader->size = memory.size() - sizeof(BlockHeader);
    m_pBlocksHead = pHeader;

    m_pCurrent = memory.data() + sizeof(BlockHeader);
    m_remainingBytes = pHeader->size;
}

/*void LinearAllocator::allocateNewBlock(size_t minBlockSize)
{
    for (auto iter = std::begin(m_recycledBlocks); iter < std::end(m_recycledBlocks); iter++) {
        auto& [pBlock, blockSize] = *iter;
        if (blockSize >= minBlockSize) {
            m_currentBlockSize = blockSize;
            m_pCurrent = pBlock.get();
            m_blocks.emplace_back(std::move(pBlock), blockSize);

            // Swap-erase because we don't care about the order in which blocks may be recycled.
            if (*iter != m_recycledBlocks.back()) {
                std::swap(*iter, m_recycledBlocks.back());
            }
            m_recycledBlocks.pop_back();

            return;
        }
    }

    auto pBlock = std::make_unique<std::byte[]>(minBlockSize);
    m_currentBlockSize = minBlockSize;
    m_pCurrent = pBlock.get();
    m_blocks.emplace_back(std::move(pBlock), minBlockSize);
}*/
}
