#include "Engine/RenderAPI/ImGui.h"
#include "Engine/RenderAPI/Descriptor/DescriptorAllocation.h"
#include "Engine/RenderAPI/SwapChain.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <imgui.h>
#include <imgui_impl_dx12.h>
DISABLE_WARNINGS_POP()

namespace RenderAPI {

void initImGuiDescriptorD3D12(
    ID3D12Device5* pDevice,
    ID3D12DescriptorHeap* pGpuVisibleDescriptorHeap,
    const DescriptorAllocation& descriptorAllocation,
    const SwapChain& swapChain)
{
    // Initialize ImGui. This call will fill the CPU descriptor.
    ImGui_ImplDX12_Init(
        pDevice, swapChain.s_parallelFrames, swapChain.backBufferFormat,
        pGpuVisibleDescriptorHeap,
        descriptorAllocation.firstCPUDescriptor,
        descriptorAllocation.firstGPUDescriptor);
    ImGui_ImplDX12_CreateDeviceObjects();
}

}