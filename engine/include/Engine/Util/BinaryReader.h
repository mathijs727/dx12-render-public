#pragma once
#include "Engine/Util/ErrorHandling.h"
#include <cassert>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <optional>
#include <tbx/disable_all_warnings.h>
#include <tbx/error_handling.h>
#include <vector>
DISABLE_WARNINGS_PUSH()
// Needs to be included before the lines below to work around bug in STL/MSVC since 16.9.0
// C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.28.29910\include\optional(35): error C2131: expression did not evaluate to a constant
// C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.28.29910\include\optional(35): note: failure was caused by call of undefined function or one not declared 'constexpr'
// C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.28.29910\include\optional(35): note: see usage of 'std::nullopt_t::nullopt_t'
#include <glm/gtc/type_ptr.hpp>
DISABLE_WARNINGS_POP()
#include "IsOfType.h"

namespace Util {

class BinaryReader;
template <typename T>
concept has_read_from = requires(T& item, BinaryReader& reader) {
    {
        item.readFrom(reader)
    }
    -> std::same_as<void>;
};

class BinaryReader {
public:
    BinaryReader(const std::filesystem::path& filePath);

    template <typename T>
    void read(T& dst);
    template <typename T>
    void read(std::optional<T>& dst);

    template <typename T>
    T read();
    std::vector<std::byte> readBytes(size_t numBytes);

private:
    template <typename T>
    void readVariant(T& dst);
    template <typename T, size_t curIdx = 0>
    void readVariantIdx(T& dst, size_t dstIdx);

private:
    std::ifstream m_fileStream;
};

inline BinaryReader::BinaryReader(const std::filesystem::path& filePath)
    : m_fileStream(filePath, std::ios::binary)
{
    Assert(std::filesystem::exists(filePath));
    Assert((bool)m_fileStream);
}

template <typename T>
inline void BinaryReader::read(T& dst)
{
    dst = read<T>();
}

template <typename T>
inline void BinaryReader::read(std::optional<T>& dst)
{
    const bool containsValue = read<bool>();
    if (containsValue)
        dst.emplace(read<T>());
}

template <typename T>
inline T BinaryReader::read()
{
    if constexpr (has_read_from<T>) {
        T dst {};
        dst.readFrom(*this);
        return dst;
    } else if constexpr (is_std_vector<T>::value) {
        using ItemT = typename T::value_type;
        const size_t vectorLength = read<size_t>();

        T dst;
        if constexpr (std::is_trivially_copyable_v<ItemT>) {
            dst.resize(vectorLength);
            m_fileStream.read(reinterpret_cast<char*>(dst.data()), vectorLength * sizeof(ItemT));
        } else {
            for (size_t i = 0; i < vectorLength; i++)
                dst.push_back(read<ItemT>());
        }
        return dst;
    } else if constexpr (is_std_array<T>::value) {
        using ItemT = typename T::value_type;

        T dst;
        if constexpr (std::is_trivially_copyable_v<ItemT>) {
            m_fileStream.read(reinterpret_cast<char*>(dst.data()), dst.size() * sizeof(ItemT));
        } else {
            for (size_t i = 0; i < dst.size(); i++)
                dst[i] = read<ItemT>();
        }
        return dst;
    } else if constexpr (is_std_variant<T>::value) {
        T dst;
        readVariant(dst);
        return dst;
    } else if constexpr (is_std_optional_v<T>) {
        if (read<bool>())
            return read<std_optional_type_t<T>>();
        else
            return {};
    } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
        return std::filesystem::path(read<std::string>());
    } else if constexpr (std::is_same_v<T, std::string>) {
        const size_t stringLength = read<size_t>();

        std::string dst;
        dst.resize(stringLength);
        m_fileStream.read(dst.data(), dst.size());
        return dst;
    } else if constexpr (std::is_trivially_copyable_v<T>) {
        T dst {};
        m_fileStream.read(reinterpret_cast<char*>(&dst), sizeof(T));
        return dst;
    } else {
        static_assert(Tbx::always_false<T>, "Type does not support deserialization.");
    }
}

inline std::vector<std::byte> BinaryReader::readBytes(size_t numBytes)
{
    std::vector<std::byte> out;
    out.resize(numBytes);
    m_fileStream.read(reinterpret_cast<char*>(out.data()), out.size());
    return out;
}

template <typename T>
void BinaryReader::readVariant(T& dst)
{
    size_t index;
    read(index);

    readVariantIdx<T, 0>(dst, index);
}

template <typename T, size_t curIdx>
void BinaryReader::readVariantIdx(T& dst, size_t dstIdx)
{
    if (curIdx == dstIdx)
        read(std::get<curIdx>(dst));

    if constexpr (curIdx + 1 < std::variant_size_v<T>)
        readVariantIdx<T, curIdx + 1>(dst, dstIdx);
}

}
