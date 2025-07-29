#pragma once
#include "disable_all_warnings.h"
#include <exception>
#include <source_location>
#include <type_traits>
DISABLE_WARNINGS_PUSH()
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()

namespace Tbx {

// Use this to static_assert if a constexpr if/else block is reached.
// if constexpr (condition...)
//     ...
// else
//     static_assert(always_false<T>, "error message...");
// https://stackoverflow.com/questions/53945490/how-to-assert-that-a-constexpr-if-else-clause-never-happen
template <class...>
constexpr std::false_type always_false {};

inline void assert_always(bool v, const char* pErrorMessage, std::source_location location = std::source_location::current())
{
    if (!v) {
        spdlog::error("file: {} ({}:{}) {}: {}", location.file_name(), location.line(), location.column(), location.function_name(), pErrorMessage);
        throw std::exception();
    }
}

inline void assert_always(bool v, std::source_location location = std::source_location::current())
{
    assert_always(v, "", location);
}

}