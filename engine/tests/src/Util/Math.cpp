#include "pch.h"
#include <Engine/Util/Math.h>

using namespace Catch::literals;

TEST_CASE("Util:: ::isPowerOf2", "[Util]")
{
    SECTION("Power of 2")
    {
        STATIC_REQUIRE(Util::isPowerOf2(1));
        STATIC_REQUIRE(Util::isPowerOf2(2));
        STATIC_REQUIRE(Util::isPowerOf2(4));
        STATIC_REQUIRE(Util::isPowerOf2(8));
        STATIC_REQUIRE(Util::isPowerOf2(16));
        STATIC_REQUIRE(Util::isPowerOf2(32));
        STATIC_REQUIRE(Util::isPowerOf2(64));
        STATIC_REQUIRE(Util::isPowerOf2(128));
    }
    SECTION("Not a power of 2")
    {
        STATIC_REQUIRE(!Util::isPowerOf2(3));
        STATIC_REQUIRE(!Util::isPowerOf2(5));
        STATIC_REQUIRE(!Util::isPowerOf2(6));
        STATIC_REQUIRE(!Util::isPowerOf2(7));
        STATIC_REQUIRE(!Util::isPowerOf2(9));
        STATIC_REQUIRE(!Util::isPowerOf2(10));
    }
}