#pragma once
#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()
#include <limits>
#include <type_traits>

namespace Core {

template <size_t N, typename T>
struct Bounds {
public:
    glm::vec<N, T> lower { std::numeric_limits<T>::max() };
    glm::vec<N, T> upper { std::numeric_limits<T>::lowest() };

public:
    Bounds() = default;
    Bounds(const glm::vec<N, T>& l, const glm::vec<N, T>& u);
    Bounds(const glm::vec<N, T>& u);

    template <typename T2 = T, typename = std::enable_if_t<std::is_same_v<T2, float>>>
    inline glm::vec<N, T2> center() const
    {
        return 0.5f * (upper + lower);
    }
    glm::vec<N, T> extent() const;

    void grow(const glm::vec<N, T>& point);
    void grow(const Bounds& bounds);

    template <typename T2>
    inline operator Bounds<N, T2>() const
    {
        return Bounds<N, T2>(
            glm::vec<N, T2>(lower),
            glm::vec<N, T2>(upper));
    }
};

using Bounds2u = Bounds<2, unsigned>;
using Bounds2f = Bounds<2, float>;
using Bounds3f = Bounds<3, float>;

// TODO(Mathijs): move to own header file or just completely remove and use AABB everywhere?
struct BoundingSphere {
    glm::vec3 center;
    float radius;
};
BoundingSphere operator*(const Core::Transform& transform, const BoundingSphere& bounds);

}

template <typename T>
Core::Bounds<3, T> operator*(const glm::mat4& matrix, const Core::Bounds<3, T>& bounds);
template <typename T>
Core::Bounds<3, T> operator*(const Core::Transform& transform, const Core::Bounds<3, T>& bounds);
