#include "Engine/Memory/Memory.h"
#include <cassert>
#include <memory>

namespace Memory {

std::byte* align(std::byte* pMem, size_t alignment)
{
    const size_t dummySize = alignment;
    auto* pMemVoid = reinterpret_cast<void*>(pMem);
    size_t dummySpace = dummySize + alignment;
    void* success = std::align(alignment, dummySize, pMemVoid, dummySpace);
    (void)success;
    assert(success != nullptr);
    return reinterpret_cast<std::byte*>(pMemVoid);
}

}
