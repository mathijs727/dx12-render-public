#include "Engine/Render/Debug.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/SwapChain.h"

namespace Render {

DebugBufferReader::DebugBufferReader(ID3D12Resource* pResource, const D3D12_RESOURCE_DESC& resourceDesc, RenderContext& renderContext)
    : m_pResource(pResource)
{
    auto readBackResourceDesc = resourceDesc;
    readBackResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    const auto numParallelFrames = RenderAPI::SwapChain::s_parallelFrames;
    for (int parallelFrameIdx = 0; parallelFrameIdx < numParallelFrames; ++parallelFrameIdx) {
        m_readBackBuffers.emplace_back(renderContext.createResource(D3D12_HEAP_TYPE_READBACK, readBackResourceDesc, D3D12_RESOURCE_STATE_COPY_DEST));
    }
}

}
