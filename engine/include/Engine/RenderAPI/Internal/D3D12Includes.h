#pragma once
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <source_location>
#include <spdlog/spdlog.h>
#include <tbx/error_handling.h>

#include <wrl/client.h>
namespace WRL = Microsoft::WRL;

namespace RenderAPI {

// From DXSampleHelper.h
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr, std::source_location location = std::source_location::current())
{
    const bool success = !FAILED(hr);
    if (!success) {
        spdlog::error("{} ({}:{}) \"{}\"", location.file_name(), location.line(), location.column(), location.function_name());
    }
    Tbx::assert_always(success);
}

}
