#pragma once
#include "Internal/D3D12Includes.h"

namespace RenderAPI {

struct PipelineState {
    WRL::ComPtr<ID3D12PipelineState> pPipelineState;
    WRL::ComPtr<ID3D12RootSignature> pRootSignature;

    inline WRL::ComPtr<ID3D12PipelineState>& operator->() { return pPipelineState; };
};

D3D12_INPUT_ELEMENT_DESC sensibleDefaultsInputElementDesc();
template <typename T = D3D12_GRAPHICS_PIPELINE_STATE_DESC>
void setSensibleDefaultPipelineStateDesc(T& pipelineStateDesc);

}