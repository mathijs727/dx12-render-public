#include "Engine/RenderAPI/Shader.h"
#include <cassert>
#include <fstream>
#include <vector>

namespace RenderAPI {

RenderAPI::Shader loadShader(ID3D12Device5* pDevice, const std::filesystem::path& shaderFilePath)
{
    static auto s_pIDxcLibrary = []() {
        WRL::ComPtr<IDxcLibrary> pTmp;
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pTmp)));
        return pTmp;
    }();
    assert(std::filesystem::exists(shaderFilePath));

    // Read binary DXIL file into memory
    // https://stackoverflow.com/questions/15138353/how-to-read-a-binary-file-into-a-vector-of-unsigned-chars
    std::ifstream file { shaderFilePath, std::ios::binary };
    file.unsetf(std::ios::skipws); // Stop eating new lines in binary mode.
    std::streampos fileSize;
    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> dxilData;
    dxilData.resize(fileSize);
    std::copy(std::istream_iterator<uint8_t>(file), std::istream_iterator<uint8_t>(), std::begin(dxilData));

    // Convert to DXIL Blob (making a copy of the data)
    // https://www.wihlidal.com/blog/pipeline/2018-09-16-dxil-signing-post-compile/
    WRL::ComPtr<IDxcBlobEncoding> pEncodedShaderLibrary;
    s_pIDxcLibrary->CreateBlobWithEncodingOnHeapCopy(
        (BYTE*)dxilData.data(),
        (UINT32)dxilData.size(),
        0,
        pEncodedShaderLibrary.GetAddressOf());

    // TODO: validation (see link)
    // https://www.wihlidal.com/blog/pipeline/2018-09-16-dxil-signing-post-compile/
    WRL::ComPtr<IDxcBlob> pShaderLibrary = pEncodedShaderLibrary;
    auto bufferSize = pShaderLibrary->GetBufferSize();
    return { .pBlob = pShaderLibrary };
}

}