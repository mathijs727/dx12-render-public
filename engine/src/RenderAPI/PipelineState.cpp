#include "Engine/RenderAPI/PipelineState.h"

namespace RenderAPI {

D3D12_INPUT_ELEMENT_DESC sensibleDefaultsInputElementDesc()
{
    return D3D12_INPUT_ELEMENT_DESC {
        .SemanticName = nullptr,
        .SemanticIndex = 0,
        .InputSlot = 0,
        .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        .InstanceDataStepRate = 0
    };
}
template <typename T>
void setSensibleDefaultPipelineStateDesc(T& pipelineStateDesc)
{
    pipelineStateDesc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT {});
    pipelineStateDesc.SampleMask = UINT_MAX;
    pipelineStateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT {});
    pipelineStateDesc.RasterizerState.FrontCounterClockwise = true;
    pipelineStateDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT {});
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc.NumRenderTargets = 0;
    pipelineStateDesc.SampleDesc.Count = 1;
}

template void setSensibleDefaultPipelineStateDesc(D3D12_GRAPHICS_PIPELINE_STATE_DESC&);
template void setSensibleDefaultPipelineStateDesc(D3DX12_MESH_SHADER_PIPELINE_STATE_DESC&);

}
