#pragma once
#include "Engine/Memory/PoolAllocator.h"
#include <tbx/move_only.h>
#include <cstddef>
#include <span>

namespace Memory {

class FixedSizePoolAllocator : public PoolAllocator {
public:
    FixedSizePoolAllocator(std::span<std::byte> memory, size_t allocationSize);
    NO_COPY(FixedSizePoolAllocator);

    void* allocate() override;
    void deallocate(void* pMemory) override;

private:
    const size_t m_poolSize;

	struct Header {
        Header* pNext;
	};
    Header* m_pHead;
};

}
