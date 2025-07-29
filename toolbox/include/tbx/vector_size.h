#pragma once
#include <type_traits>

namespace Tbx {

template <typename T>
size_t vectorSizeInBytes(const T& vec)
{
    return vec.size() * sizeof(typename std::remove_cvref_t<decltype(vec)>::value_type);
}

}