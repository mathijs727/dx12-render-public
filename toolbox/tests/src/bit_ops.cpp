#include <catch2/catch_all.hpp>
#include <tbx/__bit_ops.h>

TEST_CASE("Tbx:: ::popcount32", "[Tbx]")
{
    REQUIRE(Tbx::popcount32(0b11110000) == 4);
    REQUIRE(Tbx::popcount32(0b10101010) == 4);
    REQUIRE(Tbx::popcount32(0b11001010100) == 5);
}

TEST_CASE("Tbx:: ::popcount64", "[Tbx]")
{
    REQUIRE(Tbx::popcount64(0b11110000) == 4);
    REQUIRE(Tbx::popcount64(0b10101010) == 4);
    REQUIRE(Tbx::popcount64(0b11001010100) == 5);
    REQUIRE(Tbx::popcount64(0xF0F0F0F0F0F0F0F0) == 32);
}

TEST_CASE("Tbx:: ::bitScan32", "[Tbx]")
{
    REQUIRE(Tbx::bitScan32(0b11110000) == 4);
    REQUIRE(Tbx::bitScan32(0b00010000) == 4);
    REQUIRE(Tbx::bitScan32(0b10000000) == 7);
}

TEST_CASE("Tbx:: ::bitScan64", "[Tbx]")
{
    REQUIRE(Tbx::bitScan64(0b11110000) == 4);
    REQUIRE(Tbx::bitScan64(0b00010000) == 4);
    REQUIRE(Tbx::bitScan64(0b10000000) == 7);
    REQUIRE(Tbx::bitScan64(0x000000F000000000) == 36);
}

TEST_CASE("Tbx:: ::bitScanReverse32", "[Tbx]")
{
    REQUIRE(Tbx::bitScanReverse32(0b11110000) == 7);
    REQUIRE(Tbx::bitScanReverse32(0b00010000) == 4);
    REQUIRE(Tbx::bitScanReverse32(0b10000000) == 7);
}

TEST_CASE("Tbx:: ::bitScanReverse64", "[Tbx]")
{
    REQUIRE(Tbx::bitScanReverse64(0b11110000) == 7);
    REQUIRE(Tbx::bitScanReverse64(0b00010000) == 4);
    REQUIRE(Tbx::bitScanReverse64(0b10000000) == 7);
    REQUIRE(Tbx::bitScanReverse64(0x000000F000000000) == 39);
}
