#include "pch.h"
#include <Engine/Memory/LinearAllocator.h>
#include <Engine/Memory/Memory.h>
#include <array>
#include <unordered_set>

TEST_CASE("Memory::LinearAllocator::Randomly allocating and deallocating", "[Memory]")
{
    std::vector<std::byte> memory { std::size_t(4 * 256 * 1024 + 16) };
    Memory::LinearAllocator allocator { memory };

    constexpr int numAllocations1 = 128 * 1024;
    constexpr int numDeallocations1 = 32 * 1024;
    constexpr int numAllocations2 = 128 * 1024;
    static_assert(numDeallocations1 < numAllocations1);

    std::mt19937 rng { 7393 };

    std::vector<std::pair<int, int*>> allocations;
    for (int i = 0; i < numAllocations1; i++) {
        int* pValue = Memory::allocate_t<int>(allocator)();
        *pValue = rng();
        allocations.emplace_back(*pValue, pValue);
    }
    std::shuffle(std::begin(allocations), std::begin(allocations), rng);

    SECTION("Deallocate")
    {
        for (int i = 0; i < numDeallocations1; i++) {
            auto [value, pValue] = allocations.back();
            allocations.pop_back();
            REQUIRE(*pValue == value);
            allocator.deallocate(pValue);
        }
    }

    for (int i = 0; i < numAllocations2; i++) {
        int* pValue = Memory::allocate_t<int>(allocator)();
        *pValue = rng();
        allocations.emplace_back(*pValue, pValue);
    }

    SECTION("Verify memory hasn't changed")
    {
        std::for_each(std::begin(allocations), std::end(allocations), [](const std::pair<int, int*> pair) {
            auto [value, pValue] = pair;
            REQUIRE(*pValue == value);
        });
    }
}

TEST_CASE("Memory::LinearAllocator::Pointer is aligned according to alignment argument", "[Memory]")
{
    std::vector<std::byte> memory { std::size_t(16 * 1024) };
    Memory::LinearAllocator allocator { memory };

    // Allocation larger than alignment.
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(12, 4)) % 4 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(12, 8)) % 8 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(48, 16)) % 16 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(48, 32)) % 32 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(86, 64)) % 64 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(154, 128)) % 128 == 0);

    // Allocation smaller than alignment.
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(4, 32)) % 32 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(4, 64)) % 64 == 0);

    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(12, 4)) % 4 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(12, 8)) % 8 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(48, 16)) % 16 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(48, 32)) % 32 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(86, 64)) % 64 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(allocator.allocate(154, 128)) % 128 == 0);
}

template <size_t size, size_t alignment>
struct Allocation {
    alignas(alignment) std::array<std::byte, size> data;
};

TEST_CASE("Memory::LinearAllocator::Pointer is aligned according to types alignment", "[Memory]")
{
    std::vector<std::byte> memory { std::size_t(16 * 1024) };
    Memory::LinearAllocator allocator { memory };

    // Allocation larger than alignment.
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::allocate_t<Allocation<12, 4>>(allocator)()) % 4 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::allocate_t<Allocation<12, 8>>(allocator)()) % 8 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::allocate_t<Allocation<48, 16>>(allocator)()) % 16 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::allocate_t<Allocation<48, 32>>(allocator)()) % 32 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::allocate_t<Allocation<86, 64>>(allocator)()) % 64 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::allocate_t<Allocation<154, 128>>(allocator)()) % 128 == 0);

    // Allocation smaller than alignment.
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::allocate_t<Allocation<4, 32>>(allocator)()) % 32 == 0);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::allocate_t<Allocation<4, 64>>(allocator)()) % 32 == 0);
}
