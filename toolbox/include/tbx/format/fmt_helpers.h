#pragma once
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>
DISABLE_WARNINGS_POP()
#include <type_traits>

namespace Tbx {

template <typename T>
static auto to_printable_value(T value)
{
    if constexpr (std::is_enum_v<std::remove_all_extents_t<T>>) {
        auto optIndex = magic_enum::enum_index(value);
        if (optIndex.has_value())
            return (int)optIndex.value();
        else
            return -1;
    } else if constexpr (std::is_pointer_v<T>) {
        return fmt::ptr(value);
    } else {
        return value;
    }
}
}
