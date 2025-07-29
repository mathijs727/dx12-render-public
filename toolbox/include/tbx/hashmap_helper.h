#pragma once
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
// Work around bug in VS16.9.0 causing "warning C4103: alignment changed after including header, may be due to missing #pragma pack(pop)" warnings.
#include <EASTL/fixed_vector.h>
#include <filesystem>
#include <glm/vec2.hpp>
DISABLE_WARNINGS_POP()
#include <string>
#include <utility>

namespace Tbx {

// https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
template <class T>
constexpr void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T>
constexpr void hash_combine_range(std::size_t& seed, const T& range)
{
    for (const auto& v : range) {
        hash_combine(seed, v);
    }
}

template <typename T1, typename T2>
bool compare_ranges(T1 lhs, T2 rhs)
{
    if (lhs.size() != rhs.size())
        return false;
    return std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs));
}

}

namespace std {
template <>
struct hash<glm::uvec2> {
    inline size_t operator()(const glm::uvec2& v) const
    {
        size_t seed { 0 };
        Tbx::hash_combine(seed, v.x);
        Tbx::hash_combine(seed, v.y);
        return seed;
    }
};
}

namespace Eastl {
template <typename T, size_t nodeCount, bool bEnableOverflow>
bool operator==(const eastl::fixed_vector<T, nodeCount, bEnableOverflow>& lhs, const eastl::fixed_vector<T, nodeCount, bEnableOverflow>& rhs)
{
    return Tbx::compare_ranges(lhs, rhs);
}
}

#define HASHABLE_FORWARD_DECLARE(Name)            \
    namespace std {                               \
        template <>                               \
        struct hash<Name> {                       \
            size_t operator()(const Name&) const; \
        };                                        \
    }

// Work around for classes that do not reside in the global namespace. The freestanding operator== is only found
// when it is defined in the same namespace as the class itself.
// https://stackoverflow.com/questions/5195512/namespaces-and-operator-resolution
#define HASHABLE_FORWARD_DECLARE_NAMESPACED(Namespace, Name) \
    namespace std {                                          \
        template <>                                          \
        struct hash<Namespace::Name> {                       \
            size_t operator()(const Namespace::Name&) const; \
        };                                                   \
    }
