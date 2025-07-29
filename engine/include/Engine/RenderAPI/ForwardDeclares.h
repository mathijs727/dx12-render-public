#pragma once

namespace RenderAPI {

// Buffer
template <typename BufferDesc>
class CPUBufferLinearAllocator;
class CPUBufferRingAllocator;

// Descriptor
class CPUDescriptorLinearAllocator;
struct DescriptorAllocation;
class DescriptorBlockAllocator;
class GPUDescriptorLinearAllocator;
class GPUDescriptorStaticAllocator;

// Internal
template <typename T>
class D3D12MAWrapper;

class CommandListManager;
struct Fence;
struct D3D12MAResource;
struct AliasingResource;
class ResourceAliasManager;
struct PipelineState;
struct Shader;
struct ShaderBindingTableInfo;
class ShaderBindingTableBuilder;
struct SRVDesc;
struct UAVDesc;
class StateObjectBuilder;
struct SwapChain;

}