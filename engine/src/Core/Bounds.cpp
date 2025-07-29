#include "Engine/Core/Bounds.h"
#include "Engine/Core/Transform.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtx/component_wise.hpp>
DISABLE_WARNINGS_POP()

namespace Core {

template <size_t N, typename T>
Bounds<N, T>::Bounds(const glm::vec<N, T>& l, const glm::vec<N, T>& u)
    : lower(l)
    , upper(u)
{
}

template <size_t N, typename T>
Bounds<N, T>::Bounds(const glm::vec<N, T>& u)
    : upper(u)
{
}

template <size_t N, typename T>
glm::vec<N, T> Bounds<N, T>::extent() const
{
    return upper - lower;
}

template <size_t N, typename T>
void Bounds<N, T>::grow(const glm::vec<N, T>& point)
{
    upper = glm::max(upper, point);
    lower = glm::min(lower, point);
}

template <size_t N, typename T>
void Bounds<N, T>::grow(const Bounds<N, T>& other)
{
    lower = glm::min(lower, other.lower);
    upper = glm::max(upper, other.upper);
}

BoundingSphere operator*(const Transform& transform, const BoundingSphere& sphere)
{
    return BoundingSphere {
        .center = transform.position + sphere.center,
        .radius = glm::compMax(transform.scale) * sphere.radius
    };
}

}

template <typename T>
Core::Bounds<3, T> operator*(const glm::mat4& matrix, const Core::Bounds<3, T>& bounds)
{
    const auto transformPoint = [&](glm::vec<3, T> p) {
        return glm::vec3(matrix * glm::vec4(p, 1));
    };

    Core::Bounds<3, T> out {};
    out.grow(transformPoint({ bounds.lower.x, bounds.lower.y, bounds.lower.z }));
    out.grow(transformPoint({ bounds.lower.x, bounds.lower.y, bounds.upper.z }));
    out.grow(transformPoint({ bounds.lower.x, bounds.upper.y, bounds.lower.z }));
    out.grow(transformPoint({ bounds.lower.x, bounds.upper.y, bounds.upper.z }));
    out.grow(transformPoint({ bounds.upper.x, bounds.lower.y, bounds.lower.z }));
    out.grow(transformPoint({ bounds.upper.x, bounds.lower.y, bounds.upper.z }));
    out.grow(transformPoint({ bounds.upper.x, bounds.upper.y, bounds.lower.z }));
    out.grow(transformPoint({ bounds.upper.x, bounds.upper.y, bounds.upper.z }));
    return out;
}

template <typename T>
Core::Bounds<3, T> operator*(const Core::Transform& transform, const Core::Bounds<3, T>& bounds)
{
    return transform.matrix() * bounds;
}

template Core::Bounds<3, float> operator*(const glm::mat4& matrix, const Core::Bounds<3, float>& bounds);
template Core::Bounds<3, float> operator*(const Core::Transform& transform, const Core::Bounds<3, float>& bounds);

template struct Core::Bounds<2, unsigned>;
template struct Core::Bounds<2, float>;
template struct Core::Bounds<3, float>;
