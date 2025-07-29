#include "pch.h"
#include <Engine/Memory/UnintrusiveBuddyAllocator.h>
#include <random>

TEST_CASE("Memory::UnintrusiveBuddyAllocator::Randomly allocating and deallocating", "[Memory]")
{
    const size_t poolSize = 64 * 1024;
    Memory::UnintrusiveBuddyAllocator allocator { 0, poolSize };

    std::vector<size_t> pool;
    pool.resize(poolSize);
    std::fill(std::begin(pool), std::end(pool), 0);

    struct Allocation {
        size_t offset;
        size_t size;
        size_t storedValue;
    };
    std::vector<Allocation> allocations;
    size_t totalAllocatedSize { 0 };

    std::default_random_engine generator;
    std::uniform_int_distribution<int> allocOrDeallocDistribution(0, 1);
    std::uniform_int_distribution<size_t> allocationSizeDistribution(16, 128);

    auto allocate = [&]() {
        size_t allocationSize = allocationSizeDistribution(generator);

        size_t offset = allocator.allocate(allocationSize);
        std::fill(std::begin(pool) + offset, std::begin(pool) + offset + allocationSize, offset);
        for (size_t i = offset; i < offset + allocationSize; i++) {
            REQUIRE(pool[i] == offset);
        }
        allocations.push_back({ offset, allocationSize, offset });
        return allocationSize;
    };

    // Initially do a bunch of allocations.
    while (totalAllocatedSize < poolSize / 256) {
        totalAllocatedSize += allocate();
    }

    std::mt19937 rng { 7393 };
    std::shuffle(std::begin(allocations), std::end(allocations), rng);

    // Randomly allocate / deallocate. Everytime we deallocate make sure that the value stored is correct.
    for (int i = 0; i < 10000; i++) {
        // Always deallocate when the pool is full.
        if (!allocations.empty() && (totalAllocatedSize > poolSize / 2 || allocOrDeallocDistribution(generator) == 0)) {
            const auto [offset, size, expectedValue] = allocations.back();
            allocations.pop_back();

            for (size_t j = offset; j < offset + size; j++) {
                REQUIRE(pool[j] == expectedValue);
            }

            allocator.deallocate(offset);
            totalAllocatedSize -= size;
        } else {
            totalAllocatedSize += allocate();
        }
    }
}
