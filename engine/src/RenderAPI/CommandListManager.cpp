#include "Engine/RenderAPI/CommandListManager.h"

namespace RenderAPI {

static WRL::ComPtr<ID3D12CommandAllocator> createCommandAllocator(ID3D12Device5* pDevice, D3D12_COMMAND_LIST_TYPE type);
static WRL::ComPtr<ID3D12GraphicsCommandList6> createCommandList(ID3D12Device5* pDevice, ID3D12CommandAllocator* pRawCommandAllocator, D3D12_COMMAND_LIST_TYPE type);

CommandListManager::CommandListManager(const WRL::ComPtr<ID3D12Device5>& pDevice, D3D12_COMMAND_LIST_TYPE type)
    : m_pDevice(pDevice)
    , m_commandListType(type)
    , m_fence(createFence(pDevice.Get()))
{
}

WRL::ComPtr<ID3D12GraphicsCommandList6> CommandListManager::acquireCommandList()
{
    ID3D12CommandAllocator* pRawCommandAllocator;
    if (!m_inFlightCommandAllocators.empty() && fenceReached(m_fence, m_inFlightCommandAllocators.front().fenceValue)) {
        // Reuse CommandAllocator that has been used previously and whose commands have finished
        // executing on the GPU.
        // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset
        pRawCommandAllocator = m_inFlightCommandAllocators.front().pCommandAllocator;
        pRawCommandAllocator->Reset();
        m_inFlightCommandAllocators.pop_front();
    } else {
        auto pCommandAllocator = createCommandAllocator(m_pDevice.Get(), m_commandListType);
        pRawCommandAllocator = pCommandAllocator.Get();
        m_pCommandAllocatorOwners.push_back(std::move(pCommandAllocator));
    }

    WRL::ComPtr<ID3D12GraphicsCommandList6> pCommandList;
    if (!m_pUnusedCommandLists.empty()) {
        // Reuse CommandList that was used for a previous submission. It is safe to reset while
        // the commands are still being executed (as long as you use a new CommandAllocator):
        // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset
        pCommandList = m_pUnusedCommandLists.front();
        m_pUnusedCommandLists.pop_front();
        pCommandList->Reset(pRawCommandAllocator, nullptr);
    } else {
        // Allocate a new command list.
        pCommandList = createCommandList(m_pDevice.Get(), pRawCommandAllocator, m_commandListType);
    }

    pCommandList->SetPrivateData(__uuidof(pCommandList.Get()), sizeof(decltype(pRawCommandAllocator)), &pRawCommandAllocator);
    return pCommandList;
}

void CommandListManager::recycleCommandList(ID3D12CommandQueue* pCommandQueue, const WRL::ComPtr<ID3D12GraphicsCommandList6>& pCommandList)
{
    ID3D12CommandAllocator* pCommandAllocator;
    UINT dataSize = sizeof(decltype(pCommandAllocator));
    pCommandList->GetPrivateData(__uuidof(pCommandList.Get()), &dataSize, &pCommandAllocator);

    // CommandAllocator can only be reused when GPU is finished using it.
    const uint64_t fenceValue = insertFence(m_fence, pCommandQueue);
    m_inFlightCommandAllocators.push_back({ .pCommandAllocator = pCommandAllocator, .fenceValue = fenceValue });
    // CommandLists can be reused freely without waiting for the GPU (as long as you use a different command allocator).
    m_pUnusedCommandLists.push_back(pCommandList); // Can be reused immediately.
}

static WRL::ComPtr<ID3D12CommandAllocator> createCommandAllocator(ID3D12Device5* pDevice, D3D12_COMMAND_LIST_TYPE type)
{
    WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator;
    ThrowIfFailed(pDevice->CreateCommandAllocator(type, IID_PPV_ARGS(&pCommandAllocator)));
    return pCommandAllocator;
}

static WRL::ComPtr<ID3D12GraphicsCommandList6> createCommandList(
    ID3D12Device5* pDevice, ID3D12CommandAllocator* pRawCommandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
    WRL::ComPtr<ID3D12GraphicsCommandList6> pCommandList2;
    ThrowIfFailed(pDevice->CreateCommandList(
        0, // Node mask (multi GPU?)
        type,
        pRawCommandAllocator,
        nullptr,
        IID_PPV_ARGS(&pCommandList2)));
    return pCommandList2;
}

}
