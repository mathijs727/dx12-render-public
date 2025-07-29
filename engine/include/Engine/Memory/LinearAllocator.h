#pragma once
#include "Engine/Memory/ForwardDeclares.h"
#include <tbx/move_only.h>
#include <cstddef> // std::byte
#include <span>

namespace Memory {

class LinearAllocator {
public:
    LinearAllocator(std::span<std::byte> memory);
    LinearAllocator(PoolAllocator* pPoolAllocator);
    LinearAllocator(LinearAllocator&&);
    NO_COPY(LinearAllocator);
    ~LinearAllocator();

    void* allocate(size_t size, size_t alignment = 8);
    void deallocate(void*);

    size_t maxAllocationSize();
    void reset();

private:
    void allocateNewBlock();
    void initializeNewBlock(std::span<std::byte> memory);

private:
    struct BlockHeader {
        BlockHeader* pNext;
        size_t size;
    };

	PoolAllocator* m_pPoolAllocator { nullptr };

    BlockHeader* m_pBlocksHead { nullptr };
    std::byte* m_pCurrent { nullptr };
    size_t m_remainingBytes { 0 };
};

}
