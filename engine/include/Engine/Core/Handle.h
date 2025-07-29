#pragma once
#include <cassert>
#include <tbx/hashmap_helper.h>
#include <vector>
// For some reason, including wrl/client.h last fixes the following STL implementation error (bug???):
// alignment changed after including header, may be due to missing #pragma pack(pop)
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <wrl/client.h>
DISABLE_WARNINGS_POP()

template <typename T, typename S>
struct StronglyTypedAlias : public T {
    using T::T;
};

namespace Core {

namespace Impl {

    // NOTE(Mathijs): work around compiler bug. Code above causes internal compiler error on MSVC.
    template <typename T, unsigned N>
    struct n_bits {
        static constexpr T value = (~T { 0 }) >> (sizeof(T) * 8 - N);
    };
    template <typename T, unsigned N>
    constexpr T n_bits_v = n_bits<T, N>::value;
}

// https://blog.molecular-matters.com/2013/05/17/adventures-in-data-oriented-design-part-3b-internal-references/
template <typename T, size_t N1, size_t N2, typename T2 = void>
struct Handle {
    static_assert(N1 + N2 == sizeof(T) * 8);
    T index : N1 { Impl::n_bits_v<T, N1> };
    T generation : N2 { Impl::n_bits_v<T, N2> };

    constexpr Handle() = default;
    constexpr Handle(T index, T generation)
        : index(index)
        , generation(generation)
    {
    }

    constexpr explicit operator T() const
    {
        return index | (generation << N1);
    }
    constexpr auto operator<=>(const Handle& other) const = default;
    constexpr T2 operator++(int)
    {
        return T2 { index, generation++ };
    }

    using type = T;
    static constexpr size_t index_bits = N1;
    static constexpr size_t generation_bits = N2;
    static constexpr Handle null()
    {
        // NOTE(Mathijs): default initialized to all 1 bits.
        return Handle {};
    }
};

template <size_t N1, size_t N2>
using Handle32 = Handle<uint32_t, N1, N2>;

template <size_t N1, size_t N2>
using Handle64 = Handle<uint64_t, N1, N2>;

namespace Impl {
    template <typename T>
    struct is_com_ptr : std::false_type {
    };

    template <typename T>
    struct is_com_ptr<Microsoft::WRL::ComPtr<T>> : std::true_type {
    };
    template <typename T>
    constexpr bool is_com_ptr_v = is_com_ptr<T>::value;

    template <typename T>
    struct pointer_or_ref_type {
        using type = T&;
    };
    template <typename T>
    struct pointer_or_ref_type<Microsoft::WRL::ComPtr<T>> {
        using type = T*;
    };
    template <typename T>
    using pointer_or_ref_type_t = typename pointer_or_ref_type<T>::type;

    template <typename T>
    struct const_pointer_or_ref_type {
        using type = const T&;
    };
    template <typename T>
    struct const_pointer_or_ref_type<Microsoft::WRL::ComPtr<T>> {
        using type = const T*;
    };
    template <typename T>
    using const_pointer_or_ref_type_t = typename const_pointer_or_ref_type<T>::type;
}

template <typename T, typename Handle>
class HandleDataArray {
public:
    // addItem(T&&), T&& is a r-value reference and not an universal reference (hence the work-around)!
    Handle addItem(const T& item);
    Handle addItem(T&& item);
    void removeItem(Handle);
    Impl::pointer_or_ref_type_t<T> getItem(Handle);
    Impl::const_pointer_or_ref_type_t<T> getItem(Handle) const;

private:
    template <typename S>
    Handle addItemInternal(S&& item);

protected:
    using handle_type = typename Handle::type;
    struct DataItem {
        T item;
        size_t handleIndex;
    };
    std::vector<DataItem> m_data;
    std::vector<Handle> m_handles;

    static constexpr handle_type FREE_LIST_NULL = ((handle_type)1 << Handle::index_bits) - 1;
    handle_type m_freeListHead { FREE_LIST_NULL };
};

template <typename T, typename Handle>
inline Handle HandleDataArray<T, Handle>::addItem(const T& item)
{
    return addItemInternal(item);
}

template <typename T, typename Handle>
inline Handle HandleDataArray<T, Handle>::addItem(T&& item)
{
    return addItemInternal(std::forward<T>(item));
}

template <typename T, typename Handle>
template <typename S>
inline Handle HandleDataArray<T, Handle>::addItemInternal(S&& item)
{
    auto dataIndex = static_cast<handle_type>(m_data.size());
    m_data.emplace_back(DataItem { std::forward<S>(item), 0 });

    handle_type handleIndex;
    handle_type handleGeneration;
    if (m_freeListHead == FREE_LIST_NULL) {
        handleIndex = static_cast<handle_type>(m_handles.size());
        handleGeneration = 0;
        m_handles.push_back(Handle { dataIndex, handleGeneration });
    } else {
        handleIndex = m_freeListHead;
        handleGeneration = (m_handles[m_freeListHead].generation + 1) & ((1 << Handle::generation_bits) - 1);
        m_freeListHead = m_handles[m_freeListHead].index;
        m_handles[handleIndex] = Handle { dataIndex, handleGeneration };
    }
    m_data.back().handleIndex = handleIndex;

    return Handle { handleIndex, handleGeneration };
}

template <typename T, typename Handle>
inline void HandleDataArray<T, Handle>::removeItem(Handle handle)
{
    assert(m_handles[handle.index].generation == handle.generation);
    auto& freeListHandle = m_handles[handle.index];

    if (freeListHandle.index != m_data.size() - 1) {
        std::swap(m_data[freeListHandle.index], m_data.back());
        m_handles[m_data[freeListHandle.index].handleIndex].index = freeListHandle.index;
    }
    m_data.pop_back();

    freeListHandle.index = m_freeListHead;
    m_freeListHead = handle.index;
}

template <typename T, typename Handle>
inline Impl::pointer_or_ref_type_t<T> HandleDataArray<T, Handle>::getItem(Handle handle)
{
    assert(m_handles[handle.index].generation == handle.generation);

    if constexpr (Impl::is_com_ptr_v<T>)
        return m_data[m_handles[handle.index].index].item.Get();
    else
        return m_data[m_handles[handle.index].index].item;
}

template <typename T, typename Handle>
inline Impl::const_pointer_or_ref_type_t<T> HandleDataArray<T, Handle>::getItem(Handle handle) const
{
    assert(m_handles[handle.index].generation == handle.generation);

    if constexpr (Impl::is_com_ptr_v<T>)
        return m_data[m_handles[handle.index].index].item.Get();
    else
        return m_data[m_handles[handle.index].index].item;
}

template <typename T, size_t N1, size_t N2>
constexpr void hash_handle(std::size_t& seed, const Handle<T, N1, N2>& handle)
{
    Tbx::hash_combine(seed, handle.index);
    Tbx::hash_combine(seed, handle.generation);
}

}

namespace std {

template <typename T, typename S>
struct hash<StronglyTypedAlias<T, S>> {
    size_t operator()(const StronglyTypedAlias<T, S>& s) const
    {
        const T& myRef = static_cast<const T&>(s);
        return std::hash<T> {}(myRef);
    }
};

template <typename T, size_t N1, size_t N2>
struct hash<Core::Handle<T, N1, N2>> {
    size_t operator()(const Core::Handle<T, N1, N2>& s) const
    {
        return std::hash<T> {}(s.index);
    }
};
}
