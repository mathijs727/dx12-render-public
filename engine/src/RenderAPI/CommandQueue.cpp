#include "Engine/RenderAPI/CommandQueue.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/RenderAPI/Internal/D3D12MAHelpers.h"
#include <optional>

namespace RenderAPI {

WRL::ComPtr<ID3D12CommandQueue> createCommandQueue(ID3D12Device5* pDevice, D3D12_COMMAND_LIST_TYPE commandListType)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = commandListType;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    WRL::ComPtr<ID3D12CommandQueue> pCommandQueue;
    ThrowIfFailed(pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pCommandQueue)));
    return pCommandQueue;
}

}