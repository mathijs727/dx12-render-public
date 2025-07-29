#pragma once
#include "Engine/Memory/Memory.h"
#include <tbx/move_only.h>
#include <deque>
#include <vector>

namespace Memory {

// Unintrusive buddy allocator that uses offsets instead of pointers and is thus unable to use unallocated memory
// for bookkeeping. This is less efficient than a regular (intrusive) allocator but such a unintrusive pointer-less
// allocator can be used for allocation memory that is not accessible by the CPU (such as descriptors & GPU memory).
class UnintrusiveBuddyAllocator {
public:
    UnintrusiveBuddyAllocator(Offset baseOffset, size_t size, size_t minAllocationSize = 64);
    NO_COPY(UnintrusiveBuddyAllocator);
    DEFAULT_MOVE(UnintrusiveBuddyAllocator);

    Offset allocate(size_t size);
    void deallocate(Offset offset);

    void reset();

private:
    bool allocateRecurse(uint32_t nodeIdx, uint32_t depth, size_t offset, size_t size, Offset* pOutOffset);
    void deallocateRecurse(uint32_t nodeIdx, uint32_t depth, Offset currentNodeOffset, Offset offsetToFree);

    struct TreeNode {
        TreeNode(); // Zero initialize

        uint32_t firstChild : 30;
        uint32_t containsData : 1;
        uint32_t hasChildren : 1;
    };

private:
    std::vector<TreeNode> m_binaryTreeMemory;
    std::deque<uint32_t> m_nodePairFreeList;
    uint32_t m_rootIdx;

    size_t m_baseOffset;
    size_t m_size;
    size_t m_minAllocationSize;
};

}
