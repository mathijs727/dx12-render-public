#include <catch2/catch_all.hpp>
#include <tbx/template_meta.h>
#include <tuple>

using namespace Catch::literals;

TEST_CASE("Tbx:: ::contains_v", "[Tbx]")
{
    using namespace Tbx;
    STATIC_REQUIRE(contains_v<int, TypeForward<int, double>>);
    STATIC_REQUIRE(contains_v<double, TypeForward<int, double>>);
    STATIC_REQUIRE(!contains_v<float, TypeForward<int, double>>);
    STATIC_REQUIRE(contains_v<int, std::tuple<int, double>>);
    STATIC_REQUIRE(contains_v<double, std::tuple<int, double>>);
    STATIC_REQUIRE(!contains_v<float, std::tuple<int, double>>);
    STATIC_REQUIRE(!contains_v<int, TypeForward<>>);
    STATIC_REQUIRE(!contains_v<double, TypeForward<>>);
    STATIC_REQUIRE(!contains_v<float, TypeForward<>>);
    STATIC_REQUIRE(!contains_v<int, std::tuple<>>);
    STATIC_REQUIRE(!contains_v<double, std::tuple<>>);
    STATIC_REQUIRE(!contains_v<float, std::tuple<>>);
}

TEST_CASE("Tbx:: ::repeat_tuple_t", "[Tbx]")
{
    using namespace Tbx;
    using TupleType = repeat_tuple_t<int, 3>;
    STATIC_REQUIRE(std::is_same_v<TupleType, std::tuple<int, int, int>>);

    using TupleType2 = repeat_tuple_t<double, 5>;
    STATIC_REQUIRE(std::is_same_v<TupleType2, std::tuple<double, double, double, double, double>>);

    using TupleType3 = repeat_tuple_t<std::string, 0>;
    STATIC_REQUIRE(std::is_same_v<TupleType3, std::tuple<>>);
}

TEST_CASE("Tbx:: ::array_as_tuple", "[Tbx]")
{
    using namespace Tbx;
    std::array arr { 1, 2, 3, 4, 5 };
    auto tuple = array_as_tuple(arr);

    REQUIRE(std::is_same_v<decltype(tuple), std::tuple<int, int, int, int, int>>);
    REQUIRE(std::get<0>(tuple) == 1);
    REQUIRE(std::get<1>(tuple) == 2);
    REQUIRE(std::get<2>(tuple) == 3);
    REQUIRE(std::get<3>(tuple) == 4);
    REQUIRE(std::get<4>(tuple) == 5);
}
