#pragma once
#include <bit>
#include <cassert>
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec2.hpp>
DISABLE_WARNINGS_POP()

namespace Util {

inline size_t roundUpToClosestMultiple(size_t value, size_t divider)
{
    const size_t r = value % divider;
    return r ? value + (divider - r) : value;
}

inline unsigned roundUpToClosestMultiple(unsigned value, unsigned divider)
{
    const unsigned r = value % divider;
    return r ? value + (divider - r) : value;
}

inline glm::uvec2 roundUpToClosestMultiple(glm::uvec2 value, glm::uvec2 divider)
{
    return glm::uvec2(roundUpToClosestMultiple(value.x, divider.x), roundUpToClosestMultiple(value.y, divider.y));
}

// NOTE: divider MUST be a power of 2!
// https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicconstantbuffer
inline size_t roundUpToClosestMultiplePowerOf2(size_t value, size_t divider)
{
    assert(std::popcount(divider) == 1);
    return (value + divider - 1) & ~(divider - 1);
}

}
