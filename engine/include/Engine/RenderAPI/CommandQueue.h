#pragma once
#include "Internal/D3D12Includes.h"

namespace RenderAPI {

WRL::ComPtr<ID3D12CommandQueue> createCommandQueue(ID3D12Device5*, D3D12_COMMAND_LIST_TYPE);

}