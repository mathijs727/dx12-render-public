#include "pch.h"
#include <Engine/Memory/Memory.h>

TEST_CASE("Memory:: ::alignSize", "[Memory]")
{
    REQUIRE(Memory::alignSize(1, 4) == 4);
    REQUIRE(Memory::alignSize(2, 4) == 4);
    REQUIRE(Memory::alignSize(3, 4) == 4);
    REQUIRE(Memory::alignSize(4, 4) == 4);
    REQUIRE(Memory::alignSize(5, 4) == 8);
    REQUIRE(Memory::alignSize(12, 8) == 16);
}

TEST_CASE("Memory:: ::align", "[Memory]")
{
    const uintptr_t ptr4 = 4;
    const uintptr_t ptr5 = 5;
    const uintptr_t ptr6 = 6;
    const uintptr_t ptr7 = 7;
    const uintptr_t ptr8 = 8;
    const uintptr_t ptr9 = 9;
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::align(reinterpret_cast<std::byte*>(ptr4), 4)) == 4);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::align(reinterpret_cast<std::byte*>(ptr5), 4)) == 8);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::align(reinterpret_cast<std::byte*>(ptr6), 4)) == 8);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::align(reinterpret_cast<std::byte*>(ptr7), 4)) == 8);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::align(reinterpret_cast<std::byte*>(ptr8), 4)) == 8);
    REQUIRE(reinterpret_cast<uintptr_t>(Memory::align(reinterpret_cast<std::byte*>(ptr9), 4)) == 12);
}

TEST_CASE("Memory:: ::smallestCommonAlignment", "[Memory]")
{
    REQUIRE(Memory::smallestCommonAlignment(2, 4) == 4);
    REQUIRE(Memory::smallestCommonAlignment(8, 4) == 8);
    REQUIRE(Memory::smallestCommonAlignment(12, 4) == 12);
    REQUIRE(Memory::smallestCommonAlignment(12, 8) == 24);
    REQUIRE(Memory::smallestCommonAlignment(16, 8) == 16);
    REQUIRE(Memory::smallestCommonAlignment(5, 9) == 45);
}
