#include <catch2/catch_all.hpp>
#include <tbx/variant_helper.h>
#include <variant>

TEST_CASE("Tbx:: ::make_visitor", "[Tbx]")
{
    int visited = 0;
    const std::variant<int, float, double> variantInt { 1 };
    std::visit(
        Tbx::make_visitor(
            [&](int) {
                REQUIRE(true);
                ++visited;
            },
            [](float) {
                REQUIRE(false);
            },
            [](double) {
                REQUIRE(false);
            }),
        variantInt);
    REQUIRE(visited == 1);

    const std::variant<int, float, double> variantFloat { 1.0f };
    std::visit(
        Tbx::make_visitor(
            [](int) {
                REQUIRE(false);
            },
            [&](float) {
                REQUIRE(true);
                ++visited;
            },
            [](double) {
                REQUIRE(false);
            }),
        variantFloat);
    REQUIRE(visited == 2);

    const std::variant<int, float, double> variantDouble { 1.0 };
    std::visit(
        Tbx::make_visitor(
            [](int) {
                REQUIRE(false);
            },
            [](float) {
                REQUIRE(false);
            },
            [&](double) {
                REQUIRE(true);
                ++visited;
            }),
        variantDouble);
    REQUIRE(visited == 3);
}
