#pragma once
#include "Engine/Core/Profiling.h"
#include "Engine/Render/ForwardDeclares.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/RenderAPI/MaResource.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <imgui.h>
DISABLE_WARNINGS_POP()
#include <chrono>
#include <cstddef>
#include <deque>
#include <list>
#include <string_view>
#include <vector>

namespace Render {

class GPUProfiler {
public:
    GPUProfiler(Render::RenderContext* pRenderContext);
    ~GPUProfiler();

    uint32_t startTask(ID3D12GraphicsCommandList5* pCommandList, std::string name);
    void endTask(ID3D12GraphicsCommandList5* pCommandList, uint32_t taskHandle);

private:
    uint32_t addTimingQuery(ID3D12GraphicsCommandList5* pCommandList);

private:
    Render::RenderContext* m_pRenderContext;
    WRL::ComPtr<ID3D12QueryHeap> m_pQueryHeap;
    uint32_t m_gpuQueryOffset = 0;

    struct Task {
        std::string name;
        uint32_t startQueryIdx, endQueryIdx;
    };
    std::vector<Task> m_tasks;
};

// User interface modelled after LegitProfiler:
// https://github.com/Raikiri/LegitProfiler
class GPUFrameProfiler {
public:
    GPUFrameProfiler(Render::RenderContext& renderContext, uint32_t resolvedFrameStorage);

    void startFrame(ID3D12GraphicsCommandList5* pCommandList);
    uint32_t startTask(ID3D12GraphicsCommandList5* pCommandList, std::string name);
    void endTask(ID3D12GraphicsCommandList5* pCommandList, uint32_t taskHandle);
    void endFrame(ID3D12GraphicsCommandList5* pCommandList);

    void displayHorizontalGUI() const;
    void displayVerticalGUI() const;

private:
private:
    struct Task {
        std::string name;
        uint32_t startQueryIdx, endQueryIdx;
        uint64_t startTimestamp, endTimestamp;
    };
    struct Frame {
        uint32_t startQueryIdx, endQueryIdx;
        uint64_t startTimestamp, endTimestamp;
        std::vector<Task> tasks;
    };
    uint32_t addTimingQuery(ID3D12GraphicsCommandList5* pCommandList);
    void resolveQueriesGPU(ID3D12GraphicsCommandList5* pCommandList, const Frame& frame);
    void resolveQueriesCPU(Frame& frame);

private:
    WRL::ComPtr<ID3D12QueryHeap> m_pQueryHeap;
    RenderAPI::D3D12MAResource m_readBackBuffer;
    std::vector<uint64_t> m_cpuBuffer;
    uint32_t m_gpuQueryOffset = 0;

    const double m_secondsPerTick;
    const uint32_t m_parallelFrames;
    const uint32_t m_resolvedFrameStorage;
    std::deque<Frame> m_inFlightFrames;
    std::deque<Frame> m_resolvedFrames;
};

}
