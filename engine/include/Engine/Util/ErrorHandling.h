#pragma once
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <fmt/format.h>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <iostream>
#include <source_location>
#include <string_view>
#include <tbx/format/fmt_helpers.h>

namespace Util {
namespace Impl {
#if _MSC_VER >= 1929 && !defined(__clang__)
    // Added in Visual Studio 2019 16.10
    using source_location = std::source_location;

#else
    // <source_location> does not work (yet) in Clang due to the lack of consteval.
    // Use this as a work-around.
    struct source_location {
        inline std::uint_least32_t line() { return 0; };
        inline std::uint_least32_t column() { return 0; };
        inline const char* file_name() { return "unknown file"; };
        inline const char* function_name() { return "unknown function"; };

        inline static constexpr source_location current() noexcept
        {
            return source_location {};
        }
    };
#endif
}

template <typename T>
inline void ThrowError(T message, Impl::source_location s = Impl::source_location::current())
{
    spdlog::error("[{}] {}: ({}, {}): {}", s.file_name(), s.function_name(), s.line(), s.column(), message);
    throw std::runtime_error(message);
}

inline void ThrowError(Impl::source_location s = Impl::source_location::current())
{
    ThrowError("unknown error", s);
}

template <typename Lhs, typename Rhs>
void AssertLT(Lhs lhs, Rhs rhs, Impl::source_location s = Impl::source_location::current())
{
    if (lhs >= rhs)
        ThrowError(fmt::format("{} >= {}", Tbx::to_printable_value(lhs), Tbx::to_printable_value(rhs)), s);
}

template <typename Lhs, typename Rhs>
void AssertLE(Lhs lhs, Rhs rhs, Impl::source_location s = Impl::source_location::current())
{
    if (lhs > rhs)
        ThrowError(fmt::format("{} > {}", Tbx::to_printable_value(lhs), Tbx::to_printable_value(rhs)), s);
}

template <typename Lhs, typename Rhs>
void AssertGT(Lhs lhs, Rhs rhs, Impl::source_location s = Impl::source_location::current())
{
    if (lhs <= rhs)
        ThrowError(fmt::format("{} <= {}", Tbx::to_printable_value(lhs), Tbx::to_printable_value(rhs)), s);
}

template <typename Lhs, typename Rhs>
void AssertGE(Lhs lhs, Rhs rhs, Impl::source_location s = Impl::source_location::current())
{
    if (lhs < rhs)
        ThrowError(fmt::format("{} < {}", Tbx::to_printable_value(lhs), Tbx::to_printable_value(rhs)), s);
}

template <typename Lhs, typename Rhs>
void AssertEQ(Lhs lhs, Rhs rhs, Impl::source_location s = Impl::source_location::current())
{
    if (lhs != rhs)
        ThrowError(fmt::format("{} != {}", Tbx::to_printable_value(lhs), Tbx::to_printable_value(rhs)), s);
}

template <typename Lhs, typename Rhs>
void AssertNE(Lhs lhs, Rhs rhs, Impl::source_location s = Impl::source_location::current())
{
    if (lhs == rhs)
        ThrowError(fmt::format("{} == {}", Tbx::to_printable_value(lhs), Tbx::to_printable_value(rhs)), s);
}

template <typename Lhs>
void Assert(Lhs lhs, Impl::source_location s = Impl::source_location::current())
{
    if (!lhs)
        ThrowError(fmt::format("!{}", Tbx::to_printable_value(lhs)), s);
}

template <typename Lhs>
void AssertTrue(Lhs lhs, Impl::source_location s = Impl::source_location::current())
{
    Assert(lhs, s);
}

template <typename Lhs>
void AssertFalse(Lhs lhs, Impl::source_location s = Impl::source_location::current())
{
    if (lhs) {
        ThrowError(fmt::format("{}", Tbx::to_printable_value(lhs)), s);
    }
}

}

#define SWITCH_FAIL_DEFAULT                                \
    default: {                                             \
        Util::ThrowError("unhandled switch case");         \
        throw std::runtime_error("unhandled switch case"); \
    } break;
