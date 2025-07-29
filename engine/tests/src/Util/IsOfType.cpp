#include "pch.h"
#include <Engine/Util/IsOfType.h>

using namespace Catch::literals;

TEST_CASE("Util:: ::is_of_type", "[Util]")
{
    SECTION("std::vector")
    {
        STATIC_REQUIRE(Util::is_std_vector<std::vector<int>>::value);
        STATIC_REQUIRE(!Util::is_std_vector<std::array<int, 5>>::value);
    }
    SECTION("std::array")
    {
        STATIC_REQUIRE(Util::is_std_array<std::array<int, 5>>::value);
        STATIC_REQUIRE(!Util::is_std_array<std::vector<int>>::value);
    }
    SECTION("std::span")
    {
        STATIC_REQUIRE(Util::is_std_span<std::span<int>>::value);
        STATIC_REQUIRE(!Util::is_std_span<std::vector<int>>::value);
    }
    SECTION("std::variant")
    {
        STATIC_REQUIRE(Util::is_std_variant<std::variant<int, float>>::value);
        STATIC_REQUIRE(!Util::is_std_variant<std::vector<int>>::value);
    }
    SECTION("std::optional")
    {
        STATIC_REQUIRE(Util::is_std_optional<std::optional<int>>::value);
        STATIC_REQUIRE(!Util::is_std_optional<std::vector<int>>::value);
    }
    SECTION("std::unique_ptr")
    {
        STATIC_REQUIRE(Util::is_std_unique_ptr<std::unique_ptr<int>>::value);
        STATIC_REQUIRE(!Util::is_std_unique_ptr<std::vector<int>>::value);
    }
}