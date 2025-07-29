#include "Engine/Util/string.hlsl"

template <typename T, uint32_t N>
uint32_t strlen(T str[N]) {
    return N;
}

enum TypeID {
    typeID_float = 0,
    typeID_uint,
    typeID_int
};
struct CommandHeader {
    uint32_t stringLength;
    uint32_t commandLength; // Includes header.
};

static const uint32_t StreamSizeAddress = 0;
static const uint32_t StreamStartAddress = sizeof(uint32_t);

// Core inspired by/taken from:
// https://therealmjp.github.io/posts/hlsl-printf/#setting-up-the-print-buffer
template <typename T>
void __printf_append_arg_with_code(inout RWByteAddressBuffer buffer, inout uint32_t cursor, in const TypeID typeID, in const T value) {
    uint32_t u32_typeID = typeID;
    buffer.Store(cursor, u32_typeID);
    cursor += sizeof(uint32_t);
    buffer.Store(cursor, value);
    cursor += sizeof(T);
}
void __printf_append_arg(inout RWByteAddressBuffer buffer, inout uint32_t cursor, in const float value) {
    __printf_append_arg_with_code(buffer, cursor, typeID_float, value);
}
void __printf_append_arg(inout RWByteAddressBuffer buffer, inout uint32_t cursor, in const uint32_t value) {
    __printf_append_arg_with_code(buffer, cursor, typeID_uint, value);
}
void __printf_append_arg(inout RWByteAddressBuffer buffer, inout uint32_t cursor, in const int32_t value) {
    __printf_append_arg_with_code(buffer, cursor, typeID_int, value);
}

uint32_t __printf_args_size()
{
    return 0;
}
template <typename T0>
uint32_t __printf_args_size(T0)
{
    return sizeof(T0) + 1 * sizeof(uint32_t);
}
template <typename T0, typename T1>
uint32_t __printf_args_size(T0, T1)
{
    return sizeof(T0) + sizeof(T1) + 2 * sizeof(uint32_t);
}
template <typename T0, typename T1, typename T2>
uint32_t __printf_args_size(T0, T1, T2)
{
    return sizeof(T0) + sizeof(T1) + sizeof(T2) + 3 * sizeof(uint32_t);
}
template <typename T0, typename T1, typename T2, typename T3>
uint32_t __printf_args_size(T0, T1, T2, T3)
{
    return sizeof(T0) + sizeof(T1) + sizeof(T2) + sizeof(T3) + 4 * sizeof(uint32_t);
}
template <typename T0, typename T1, typename T2, typename T3, typename T4>
uint32_t __printf_args_size(T0, T1, T2, T3, T4)
{
    return sizeof(T0) + sizeof(T1) + sizeof(T2) + sizeof(T3) + sizeof(T4) + 5 * sizeof(uint32_t);
}
template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
uint32_t __printf_args_size(T0, T1, T2, T3, T4, T5)
{
    return sizeof(T0) + sizeof(T1) + sizeof(T2) + sizeof(T3) + sizeof(T4) + sizeof(T5) + 6 * sizeof(uint32_t);
}
template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
uint32_t __printf_args_size(T0, T1, T2, T3, T4, T5, T6)
{
    return sizeof(T0) + sizeof(T1) + sizeof(T2) + sizeof(T3) + sizeof(T4) + sizeof(T5) + sizeof(T6) + 7 * sizeof(uint32_t);
}
template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
uint32_t __printf_args_size(T0, T1, T2, T3, T4, T5, T6, T7)
{
    return sizeof(T0) + sizeof(T1) + sizeof(T2) + sizeof(T3) + sizeof(T4) + sizeof(T5) + sizeof(T6) + sizeof(T7) + 8 * sizeof(uint32_t);
}
template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
uint32_t __printf_args_size(T0, T1, T2, T3, T4, T5, T6, T7, T8)
{
    return sizeof(T0) + sizeof(T1) + sizeof(T2) + sizeof(T3) + sizeof(T4) + sizeof(T5) + sizeof(T6) + sizeof(T7) + sizeof(T8) + 9 * sizeof(uint32_t);
}

