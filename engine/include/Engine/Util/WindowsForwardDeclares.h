#pragma once
#include <cstdint>

using UINT = unsigned;
using WPARAM = unsigned long long;
using LPARAM = int64_t;
struct HWND__;
using HWND = HWND__*;
using LRESULT = long long;
#define CALLBACK __stdcall
struct HINSTANCE__;
using HINSTANCE = HINSTANCE__*;

#define FAILED(hr) (((HRESULT)(hr)) < 0)