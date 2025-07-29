#include "Engine/Memory/UnintrusiveBuddyAllocator.h"
#include "Engine/Util/Math.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <cassert>
#include <exception>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()

namespace Memory {

UnintrusiveBuddyAllocator::UnintrusiveBuddyAllocator(Offset baseOffset, size_t size, size_t minAllocationSize)
    : m_binaryTreeMemory()
    , m_rootIdx(0)
    , m_baseOffset(baseOffset)
    , m_size(size)
    , m_minAllocationSize(minAllocationSize)
{
    if (!Util::isPowerOf2(size)) {
        spdlog::error("UnintrusiveBuddyAllocator size not a power of 2");
        throw std::exception {};
    }

    m_binaryTreeMemory.emplace_back();
    m_binaryTreeMemory.emplace_back();
}

Offset UnintrusiveBuddyAllocator::allocate(size_t allocationSize)
{
    Offset ret;
    if (!allocateRecurse(m_rootIdx, 0, 0, allocationSize, &ret)) {
        spdlog::error("UnintrusiveBuddyAllocator allocation failed");
        throw std::exception {};
    }
    return ret;
}

void UnintrusiveBuddyAllocator::deallocate(Offset allocation)
{
    deallocateRecurse(m_rootIdx, 0, 0, allocation);
}

void UnintrusiveBuddyAllocator::reset()
{
    m_binaryTreeMemory.clear();

    m_binaryTreeMemory.emplace_back();
    m_binaryTreeMemory.emplace_back();
	m_rootIdx = 0;
    m_binaryTreeMemory[m_rootIdx].hasChildren = false;
    m_binaryTreeMemory[m_rootIdx].containsData = false;

    m_nodePairFreeList.clear();
}

bool UnintrusiveBuddyAllocator::allocateRecurse(uint32_t nodeIdx, uint32_t depth, size_t offset, size_t allocationSize, Offset* pOutOffset)
{
    auto& node = m_binaryTreeMemory[nodeIdx];
    if (node.containsData)
        return false;

    size_t levelSize = m_size >> depth;
    size_t nextLevelSize = levelSize >> 1;
    if (allocationSize > nextLevelSize || nextLevelSize < m_minAllocationSize) {
        // Cannot subdivide further (next level is smaller than the allocation size).
        if (!node.hasChildren) {
            node.containsData = true;
            *pOutOffset = m_baseOffset + offset;
            return true;
        } else {
            return false;
        }
    } else {
        uint32_t firstChildIdx = node.firstChild;
        if (!node.hasChildren) {
            // Subdivide
            if (!m_nodePairFreeList.empty()) {
                firstChildIdx = m_nodePairFreeList.back();
                m_nodePairFreeList.pop_back();

                m_binaryTreeMemory[firstChildIdx] = TreeNode {};
                m_binaryTreeMemory[static_cast<size_t>(firstChildIdx) + 1] = TreeNode {};
            } else {
                firstChildIdx = static_cast<uint32_t>(m_binaryTreeMemory.size());
            }
            node.firstChild = firstChildIdx;
            node.hasChildren = true;
            m_binaryTreeMemory.emplace_back(); // WARNING: invalidates node reference (may realloc m_binaryTreeMemory)
            m_binaryTreeMemory.emplace_back(); // WARNING: invalidates node reference (may realloc m_binaryTreeMemory)
        }

		// Traverse
        bool ret = false;
        ret |= allocateRecurse(firstChildIdx, depth + 1, offset, allocationSize, pOutOffset);
        if (!ret)
            ret |= allocateRecurse(firstChildIdx + 1, depth + 1, offset + levelSize / 2, allocationSize, pOutOffset);

        // Propagate up the tree that this whole subtree is full.
        if (m_binaryTreeMemory[firstChildIdx].containsData && m_binaryTreeMemory[static_cast<size_t>(firstChildIdx) + 1].containsData)
            node.containsData = true;

        return ret;
    }
}

void UnintrusiveBuddyAllocator::deallocateRecurse(uint32_t nodeIdx, uint32_t depth, Offset currentNodeOffset, Offset offsetToFree)
{
    auto& node = m_binaryTreeMemory[nodeIdx];
    size_t levelSize = m_size >> depth;

    if (node.hasChildren) {
        if (offsetToFree < currentNodeOffset + levelSize / 2) {
            deallocateRecurse(node.firstChild, depth + 1, currentNodeOffset, offsetToFree);
        } else {
            deallocateRecurse(node.firstChild + 1, depth + 1, currentNodeOffset + levelSize / 2, offsetToFree);
        }

        auto& leftChild = m_binaryTreeMemory[node.firstChild];
        auto& rightChild = m_binaryTreeMemory[static_cast<size_t>(node.firstChild) + 1];
        if (!leftChild.containsData && !leftChild.hasChildren && !rightChild.containsData && !rightChild.hasChildren) {
            // Both children are empty => collapse.
            // List represents pairs of unused nodes so only push the first one, the second child is implicit.
            m_nodePairFreeList.push_back(node.firstChild);
            node.firstChild = 0;
            node.hasChildren = false;
        }
    } else {
        // This must be the node that represents the allocation.
        assert(currentNodeOffset == offsetToFree);
        assert(node.containsData);
        assert(!node.hasChildren);
    }

    node.containsData = false;
}

UnintrusiveBuddyAllocator::TreeNode::TreeNode()
    : firstChild(0)
    , containsData(0)
    , hasChildren(0)
{
}

}
