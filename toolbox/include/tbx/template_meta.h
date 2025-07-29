#pragma once
#include <array>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Tbx {

template <typename... Ts>
struct TypeForward {
};
template <auto V>
struct ValueForward {
    static constexpr auto value = V;
};

// https://stackoverflow.com/questions/34099597/check-if-a-type-is-passed-in-variadic-template-parameter-pack
template <typename T, typename... Ts>
struct contains_ts : std::disjunction<std::is_same<T, Ts>...> {
};
template <typename T, typename... Ts>
struct contains {
};
template <typename T, typename... Ts>
struct contains<T, std::tuple<Ts...>> : contains_ts<T, Ts...> {
};
template <typename T, typename... Ts>
struct contains<T, TypeForward<Ts...>> : contains_ts<T, Ts...> {
};
template <typename T, typename S>
constexpr bool contains_v = contains<T, S>::value;

// https://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
template <typename Dependent, size_t Index>
using DependOn = Dependent;

template <typename T, size_t N, typename Indices = std::make_index_sequence<N>>
struct repeat;

template <typename T, size_t N, size_t... Indices>
struct repeat<T, N, std::index_sequence<Indices...>> {
    using type = std::tuple<DependOn<T, Indices>...>;
};

template <typename T, size_t N>
using repeat_tuple_t = typename repeat<T, N>::type;

//namespace {
//
//    template <typename F, typename ArgsTuple, typename... AdditionalArgs>
//    struct TupleToArgumentsHelper {
//        template <size_t... Indices>
//        static auto callFunc(
//            std::index_sequence<Indices...>,
//            const ArgsTuple& args,
//            AdditionalArgs&&... additionalArgs)
//        {
//            return F(std::forward<AdditionalArgs>(additionalArgs)..., std::get<Indices>(args)...);
//        }
//    };
//
//}
//
//template <typename F, typename ArgsTuple, typename... AdditionalArgs>
//auto invoke_unpack_arguments(const ArgsTuple& args, AdditionalArgs&&... additionalArgs)
//{
//    return TupleToArgumentsHelper<F, ArgsTuple>::callFunc(
//        std::make_index_sequence<std::tuple_size_v<ArgsTuple>> {},
//        args,
//        std::forward<AdditionalArgs>(additionalArgs)...);
//}

// https://stackoverflow.com/questions/37029886/how-to-construct-a-tuple-from-an-array
template <typename T, size_t N, size_t... Indices>
auto array_as_tuple(const std::array<T, N>& arr, std::index_sequence<Indices...>)
{
    return std::make_tuple(arr[Indices]...);
}

template <typename T, size_t N>
auto array_as_tuple(const std::array<T, N>& arr)
{
    return array_as_tuple<T, N>(arr, std::make_index_sequence<N> {});
}

}
