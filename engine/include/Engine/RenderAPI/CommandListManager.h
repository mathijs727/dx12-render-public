#pragma once
#include "Fence.h"
#include "Internal/D3D12Includes.h"
#include <deque>

namespace RenderAPI {

class CommandListManager {
public:
    CommandListManager(const WRL::ComPtr<ID3D12Device5>& pDevice, D3D12_COMMAND_LIST_TYPE type);

    WRL::ComPtr<ID3D12GraphicsCommandList6> acquireCommandList();
    void recycleCommandList(ID3D12CommandQueue*, const WRL::ComPtr<ID3D12GraphicsCommandList6>&);


private:
    WRL::ComPtr<ID3D12Device5> m_pDevice;
    D3D12_COMMAND_LIST_TYPE m_commandListType;
    Fence m_fence;

    struct InFlightCommandAllocator {
        ID3D12CommandAllocator* pCommandAllocator;
        uint64_t fenceValue;
    };
    std::deque<WRL::ComPtr<ID3D12CommandAllocator>> m_pCommandAllocatorOwners;
    std::deque<InFlightCommandAllocator> m_inFlightCommandAllocators;
    std::deque<WRL::ComPtr<ID3D12GraphicsCommandList6>> m_pUnusedCommandLists;
};

}
