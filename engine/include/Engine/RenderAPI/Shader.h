#pragma once
#include "Internal/D3D12Includes.h"
#include <dxcapi.h>
#include <filesystem>

namespace RenderAPI {

struct Shader {
public:
    WRL::ComPtr<IDxcBlob> pBlob;

public:
    inline operator D3D12_SHADER_BYTECODE() const
    {
        return D3D12_SHADER_BYTECODE {
            .pShaderBytecode = pBlob->GetBufferPointer(),
            .BytecodeLength = pBlob->GetBufferSize()
        };
    }
};
Shader loadShader(ID3D12Device5*, const std::filesystem::path&);

}