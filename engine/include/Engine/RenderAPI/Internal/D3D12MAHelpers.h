#pragma once
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED 1
#include <D3D12MemAlloc.h>
DISABLE_WARNINGS_POP()
#include <cassert>

namespace RenderAPI {

// RAII allocation wrapper.
template <typename T>
class D3D12MAWrapper {
public:
    D3D12MAWrapper() = default;
    inline D3D12MAWrapper(T* p)
        : pointer(p)
    {
    }
    D3D12MAWrapper(const D3D12MAWrapper&) = delete;
    inline D3D12MAWrapper(D3D12MAWrapper<T>&& other)
        : pointer(other.pointer)
    {
        other.pointer = nullptr;
    }
    inline ~D3D12MAWrapper()
    {
        reset();
    }
    inline D3D12MAWrapper<T>& operator=(D3D12MAWrapper<T>&& other) noexcept
    {
        reset();
        pointer = other.pointer;
        other.pointer = nullptr;
        return *this;
    }
    inline operator bool() const
    {
        return pointer != nullptr;
    }
    inline T* operator&()
    {
        return pointer;
    }
    inline const T* operator&() const
    {
        return pointer;
    }
    inline operator T*()
    {
        return pointer;
    }
    inline operator const T*() const
    {
        return pointer;
    }
    inline T* get()
    {
        return pointer;
    }
    inline const T* get() const
    {
        return pointer;
    }
    inline T* operator->()
    {
        return pointer;
    }
    inline const T* operator->() const
    {
        return pointer;
    }

private:
    inline void reset()
    {
        if (this->pointer) {
            this->pointer->Release();
            this->pointer = nullptr;
        }
    }

private:
    T* pointer { nullptr };
};

}
