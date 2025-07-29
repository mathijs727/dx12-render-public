#include <catch2/catch_all.hpp>
#include <spdlog/spdlog.h>
#include <tbx/error_handling.h>

TEST_CASE("Tbx:: ::assert_always", "[Tbx]")
{
    auto logLevel = spdlog::get_level(); // Get the current logging level
    spdlog::set_level(spdlog::level::off); // Disable logging for this test

    SECTION("assert_always(true) does not throw")
    {
        CHECK_NOTHROW(Tbx::assert_always(true));
        CHECK_NOTHROW(Tbx::assert_always(true, "error message"));
    }

    SECTION("assert_always(false) does throw")
    {
        CHECK_THROWS(Tbx::assert_always(false));
        CHECK_THROWS(Tbx::assert_always(false, "error message"));
    }

    spdlog::set_level(logLevel); // Restore the original logging level
}
