#include "pch.h"
#include <Engine/Util/CompileTimeStringMap.h>

using namespace Catch::literals;

TEST_CASE("Util::CompileTimeString::Member functions", "[Util]")
{
    SECTION("Default constructor")
    {
        constexpr Util::CompileTimeString str;
        REQUIRE(str.size == 0);
    }
    SECTION("Constructor with string literal")
    {
        constexpr Util::CompileTimeString str("Hello, World!");
        STATIC_REQUIRE(str.size == 14);
        STATIC_REQUIRE(str.chars[0] == 'H');
        STATIC_REQUIRE(str.chars[12] == '!');
        STATIC_REQUIRE(str.chars[13] == '\0');
        for (size_t i = 1; i < 12; i++) {
            REQUIRE(str.chars[i] != 0);
        }
    }
    SECTION("Equality operator")
    {
        constexpr Util::CompileTimeString str1("Test");
        constexpr Util::CompileTimeString str2("Test");
        constexpr Util::CompileTimeString str3("Different");
        STATIC_REQUIRE(str1 == str2);
        STATIC_REQUIRE(!(str1 == str3));
    }
}
