#pragma once

namespace Memory {

class PoolAllocator {
public:
    PoolAllocator(size_t allocationSize);
    virtual ~PoolAllocator() = default;

	virtual void* allocate() = 0;
    virtual void deallocate(void* pMemory) = 0;

    virtual size_t allocationSize() const;

protected:
    const size_t m_allocationSize;
};

}
