#include <catch2/catch_all.hpp>
#include <fmt/format.h>
#include <tbx/format/fmt_glm.h>

using namespace Catch::literals;

TEST_CASE("Tbx:: ::format glm", "[Tbx]")
{
    glm::vec2 v2(1.0f, 2.0f);
    glm::vec3 v3(1.0f, 2.0f, 3.0f);
    glm::vec4 v4(1.0f, 2.0f, 3.0f, 4.0f);
    REQUIRE(fmt::format("{}", v2) == "(1, 2)");
    REQUIRE(fmt::format("{}", v3) == "(1, 2, 3)");
    REQUIRE(fmt::format("{}", v4) == "(1, 2, 3, 4)");
}