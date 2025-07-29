#include "pch.h"
#include <Engine/Util/Align.h>
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtx/io.hpp>
DISABLE_WARNINGS_POP()

using namespace Catch::literals;

TEST_CASE("Util:: ::roundUpToClosestMultiple", "[Util]")
{
    SECTION("size_t")
    {
        REQUIRE(Util::roundUpToClosestMultiple(size_t(45), size_t(15)) == 45);
        REQUIRE(Util::roundUpToClosestMultiple(size_t(46), size_t(15)) == 60);
    }

    SECTION("unsigned")
    {
        REQUIRE(Util::roundUpToClosestMultiple(unsigned(45), unsigned(15)) == 45);
        REQUIRE(Util::roundUpToClosestMultiple(unsigned(46), unsigned(15)) == 60);
    }

    SECTION("glm::uvec2")
    {
        REQUIRE(Util::roundUpToClosestMultiple(glm::uvec2(45, 80), glm::uvec2(15, 20)) == glm::uvec2(45, 80));
        REQUIRE(Util::roundUpToClosestMultiple(glm::uvec2(46, 80), glm::uvec2(15, 20)) == glm::uvec2(60, 80));

        REQUIRE(Util::roundUpToClosestMultiple(glm::uvec2(45, 81), glm::uvec2(15, 20)) == glm::uvec2(45, 100));
        REQUIRE(Util::roundUpToClosestMultiple(glm::uvec2(46, 81), glm::uvec2(15, 20)) == glm::uvec2(60, 100));
    }
}

TEST_CASE("Util:: ::roundUpToClosestMultiplePowerOf2", "[Util]")
{
    // Rounds up to the next power of 2
    REQUIRE(Util::roundUpToClosestMultiplePowerOf2(1, 16) == 16); 
    REQUIRE(Util::roundUpToClosestMultiplePowerOf2(2, 16) == 16);
    REQUIRE(Util::roundUpToClosestMultiplePowerOf2(3, 16) == 16);

    REQUIRE(Util::roundUpToClosestMultiplePowerOf2(32, 16) == 32);
    REQUIRE(Util::roundUpToClosestMultiplePowerOf2(33, 16) == 48);
}