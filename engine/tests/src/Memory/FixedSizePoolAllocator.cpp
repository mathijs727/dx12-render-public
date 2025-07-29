#include "pch.h"
#include <Engine/Memory/FixedSizePoolAllocator.h>
#include <Engine/Memory/Memory.h>
#include <unordered_set>

TEST_CASE("Memory::FixedSizePoolAllocator::Randomly allocating and deallocating", "[Memory]")
{
    constexpr size_t allocationSize = 1024;
    std::vector<std::byte> memory { std::size_t(256 * 1024) };
    Memory::FixedSizePoolAllocator allocator { memory, allocationSize };

    constexpr int numAllocations1 = 128;
    constexpr int numDeallocations1 = 32;
    constexpr int numAllocations2 = 128;
    static_assert(numDeallocations1 < numAllocations1);

    std::mt19937 rng { 7393 };

    std::vector<std::pair<int, int*>> allocations;
    for (int i = 0; i < numAllocations1; i++) {
        int* pValues = reinterpret_cast<int*>(allocator.allocate());
        int value = rng();
        std::fill(pValues, pValues + allocationSize / sizeof(int), value);
        allocations.emplace_back(value, pValues);
    }
    std::shuffle(std::begin(allocations), std::begin(allocations), rng);

    SECTION("Deallocate")
    {
        for (int i = 0; i < numDeallocations1; i++) {
            auto [value, pValue] = allocations.back();
            allocations.pop_back();
            for (size_t j = 0; j < allocationSize / sizeof(int); j++)
                REQUIRE(*pValue == value);
            allocator.deallocate(pValue);
        }
    }

    for (int i = 0; i < numAllocations2; i++) {
        int* pValues = reinterpret_cast<int*>(allocator.allocate());
        int value = rng();
        std::fill(pValues, pValues + allocationSize / sizeof(int), value);
        allocations.emplace_back(value, pValues);
    }

    SECTION("Verify memory hasn't changed")
    {
        std::for_each(std::begin(allocations), std::end(allocations), [=](const std::pair<int, int*> pair) {
            auto [value, pValue] = pair;
            for (size_t j = 0; j < allocationSize / sizeof(int); j++)
                REQUIRE(*pValue == value);
        });
    }
}
