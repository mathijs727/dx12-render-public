#pragma once
#include <EASTL/fixed_vector.h>
#include <span>

namespace Tbx {

template <typename T, size_t nodeCount, bool bEnableOverflow, typename OverflowAllocator>
std::span<const T> eastl_to_span(const eastl::fixed_vector<T, nodeCount, bEnableOverflow, OverflowAllocator>& vec)
{
    return std::span(vec.data(), vec.size());
}

template <typename T, size_t nodeCount, bool bEnableOverflow, typename OverflowAllocator>
std::span<T> eastl_to_span(eastl::fixed_vector<T, nodeCount, bEnableOverflow, OverflowAllocator>& vec)
{
    return std::span(vec.data(), vec.size());
}

/*template <typename T>
auto eastl_to_span(const T& vec)
{
    return std::span(vec.data(), vec.size());
}*/

}