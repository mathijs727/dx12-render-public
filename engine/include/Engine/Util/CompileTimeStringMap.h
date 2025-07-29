#pragma once
#include <array>

namespace Util {

// std::string & std::string_view are not literal types and can thus not be used as a template value parameter.
struct CompileTimeString {
    static constexpr size_t MaxCharCount = 32;

    consteval CompileTimeString()
    {
        for (size_t i = 0; i < chars.size(); i++)
            chars[i] = 0;
    }
    template <size_t N>
    consteval CompileTimeString(const char (&array)[N])
    {
        static_assert(N <= MaxCharCount, "cannot create a compile time string this long");

        for (size_t i = 0; i < chars.size(); i++)
            chars[i] = 0;
        for (size_t i = 0; i < N; i++)
            chars[i] = array[i];
        size = N;
    }

    consteval bool operator==(const CompileTimeString& other) const
    {
        if (size != other.size)
            return false;
        for (size_t i = 0; i < size; i++) {
            if (chars[i] != other.chars[i])
                return false;
        }
        return true;
    }

    std::array<char, MaxCharCount> chars;
    size_t size = 0;
};

template <typename K, typename V, int maxSize = 8>
class CompileTimeMap {
public:
    consteval void append(K key, V value) noexcept
    {
        if (contains(key))
            return;
        keys[size] = key;
        values[size] = value;
        size++;
    }
    [[nodiscard]] consteval int findIndex(K key) const noexcept
    {
        for (int i = 0; i < size; i++) {
            if (keys[i] == key)
                return i;
        }
        return -1;
    }
    [[nodiscard]] consteval V find(K key) const noexcept
    {
        return values[findIndex(key)];
    }

    [[nodiscard]] consteval bool contains(K key) const noexcept
    {
        return findIndex(key) != -1;
    }

    [[nodiscard]] consteval CompileTimeMap<K, V> erased(K key) const noexcept
    {
        const auto idx = findIndex(key);
        auto copy = *this;
        for (size_t i = idx; i < size - 1; i++) {
            copy.keys[i] = copy.keys[i + 1];
            copy.values[i] = copy.values[i + 1];
        }
        --copy.size;
        return copy;
    }

public:
    // Separate arrays for keys/values because using a single array of struct Item { K key; V value; } will make MSVC crash.
    std::array<K, maxSize> keys;
    std::array<V, maxSize> values;
    int size = 0;
};

}
