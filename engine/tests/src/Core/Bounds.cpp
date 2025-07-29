#include "pch.h"
#include <Engine/Core/Bounds.h>
#include <Engine/Core/Transform.h>

using namespace Catch::literals;

TEST_CASE("Core::Bounds3f::Member functions", "[Core]")
{
    const Core::Bounds3f aabb1 { glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(4.0f) };
    const Core::Bounds3f aabb2 { glm::vec3(-1.0f), glm::vec3(0.5f) };

    SECTION("center")
    {
        REQUIRE(aabb1.center() == glm::vec3(2.5f, 3.0f, 3.5f));
        REQUIRE(aabb2.center() == glm::vec3(-0.25f));
    }

    SECTION("extent")
    {
        REQUIRE(aabb1.extent() == glm::vec3(3.0f, 2.0f, 1.0f));
        REQUIRE(aabb2.extent() == glm::vec3(1.5f));
    }

    SECTION("grow(vec3)")
    {
        auto aabb3 = aabb1;
        aabb3.grow(glm::vec3(3.0f, 5.0f, 1.0f));
        REQUIRE(aabb3.lower == glm::vec3(1.0f, 2.0f, 1.0f));
        REQUIRE(aabb3.upper == glm::vec3(4.0f, 5.0f, 4.0f));
    }

    SECTION("grow(aabb)")
    {
        auto aabb3 = aabb1;
        aabb3.grow(aabb2);
        REQUIRE(aabb3.lower == glm::vec3(-1.0f));
        REQUIRE(aabb3.upper == glm::vec3(4.0f));
    }

    SECTION("operator*(mat4)")
    {
        auto aabb3 = glm::mat4(2.0f) * aabb2;
        REQUIRE(aabb3.lower == glm::vec3(-2.0f));
        REQUIRE(aabb3.upper == glm::vec3(1.0f));
    }
}

TEST_CASE("Core::BoundingSphere::Member functions", "[Core]")
{
    SECTION("operator*(Transform)")
    {
        const Core::Transform transform {
            .position = glm::vec3(3.0f),
            .scale = glm::vec3(1.5f)
        };
        const Core::BoundingSphere sphere {
            .center = glm::vec3(2.0f),
            .radius = 3.0f
        };

        Core::BoundingSphere transformedSphere = transform * sphere;
        REQUIRE(transformedSphere.center == glm::vec3(5.0f));
        REQUIRE(transformedSphere.radius == 4.5_a);
    }
}
