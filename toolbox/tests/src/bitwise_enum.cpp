#include <catch2/catch_all.hpp>
#include <tbx/bitwise_enum.h>

enum class MyEnum : int {
    Value1 = 1,
    Value2 = 2,
    Value4 = 4
};
ENABLE_BITMASK_OPERATORS(MyEnum);

TEST_CASE("Tbx:: ::to_underlying", "[Tbx]")
{
    const MyEnum myEnum1 { MyEnum::Value1 };
    const MyEnum myEnum2 { MyEnum::Value2 };
    const MyEnum myEnum4 { MyEnum::Value4 };

    REQUIRE(Tbx::to_underlying(myEnum1) == 1);
    REQUIRE(Tbx::to_underlying(myEnum2) == 2);
    REQUIRE(Tbx::to_underlying(myEnum4) == 4);
}

TEST_CASE("Tbx:: ::enum", "[Tbx]")
{
    SECTION("operator| on enum class")
    {
        const MyEnum myEnum1 { MyEnum::Value1 };
        const MyEnum myEnum2 { MyEnum::Value2 };
        const MyEnum myEnum4 { MyEnum::Value4 };

        REQUIRE(Tbx::to_underlying(myEnum1 | myEnum2) == 3);
        REQUIRE(Tbx::to_underlying(myEnum1 | myEnum4) == 5);
        REQUIRE(Tbx::to_underlying(myEnum2 | myEnum4) == 6);

        REQUIRE(Tbx::to_underlying(myEnum2 | myEnum1) == 3);
        REQUIRE(Tbx::to_underlying(myEnum4 | myEnum1) == 5);
        REQUIRE(Tbx::to_underlying(myEnum4 | myEnum2) == 6);

        REQUIRE(Tbx::to_underlying(myEnum1 | myEnum2 | myEnum4) == 7);

        MyEnum myEnum12 = myEnum1;
        const MyEnum tmp = (myEnum12 |= myEnum2);
        REQUIRE(Tbx::to_underlying(myEnum12) == 3);
        REQUIRE(Tbx::to_underlying(tmp) == 3);

        MyEnum myEnum24 = myEnum4;
        myEnum24 |= myEnum2;
        REQUIRE(Tbx::to_underlying(myEnum24) == 6);
    }

    SECTION("operator& on enum class")
    {
        const MyEnum myEnum1 { MyEnum::Value1 };
        const MyEnum myEnum2 { MyEnum::Value2 };
        const MyEnum myEnum4 { MyEnum::Value4 };

        REQUIRE(Tbx::to_underlying(myEnum1 & myEnum2) == 0);
        REQUIRE(Tbx::to_underlying(myEnum1 & myEnum4) == 0);
        REQUIRE(Tbx::to_underlying(myEnum2 & myEnum4) == 0);

        REQUIRE(Tbx::to_underlying(myEnum2 & myEnum1) == 0);
        REQUIRE(Tbx::to_underlying(myEnum4 & myEnum1) == 0);
        REQUIRE(Tbx::to_underlying(myEnum4 & myEnum2) == 0);

        REQUIRE(Tbx::to_underlying(myEnum1 & myEnum2 & myEnum4) == 0);

        {
            MyEnum myEnum12 = myEnum1;
            const MyEnum tmp = (myEnum12 &= myEnum2);
            REQUIRE(Tbx::to_underlying(myEnum12) == 0);
            REQUIRE(Tbx::to_underlying(tmp) == 0);
        }

        {
            MyEnum myEnum24 = myEnum4;
            const MyEnum tmp = (myEnum24 &= myEnum2);
            REQUIRE(Tbx::to_underlying(myEnum24) == 0);
            REQUIRE(Tbx::to_underlying(tmp) == 0);
        }

        const MyEnum myEnum12 = myEnum1 | myEnum2;
        REQUIRE(Tbx::to_underlying(myEnum12 & myEnum1) == 1);
        REQUIRE(Tbx::to_underlying(myEnum12 & myEnum2) == 2);
        REQUIRE(Tbx::to_underlying(myEnum12 & myEnum4) == 0);

        const MyEnum myEnum124 = myEnum1 | myEnum2 | myEnum4;
        REQUIRE(Tbx::to_underlying(myEnum124 & myEnum12) == 3);
        REQUIRE(Tbx::to_underlying(myEnum12 & myEnum124) == 3);
    }

    SECTION("operator^ on enum class")
    {
        const MyEnum myEnum1 { MyEnum::Value1 };
        const MyEnum myEnum2 { MyEnum::Value2 };
        const MyEnum myEnum4 { MyEnum::Value4 };

        const MyEnum myEnum12 = myEnum1 | myEnum2;
        REQUIRE(Tbx::to_underlying(myEnum12 ^ myEnum1) == 2);
        REQUIRE(Tbx::to_underlying(myEnum12 ^ myEnum2) == 1);
        REQUIRE(Tbx::to_underlying(myEnum12 ^ myEnum4) == 7);

        REQUIRE(Tbx::to_underlying(myEnum1 ^ myEnum12) == 2);
        REQUIRE(Tbx::to_underlying(myEnum2 ^ myEnum12) == 1);
        REQUIRE(Tbx::to_underlying(myEnum4 ^ myEnum12) == 7);

        {
            MyEnum myEnum12Copy = myEnum12;
            const MyEnum tmp = (myEnum12Copy ^= myEnum1);
            REQUIRE(Tbx::to_underlying(myEnum12Copy) == 2);
            REQUIRE(Tbx::to_underlying(tmp) == 2);
        }

        {
            MyEnum myEnum12Copy = myEnum12;
            const MyEnum tmp = (myEnum12Copy ^= myEnum2);
            REQUIRE(Tbx::to_underlying(myEnum12Copy) == 1);
            REQUIRE(Tbx::to_underlying(tmp) == 1);
        }

        {
            MyEnum myEnum12Copy = myEnum12;
            const MyEnum tmp = (myEnum12Copy ^= myEnum4);
            REQUIRE(Tbx::to_underlying(myEnum12Copy) == 7);
            REQUIRE(Tbx::to_underlying(tmp) == 7);
        }
    }

    SECTION("operator~ on enum class")
    {
        const MyEnum myEnum1 { MyEnum::Value1 };
        const MyEnum myEnum2 { MyEnum::Value2 };
        const MyEnum myEnum4 { MyEnum::Value4 };

        const MyEnum myEnum124 { MyEnum::Value1 | MyEnum::Value2 | MyEnum::Value4 };

        const MyEnum myEnum24 = (~myEnum1) & myEnum124;
        const MyEnum myEnum14 = (~myEnum2) & myEnum124;
        const MyEnum myEnum12 = (~myEnum4) & myEnum124;

        REQUIRE(Tbx::to_underlying(myEnum24) == 6);
        REQUIRE(Tbx::to_underlying(myEnum14) == 5);
        REQUIRE(Tbx::to_underlying(myEnum12) == 3);
    }

    SECTION("operator== on enum class")
    {
        const MyEnum myEnum1 { MyEnum::Value1 };
        const MyEnum myEnum2 { MyEnum::Value2 };
        const MyEnum myEnum4 { MyEnum::Value4 };

        const MyEnum myEnum12 { MyEnum::Value1 | MyEnum::Value2 };
        const MyEnum myEnum14 { MyEnum::Value1 | MyEnum::Value4 };
        const MyEnum myEnum124 { MyEnum::Value1 | MyEnum::Value2 | MyEnum::Value4 };

        REQUIRE(Tbx::to_underlying(myEnum12) == 3);
        REQUIRE(Tbx::to_underlying(myEnum14) == 5);
        REQUIRE(Tbx::to_underlying(myEnum124) == 7);

        REQUIRE(myEnum12 != myEnum1);
        REQUIRE(myEnum12 != myEnum2);
        REQUIRE(myEnum12 != myEnum4);
        REQUIRE(myEnum12 != myEnum14);
        REQUIRE(myEnum12 != myEnum124);

        REQUIRE(myEnum14 != myEnum1);
        REQUIRE(myEnum14 != myEnum2);
        REQUIRE(myEnum14 != myEnum4);
        REQUIRE(myEnum14 != myEnum12);
        REQUIRE(myEnum14 != myEnum124);

        REQUIRE(myEnum124 != myEnum1);
        REQUIRE(myEnum124 != myEnum2);
        REQUIRE(myEnum124 != myEnum4);
        REQUIRE(myEnum124 != myEnum12);
        REQUIRE(myEnum124 != myEnum14);
    }

    SECTION("Test if any bit in enum class is set")
    {
        const MyEnum myEnum0 {};
        const MyEnum myEnum1 { MyEnum::Value1 };
        const MyEnum myEnum2 { MyEnum::Value2 };
        const MyEnum myEnum4 { MyEnum::Value4 };

        const MyEnum myEnum12 { MyEnum::Value1 | MyEnum::Value2 };
        const MyEnum myEnum14 { MyEnum::Value1 | MyEnum::Value4 };
        const MyEnum myEnum124 { MyEnum::Value1 | MyEnum::Value2 | MyEnum::Value4 };

        REQUIRE(Tbx::to_underlying(myEnum12) == 3);
        REQUIRE(Tbx::to_underlying(myEnum14) == 5);
        REQUIRE(Tbx::to_underlying(myEnum124) == 7);

        REQUIRE(!Tbx::any(myEnum0));
        REQUIRE(Tbx::any(myEnum1));
        REQUIRE(Tbx::any(myEnum2));
        REQUIRE(Tbx::any(myEnum4));

        REQUIRE(Tbx::any(myEnum12));
        REQUIRE(Tbx::any(myEnum14));
        REQUIRE(Tbx::any(myEnum124));
    }
}