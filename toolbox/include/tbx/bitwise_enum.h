#pragma once
#include <bit>
#include <type_traits>

// http://blog.bitwigglers.org/using-enum-classes-as-type-safe-bitmasks/
//  Type safe bitmask enums

// https://stackoverflow.com/questions/9874960/how-can-i-use-an-enum-class-in-a-boolean-context
// Enum to boolean

#define ENABLE_BITMASK_OPERATORS(x)      \
    namespace Tbx::Impl {                \
    template <>                          \
    struct EnableBitMaskOperators<x> {   \
        static const bool enable = true; \
    };                                   \
    }

namespace Tbx::Impl {
template <typename Enum>
struct EnableBitMaskOperators {
    static const bool enable = false;
};
}

template <typename Enum>
std::enable_if_t<Tbx::Impl::EnableBitMaskOperators<Enum>::enable, Enum>
operator|(Enum lhs, Enum rhs)
{
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<Enum>(
        static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <typename Enum>
std::enable_if_t<Tbx::Impl::EnableBitMaskOperators<Enum>::enable, Enum>
operator&(Enum lhs, Enum rhs)
{
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<Enum>(
        static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <typename Enum>
std::enable_if_t<Tbx::Impl::EnableBitMaskOperators<Enum>::enable, Enum>
operator^(Enum lhs, Enum rhs)
{
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<Enum>(
        static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template <typename Enum>
std::enable_if_t<Tbx::Impl::EnableBitMaskOperators<Enum>::enable, Enum>
operator~(Enum rhs)
{
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<Enum>(
        ~static_cast<underlying>(rhs));
}

template <typename Enum>
std::enable_if_t<Tbx::Impl::EnableBitMaskOperators<Enum>::enable, Enum>
operator|=(Enum& lhs, Enum rhs)
{
    using underlying = std::underlying_type_t<Enum>;
    lhs = static_cast<Enum>(
        static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    return lhs;
}

template <typename Enum>
std::enable_if_t<Tbx::Impl::EnableBitMaskOperators<Enum>::enable, Enum>
operator&=(Enum& lhs, Enum rhs)
{
    using underlying = std::underlying_type_t<Enum>;
    lhs = static_cast<Enum>(
        static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    return lhs;
}

template <typename Enum>
std::enable_if_t<Tbx::Impl::EnableBitMaskOperators<Enum>::enable, Enum>
operator^=(Enum& lhs, Enum rhs)
{
    using underlying = std::underlying_type_t<Enum>;
    lhs = static_cast<Enum>(
        static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
    return lhs;
}

template <typename Enum>
std::enable_if_t<Tbx::Impl::EnableBitMaskOperators<Enum>::enable, bool>
operator==(Enum lhs, Enum rhs)
{
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<underlying>(lhs) == static_cast<underlying>(rhs);
}

namespace Tbx {
template <typename Enum>
std::enable_if_t<Impl::EnableBitMaskOperators<Enum>::enable, bool>
any(Enum rhs)
{
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<underlying>(rhs) != 0;
}

template <typename Enum>
std::enable_if_t<Impl::EnableBitMaskOperators<Enum>::enable, int>
bitnum_count(Enum rhs)
{
    using underlying = std::underlying_type_t<Enum>;
    // NOTE(Mathijs): std::popcount is supposed to work (it's defined in the <bit> header file
    // and Intellisense even forward me to it). However, when compiling MSVC get's confused and
    // says it can't find popcount in namespace std..
    const underlying v = static_cast<underlying>(rhs);
    int bits = 0;
    for (size_t i = 0; i < sizeof(underlying) * 8; i++)
        bits += (v & (1 << i)) ? 1 : 0;
    return bits;
    // return std::popcount(static_cast<unsigned>(static_cast<underlying>(rhs)));
}

// https://stackoverflow.com/questions/8357240/how-to-automatically-convert-strongly-typed-enum-into-int
template <typename E>
static constexpr auto to_underlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

}
