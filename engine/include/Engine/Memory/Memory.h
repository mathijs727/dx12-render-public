#pragma once
#include <type_traits>
#include <utility>
#include <cstddef>
#include <span>

namespace Memory {

using Offset = size_t;

template <typename T, typename Allocator>
struct allocate_t_helper {
    Allocator& allocator;

    template <typename... Args>
    T* operator()(Args&&... args)
    {
        void* pMem = allocator.allocate(sizeof(T), std::alignment_of_v<T>);
        return new (pMem) T(std::forward<Args>(args)...);
    }
};
template <typename T, typename Allocator>
allocate_t_helper<T, Allocator> allocate_t(Allocator& allocator)
{
    return allocate_t_helper<T, Allocator> { allocator };
}

template <typename T, typename Allocator>
T* allocate_ts(size_t N, Allocator& allocator)
{
    void* pMem = allocator.allocate(sizeof(T) * N, std::alignment_of_v<T>);
    return new (pMem) T[N];
}

template <typename T, typename Allocator>
std::span<T> allocate_ts_span(size_t N, Allocator& allocator)
{
    return std::span(allocate_ts<T, Allocator>(N, allocator), N);
}

std::byte* align(std::byte* pMem, size_t alignment);

constexpr size_t alignSize(size_t size, size_t alignment)
{
    // TODO: I'm pretty sure there is a smarter way to do this (without branch)...
    if (size % alignment == 0)
        return size;
    else
        return size + alignment - size % alignment;
}

// Find an alignment that works for both...
//constexpr size_t smallestCommonAlignment(size_t alignment1, size_t alignment2);
// https://stackoverflow.com/questions/3154454/what-is-the-most-efficient-way-to-calculate-the-least-common-multiple-of-two-int
static constexpr size_t greatestCommonDenominator(size_t a, size_t b)
{
    if (b == 0)
        return a;
    return greatestCommonDenominator(b, a % b);
}
// https://stackoverflow.com/questions/3154454/what-is-the-most-efficient-way-to-calculate-the-least-common-multiple-of-two-int
static constexpr size_t leastCommonDenominator(size_t a, size_t b)
{
    if (a > b)
        return (a / greatestCommonDenominator(a, b)) * b;
    else
        return (b / greatestCommonDenominator(a, b)) * a;
}

constexpr size_t smallestCommonAlignment(size_t alignment1, size_t alignment2)
{
    return leastCommonDenominator(alignment1, alignment2);
}

}
