#pragma once
#include "ForwardDeclares.h"
#include "Internal/D3D12Includes.h"

namespace RenderAPI {

void initImGuiDescriptorD3D12(
	ID3D12Device5* pDevice,
	ID3D12DescriptorHeap* pGpuVisibleDescriptorHeap,
	const DescriptorAllocation& descriptorAllocation, 
	const SwapChain& swapChain);

}