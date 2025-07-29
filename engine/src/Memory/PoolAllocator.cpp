#include "Engine/Memory/PoolAllocator.h"

Memory::PoolAllocator::PoolAllocator(size_t allocationSize)
    : m_allocationSize(allocationSize)
{
}

size_t Memory::PoolAllocator::allocationSize() const
{
    return m_allocationSize;
}
