#pragma once
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>
DISABLE_WARNINGS_POP()
#include <DirectXPackedVector.h>

namespace Util {

template <typename T>
inline constexpr bool isPowerOf2(T n)
{
    // https://stackoverflow.com/questions/108318/whats-the-simplest-way-to-test-whether-a-number-is-a-power-of-2-in-c
    return (n & (n - 1)) == 0;
}

inline DirectX::PackedVector::XMHALF2 toHalf2(const glm::vec2& v)
{
    return DirectX::PackedVector::XMHALF2(glm::value_ptr(v));
}

}
