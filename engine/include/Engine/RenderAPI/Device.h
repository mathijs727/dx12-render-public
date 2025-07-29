#pragma once
#include "Internal/D3D12Includes.h"

namespace RenderAPI {

WRL::ComPtr<IDXGIAdapter4> createAdapter();
WRL::ComPtr<ID3D12Device5> createDevice(IDXGIAdapter4* pAdapter);
WRL::ComPtr<ID3D12DebugDevice1> createDebugDevice(ID3D12Device5* pDevice);

void reportLiveObjects(ID3D12Device5* pDevice);

}