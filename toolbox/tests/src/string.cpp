#include <catch2/catch_all.hpp>
#include <tbx/string.h>

TEST_CASE("Tbx:: ::toLower", "[Tbx]")
{
    REQUIRE(Tbx::toLower("HeLLo WorLD") == "hello world");
}

TEST_CASE("Tbx:: ::toUpper", "[Tbx]")
{
    REQUIRE(Tbx::toUpper("HeLLo WorLD") == "HELLO WORLD");
}