void __printf_append_args()
{
}
template <typename T0>
void __printf_append_args(inout RWByteAddressBuffer buffer, inout uint32_t cursor, T0 arg0)
{
    __printf_append_arg(buffer, cursor, arg0);
}
template <typename T0, typename T1>
void __printf_append_args(inout RWByteAddressBuffer buffer, inout uint32_t cursor, T0 arg0, T1 arg1)
{
    __printf_append_arg(buffer, cursor, arg0);
    __printf_append_arg(buffer, cursor, arg1);
}
template <typename T0, typename T1, typename T2>
void __printf_append_args(inout RWByteAddressBuffer buffer, inout uint32_t cursor, T0 arg0, T1 arg1, T2 arg2)
{
    __printf_append_arg(buffer, cursor, arg0);
    __printf_append_arg(buffer, cursor, arg1);
    __printf_append_arg(buffer, cursor, arg2);
}
template <typename T0, typename T1, typename T2, typename T3>
void __printf_append_args(inout RWByteAddressBuffer buffer, inout uint32_t cursor, T0 arg0, T1 arg1, T2 arg2, T3 arg3)
{
    __printf_append_arg(buffer, cursor, arg0);
    __printf_append_arg(buffer, cursor, arg1);
    __printf_append_arg(buffer, cursor, arg2);
    __printf_append_arg(buffer, cursor, arg3);
}
template <typename T0, typename T1, typename T2, typename T3, typename T4>
void __printf_append_args(inout RWByteAddressBuffer buffer, inout uint32_t cursor, T0 arg0, T1 arg1, T2 arg2, T3 arg3, T4 arg4)
{
    __printf_append_arg(buffer, cursor, arg0);
    __printf_append_arg(buffer, cursor, arg1);
    __printf_append_arg(buffer, cursor, arg2);
    __printf_append_arg(buffer, cursor, arg3);
    __printf_append_arg(buffer, cursor, arg4);
}
template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
void __printf_append_args(inout RWByteAddressBuffer buffer, inout uint32_t cursor, T0 arg0, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5)
{
    __printf_append_arg(buffer, cursor, arg0);
    __printf_append_arg(buffer, cursor, arg1);
    __printf_append_arg(buffer, cursor, arg2);
    __printf_append_arg(buffer, cursor, arg3);
    __printf_append_arg(buffer, cursor, arg4);
    __printf_append_arg(buffer, cursor, arg5);
}
template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
void __printf_append_args(inout RWByteAddressBuffer buffer, inout uint32_t cursor, T0 arg0, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6)
{
    __printf_append_arg(buffer, cursor, arg0);
    __printf_append_arg(buffer, cursor, arg1);
    __printf_append_arg(buffer, cursor, arg2);
    __printf_append_arg(buffer, cursor, arg3);
    __printf_append_arg(buffer, cursor, arg4);
    __printf_append_arg(buffer, cursor, arg5);
    __printf_append_arg(buffer, cursor, arg6);
}
template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
void __printf_append_args(inout RWByteAddressBuffer buffer, inout uint32_t cursor, T0 arg0, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6, T7 arg7)
{
    __printf_append_arg(buffer, cursor, arg0);
    __printf_append_arg(buffer, cursor, arg1);
    __printf_append_arg(buffer, cursor, arg2);
    __printf_append_arg(buffer, cursor, arg3);
    __printf_append_arg(buffer, cursor, arg4);
    __printf_append_arg(buffer, cursor, arg5);
    __printf_append_arg(buffer, cursor, arg6);
    __printf_append_arg(buffer, cursor, arg7);
}
template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
void __printf_append_args(inout RWByteAddressBuffer buffer, inout uint32_t cursor, T0 arg0, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6, T7 arg7, T8 arg8)
{
    __printf_append_arg(buffer, cursor, arg0);
    __printf_append_arg(buffer, cursor, arg1);
    __printf_append_arg(buffer, cursor, arg2);
    __printf_append_arg(buffer, cursor, arg3);
    __printf_append_arg(buffer, cursor, arg4);
    __printf_append_arg(buffer, cursor, arg5);
    __printf_append_arg(buffer, cursor, arg6);
    __printf_append_arg(buffer, cursor, arg7);
    __printf_append_arg(buffer, cursor, arg8);
}


// Took tips / inspiration from:
// https://therealmjp.github.io/posts/hlsl-printf/#setting-up-the-print-buffer
#define printf(state, str, ...)                                                                         \
    if (!state.paused) {                                                                                \
        const uint32_t lenU8 = strlen(str);                                                             \
        const uint32_t lenU32 = ((lenU8 - 1) / sizeof(uint32_t)) + 1;                                   \
        const uint32_t paddedLenU8 = lenU32 * sizeof(uint32_t);                                         \
                                                                                                        \
        uint32_t commandLength = sizeof(CommandHeader) + paddedLenU8 + __printf_args_size(__VA_ARGS__); \
        uint32_t cursor;                                                                                \
        state.printBuffer.InterlockedAdd(StreamSizeAddress, commandLength, cursor);                     \
        cursor += StreamStartAddress;                                                                   \
                                                                                                        \
        CommandHeader commandHeader;                                                                    \
        commandHeader.stringLength = paddedLenU8;                                                       \
        commandHeader.commandLength = commandLength;                                                    \
        state.printBuffer.Store(cursor, commandHeader);                                                 \
        cursor += sizeof(commandHeader);                                                                \
                                                                                                        \
        for (uint32_t i = 0; i < lenU32; ++i) {                                                         \
            const uint32_t begin = i * sizeof(uint32_t);                                                \
            const uint32_t end = min(begin + sizeof(uint32_t), lenU8);                                  \
            uint32_t c32 = 0;                                                                           \
            for (uint32_t j = begin; j < end; ++j) {                                                    \
                const uint32_t c = char_to_int(str[j]);                                                 \
                c32 |= c << ((j - begin) * 8);                                                          \
            }                                                                                           \
            state.printBuffer.Store(cursor, c32);                                                       \
            cursor += sizeof(uint32_t);                                                                 \
        }                                                                                               \
        __printf_append_args(state.printBuffer, cursor, __VA_ARGS__);                                   \
    }