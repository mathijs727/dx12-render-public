#include "pch.h"
#include <Engine/Util/ErrorHandling.h>

using namespace Catch::literals;

TEST_CASE("Util::ThrowError::ThrowError", "[Util]")
{
    auto logLevel = spdlog::get_level(); // Get the current logging level
    spdlog::set_level(spdlog::level::off); // Disable logging for this test

    SECTION("with message")
    {
        REQUIRE_THROWS_WITH(Util::ThrowError("Test error"), "Test error");
    }
    SECTION("without message")
    {
        REQUIRE_THROWS(Util::ThrowError());
    }

    spdlog::set_level(logLevel); // Restore the original logging level
}

TEST_CASE("Util::Assertions::Assertions", "[Util]")
{
    auto logLevel = spdlog::get_level(); // Get the current logging level
    spdlog::set_level(spdlog::level::off); // Disable logging for this test

    SECTION("AssertLT")
    {
        REQUIRE_NOTHROW(Util::AssertLT(5, 10));
        REQUIRE_THROWS(Util::AssertLT(10, 5));
    }
    SECTION("AssertLE")
    {
        REQUIRE_NOTHROW(Util::AssertLE(5, 10));
        REQUIRE_NOTHROW(Util::AssertLE(10, 10));
        REQUIRE_THROWS(Util::AssertLE(10, 5));
    }
    SECTION("AssertGT")
    {
        REQUIRE_NOTHROW(Util::AssertGT(10, 5));
        REQUIRE_THROWS(Util::AssertGT(5, 10));
    }
    SECTION("AssertGE")
    {
        REQUIRE_NOTHROW(Util::AssertGE(10, 5));
        REQUIRE_NOTHROW(Util::AssertGE(10, 10));
        REQUIRE_THROWS(Util::AssertGE(5, 10));
    }
    SECTION("AssertEQ")
    {
        REQUIRE_NOTHROW(Util::AssertEQ(10, 10));
        REQUIRE_THROWS(Util::AssertEQ(10, 5));
    }
    SECTION("AssertNE")
    {
        REQUIRE_NOTHROW(Util::AssertNE(10, 5));
        REQUIRE_THROWS(Util::AssertNE(10, 10));
    }
    SECTION("AssertTrue")
    {
        REQUIRE_NOTHROW(Util::AssertTrue(true));
        REQUIRE_THROWS(Util::AssertTrue(false));
    }
    SECTION("AssertFalse")
    {
        REQUIRE_NOTHROW(Util::AssertFalse(false));
        REQUIRE_THROWS(Util::AssertFalse(true));
    }
    SECTION("Assert")
    {
        REQUIRE_NOTHROW(Util::Assert(true));
        REQUIRE_THROWS(Util::Assert(false));
    }

    spdlog::set_level(logLevel); // Restore the original logging level
}
