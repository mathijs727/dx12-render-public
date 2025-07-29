#include <array>
#include <catch2/catch_all.hpp>
#include <tbx/hashmap_helper.h>
#include <vector>

TEST_CASE("Tbx:: ::hash_combine", "[Tbx]")
{
    SECTION("changes seed")
    {
        size_t seed = 12345;
        Tbx::hash_combine(seed, 42);
        REQUIRE(seed != 12345);
    }
}

TEST_CASE("Tbx:: ::hash_combine_range", "[Tbx]")
{
    SECTION("changes seed")
    {
        size_t seed = 12345;
        std::vector<int> range { 42 };
        Tbx::hash_combine_range(seed, range);
        REQUIRE(seed != 12345);

        size_t seedLength1 = seed;
        range.push_back(123);
        seed = 12345;
        Tbx::hash_combine_range(seed, range);
        REQUIRE(seed != seedLength1);
    }
}

TEST_CASE("Tbx:: ::compare_ranges", "[Tbx]")
{
    SECTION("equal")
    {
        std::vector<int> range1 { 42, 123 };
        std::vector<int> range2 { 42, 123 };
        REQUIRE(Tbx::compare_ranges(range1, range2) == true);
    }

    SECTION("different length")
    {
        std::vector<int> range1 { 42, 123 };
        std::vector<int> range2 { 42, 123, 123 };
        REQUIRE(Tbx::compare_ranges(range1, range2) == false);
    }

    SECTION("different values")
    {
        std::vector<int> range1 { 42, 123 };
        std::vector<int> range2 { 42, 8 };
        REQUIRE(Tbx::compare_ranges(range1, range2) == false);
    }
}
