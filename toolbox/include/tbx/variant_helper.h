#pragma once
#include <variant>

namespace Tbx {

template <typename T, size_t i = 0>
void setVariantIndex(T& variant, size_t idx)
{
    if constexpr (i < std::variant_size_v<T>) {
        setVariantIndex<T, i + 1>(variant, idx);
        if (idx == i)
            variant.template emplace<i>();
    }
}

// To be used with std::visit & std::variant
// https://bitbashing.io/std-visit.html
template <class... Fs>
struct overload;
template <class F0, class... Frest>
struct overload<F0, Frest...> : F0, overload<Frest...> {
    overload(F0 f0, Frest... rest)
        : F0(f0)
        , overload<Frest...>(rest...)
    {
    }

    using F0::operator();
    using overload<Frest...>::operator();
};
template <class F0>
struct overload<F0> : F0 {
    overload(F0 f0)
        : F0(f0)
    {
    }

    using F0::operator();
};
template <class... Fs>
auto make_visitor(Fs... fs)
{
    return overload<Fs...>(fs...);
}

}
