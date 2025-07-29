#include "Engine/Render/GPUProfiler.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include "Engine/Util/ErrorHandling.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <cppitertools/enumerate.hpp>
#include <cppitertools/zip.hpp>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <algorithm>
#include <array>
#include <numeric>
#include <string_view>

namespace Render {

static constexpr UINT queryHeapSize = 4096;

// Distinct colors generated using:
// http://vrl.cs.brown.edu/color
static const std::array s_colors {
    ImColor(156, 26, 84),
    ImColor(247, 57, 49),
    ImColor(45, 160, 161),
    ImColor(238, 119, 160),
    ImColor(98, 125, 227),
    ImColor(224, 113, 66),
    ImColor(38, 73, 109),
    ImColor(194, 223, 125),
    ImColor(255, 0, 135),
    ImColor(86, 235, 211),
    ImColor(21, 81, 38),
    ImColor(238, 192, 82),
    ImColor(73, 56, 142),
    ImColor(121, 155, 81),
    ImColor(102, 61, 32),
    ImColor(244, 202, 203),
    ImColor(191, 112, 225),
    ImColor(106, 16, 166),
    ImColor(161, 222, 240),
    ImColor(55, 196, 84)
};

static double getSecondsPerTick(Render::RenderContext& renderContext)
{
    uint64_t frequency;
    RenderAPI::ThrowIfFailed(renderContext.pGraphicsQueue->GetTimestampFrequency(&frequency));
    return 1.0 / double(frequency);
}

GPUFrameProfiler::GPUFrameProfiler(Render::RenderContext& renderContext, uint32_t resolvedFrameStorage)
    : m_parallelFrames(RenderAPI::SwapChain::s_parallelFrames)
    , m_resolvedFrameStorage(resolvedFrameStorage)
    , m_cpuBuffer((size_t)queryHeapSize, 0)
    , m_secondsPerTick(getSecondsPerTick(renderContext))
{
    D3D12_QUERY_HEAP_DESC heapDesc {};
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count = queryHeapSize;
    renderContext.pDevice->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_pQueryHeap));

    const auto readBackBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(queryHeapSize * sizeof(uint64_t), D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
    m_readBackBuffer = renderContext.createResource(D3D12_HEAP_TYPE_READBACK, readBackBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST);
}

void GPUFrameProfiler::startFrame(ID3D12GraphicsCommandList5* pCommandList)
{
    // After m_parallelFrames we are sure that a frame has finished rendering on the GPU.
    // We can now safely read its contents from the readback buffer.
    if (m_inFlightFrames.size() == m_parallelFrames) {
        auto& lastRenderedFrame = m_inFlightFrames.back();
        resolveQueriesCPU(lastRenderedFrame);
        m_resolvedFrames.push_front(std::move(lastRenderedFrame));
        m_inFlightFrames.pop_back();

        if (m_resolvedFrames.size() > m_resolvedFrameStorage)
            m_resolvedFrames.pop_back();
    }

    m_inFlightFrames.push_front({ .startQueryIdx = addTimingQuery(pCommandList) });
}

uint32_t GPUFrameProfiler::startTask(ID3D12GraphicsCommandList5* pCommandList, std::string name)
{
    auto& frame = m_inFlightFrames.front();
    const uint32_t taskHandle = (uint32_t)frame.tasks.size();
    const uint32_t startQueryIdx = addTimingQuery(pCommandList);
    frame.tasks.push_back({ .name = name, .startQueryIdx = startQueryIdx });
    return taskHandle;
}

void GPUFrameProfiler::endTask(ID3D12GraphicsCommandList5* pCommandList, uint32_t taskHandle)
{
    auto& frame = m_inFlightFrames.front();
    Task& task = frame.tasks[taskHandle];
    const uint32_t endQueryIdx = addTimingQuery(pCommandList);
    task.endQueryIdx = addTimingQuery(pCommandList);
}

void GPUFrameProfiler::endFrame(ID3D12GraphicsCommandList5* pCommandList)
{
    auto& frame = m_inFlightFrames.front();
    frame.endQueryIdx = addTimingQuery(pCommandList);
    resolveQueriesGPU(pCommandList, frame);
}

uint32_t GPUFrameProfiler::addTimingQuery(ID3D12GraphicsCommandList5* pCommandList)
{
    const uint32_t queryOffset = m_gpuQueryOffset;
    pCommandList->EndQuery(m_pQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_gpuQueryOffset);
    m_gpuQueryOffset = (m_gpuQueryOffset + 1) % queryHeapSize;
    return queryOffset;
}

void GPUFrameProfiler::resolveQueriesGPU(ID3D12GraphicsCommandList5* pCommandList, const Frame& frame)
{
    const auto resolveRange = [this, pCommandList](uint32_t startQueryIdx, uint32_t endQueryIdx) {
        const auto count = endQueryIdx - startQueryIdx;
        pCommandList->ResolveQueryData(
            m_pQueryHeap.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            startQueryIdx,
            count,
            m_readBackBuffer,
            startQueryIdx * sizeof(uint64_t));
    };
    if (frame.startQueryIdx > frame.endQueryIdx) {
        resolveRange(frame.startQueryIdx, queryHeapSize);
        resolveRange(0, frame.endQueryIdx + 1);
    } else {
        resolveRange(frame.startQueryIdx, frame.endQueryIdx + 1);
    }
}

void GPUFrameProfiler::resolveQueriesCPU(Frame& frame)
{
    const auto readRange = [this](uint32_t startQueryOffset, uint32_t endQueryOffset) {
        uint64_t* data;
        const CD3DX12_RANGE readRange { startQueryOffset * sizeof(uint64_t), endQueryOffset * sizeof(uint64_t) };
        RenderAPI::ThrowIfFailed(m_readBackBuffer->Map(0, &readRange, (void**)&data));
        std::memcpy(&m_cpuBuffer[startQueryOffset], &data[startQueryOffset], (endQueryOffset - startQueryOffset) * sizeof(uint64_t));
        const D3D12_RANGE writeRange { 0, 0 };
        m_readBackBuffer->Unmap(0, &writeRange);
    };
    if (frame.startQueryIdx > frame.endQueryIdx) {
        readRange(frame.startQueryIdx, queryHeapSize);
        readRange(0, frame.endQueryIdx + 1);
    } else {
        readRange(frame.startQueryIdx, frame.endQueryIdx + 1);
    }

    frame.startTimestamp = m_cpuBuffer[frame.startQueryIdx];
    frame.endTimestamp = m_cpuBuffer[frame.endQueryIdx];
    for (auto& task : frame.tasks) {
        task.startTimestamp = m_cpuBuffer[task.startQueryIdx];
        task.endTimestamp = m_cpuBuffer[task.endQueryIdx];
    }
}

void GPUFrameProfiler::displayHorizontalGUI() const
{
    // User interface modeled after LegitProfiler:
    // https://github.com/Raikiri/LegitProfiler

    // Code taken from PlotEx function in imgui_widgets.cpp and modified.
    // https://github.com/ocornut/imgui/blob/master/imgui_widgets.cpp

    if (m_resolvedFrames.empty())
        return;

    ImGuiContext& g = *GImGui;
    ImGuiWindow* pWindow = ImGui::GetCurrentWindow();
    // Don't draw if the pWindow is collapsed.
    if (pWindow->SkipItems)
        return;
    const ImGuiStyle& style = g.Style;
    // const ImGuiID id = pWindow->GetID(pLabel);

    const uint32_t widgetWidth = uint32_t(std::max(1.0f, ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x));
    const uint32_t widgetHeight = widgetWidth / 2;

    const float normalizedBarWidth = 1.0f / static_cast<float>(m_resolvedFrameStorage);
    const float normalizedBarPadding = normalizedBarWidth * 0.05f;
    const double widgetMaxTimeInSeconds = 0.033f;
    const double normalizedBarHeightScale = m_secondsPerTick / widgetMaxTimeInSeconds;

    // Compute legend items.
    struct LegendItem {
        std::string label;
        // Top/bot right corners of the associated bar.
        ImVec2 barBotRight;
        ImVec2 barTopRight;
    };
    std::vector<LegendItem> legendItems;
    for (const auto& task : m_resolvedFrames.front().tasks) {
        const double timingInSeconds = (task.endTimestamp - task.startTimestamp) * m_secondsPerTick;
        legendItems.push_back(LegendItem { .label = fmt::format("[{:2.2f}]ms {}", timingInSeconds * 1000.0, task.name) });
    }

    // Compute the size of the legend.
    const ImVec2 maxLabelSize = std::transform_reduce(
        std::begin(legendItems), std::end(legendItems),
        ImVec2(0, 0),
        [](const ImVec2& lhs, const ImVec2& rhs) {
            return ImVec2(std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y));
        },
        [](const LegendItem& legendItem) {
            return ImGui::CalcTextSize(legendItem.label.c_str(), nullptr, true);
        });
    const ImVec2 markerSize { maxLabelSize.y, maxLabelSize.y }; // Color cube to the left of the label.
    const float markerConnectorWidth = 3.0f * markerSize.x; // The colored triangle connecting the marker to the bar.
    const float legendWidth = markerConnectorWidth + markerSize.x + style.ItemInnerSpacing.x + maxLabelSize.x;

    // Compute a reasonable maximum width for the bar plot ticks.
    const ImVec2 barPlotTickSize = ImGui::CalcTextSize("00.0ms") + ImVec2(style.ItemInnerSpacing.x, 0);
    const float halfTickHeight = 0.5f * barPlotTickSize.y;

    // Total drawing frame.
    const ImRect outerFrameBounds(pWindow->DC.CursorPos, pWindow->DC.CursorPos + ImVec2(float(widgetWidth), float(widgetHeight)));
    // Add some padding according to the layout.
    const ImRect innerFrameBounds(outerFrameBounds.Min + style.FramePadding, outerFrameBounds.Max - style.FramePadding);
    // Draw the tick marks at the left.
    const ImRect barPlotTicksBounds(innerFrameBounds.Min, ImVec2(innerFrameBounds.Min.x + barPlotTickSize.x, innerFrameBounds.Max.y));
    // Draw the legend at the right.
    const ImRect legendBounds(ImVec2(innerFrameBounds.Max.x - legendWidth, innerFrameBounds.Min.y), innerFrameBounds.Max);
    // Draw the bar plot in the middle.
    const ImRect barPlotBounds(ImVec2(barPlotTicksBounds.Max.x, innerFrameBounds.Min.y), ImVec2(legendBounds.Min.x, innerFrameBounds.Max.y));

    ImGui::ItemSize(outerFrameBounds, 0); // Increment cursor by outerFrameBounds
    if (!ImGui::ItemAdd(innerFrameBounds, 0)) // Draw inside innerFrameBounds
        return;

    // Draw tick marks.
    constexpr int numTickMarks = 5;
    for (int i = 0; i < numTickMarks; i++) {
        const auto tickLabel = fmt::format("{:2.1f}ms", 1000 * widgetMaxTimeInSeconds * i / (numTickMarks - 1));
        const auto tickWidth = ImGui::CalcTextSize(tickLabel.c_str()).x;
        const ImVec2 pos {
            barPlotTicksBounds.Max.x - style.ItemInnerSpacing.x - tickWidth,
            ImLerp(barPlotTicksBounds.Min.y, barPlotTicksBounds.Max.y, 1.0f - static_cast<float>(i) / (numTickMarks - 1)) - 0.5f * barPlotTickSize.y
        };
        pWindow->DrawList->AddText(pos, 0xFFFFFFFF, tickLabel.c_str());
    }

    // Draw the bar plot frame.
    ImGui::RenderFrame(barPlotBounds.Min, barPlotBounds.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    if (m_resolvedFrames.empty())
        return;

    // Draw the barplot.
    const ImU32 colorBase = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    // const ImU32 colorHovered = ImGui::GetColorU32(ImGuiCol_PlotHistogramHovered);
    for (const auto [frameIdx, frame] : iter::enumerate(m_resolvedFrames)) {
        const float t0 = frameIdx * normalizedBarWidth;
        const float t1 = (frameIdx + 1) * normalizedBarWidth;

        const auto f = frame; // Work-around Clang error regarding capturing reference to structured binding.
        const auto drawBar = [=](ImU32 color, uint64_t v0, uint64_t v1, ImVec2* pTopRight = nullptr, ImVec2* pBotRight = nullptr) {
            const ImVec2 tp0 = ImVec2(t0 + normalizedBarPadding, 1.0f - ImSaturate(static_cast<float>((v0 - f.startTimestamp) * normalizedBarHeightScale)));
            const ImVec2 tp1 = ImVec2(t1 - normalizedBarPadding, 1.0f - ImSaturate(static_cast<float>((v1 - f.startTimestamp) * normalizedBarHeightScale)));

            const ImVec2 pos0 = ImLerp(barPlotBounds.Min, barPlotBounds.Max, tp0);
            const ImVec2 pos1 = ImLerp(barPlotBounds.Min, barPlotBounds.Max, tp1);
            pWindow->DrawList->AddRectFilled(pos0, pos1, color);

            if (pTopRight)
                *pTopRight = ImVec2(pos1.x, pos1.y);
            if (pBotRight)
                *pBotRight = ImVec2(pos1.x, pos0.y);
        };

        // Draw background bar to represent time periods that are unaccounted for.
        drawBar(colorBase, frame.startTimestamp, frame.endTimestamp);

        for (const auto& [taskIdx, task] : iter::enumerate(frame.tasks)) {
            const size_t nameHash = std::hash<std::string_view>()(task.name);
            ImColor color = s_colors[nameHash % s_colors.size()];

            if (frameIdx == m_resolvedFrames.size() - 1)
                drawBar(color, task.startTimestamp, task.endTimestamp, &legendItems[taskIdx].barTopRight, &legendItems[taskIdx].barBotRight);
            else
                drawBar(color, task.startTimestamp, task.endTimestamp);
        }
    }

    // Draw legend.
    const auto& frame = m_resolvedFrames.front();
    for (const auto& [i, legendAndTask] : iter::enumerate(iter::zip(legendItems, frame.tasks))) {
        // Draw marker.
        const auto& [legendItem, task] = legendAndTask;
        const ImVec2 markerPos {
            legendBounds.Min.x + markerConnectorWidth,
            legendBounds.Max.y - i * (markerSize.y + style.ItemInnerSpacing.y) - markerSize.y
        };
        const size_t nameHash = std::hash<std::string_view>()(task.name);
        const ImColor markerColor = s_colors[nameHash % s_colors.size()];
        pWindow->DrawList->AddRectFilled(markerPos, markerPos + markerSize, markerColor);

        // Draw label.
        const ImVec2 labelPos {
            markerPos.x + markerSize.x + style.ItemInnerSpacing.x,
            markerPos.y
        };
        pWindow->DrawList->AddText(labelPos, 0xffffffff, legendItem.label.c_str());

        // Draw the connector thingy that connects the bar to the marker.
        const auto horizontalPadding = ImVec2(style.ItemInnerSpacing.x / 2.0f, 0);
        const std::array points {
            legendItem.barBotRight + horizontalPadding,
            legendItem.barTopRight + horizontalPadding,
            markerPos - horizontalPadding,
            markerPos - horizontalPadding + ImVec2(0, markerSize.y)
        };
        pWindow->DrawList->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), markerColor);
    }
}

void GPUFrameProfiler::displayVerticalGUI() const
{
    // User interface modeled after LegitProfiler:
    // https://github.com/Raikiri/LegitProfiler

    // Code taken from PlotEx function in imgui_widgets.cpp and modified.
    // https://github.com/ocornut/imgui/blob/master/imgui_widgets.cpp

    if (m_resolvedFrames.empty())
        return;

    ImGuiContext& g = *GImGui;
    ImGuiWindow* pWindow = ImGui::GetCurrentWindow();
    // Don't draw if the pWindow is collapsed.
    if (pWindow->SkipItems)
        return;
    const ImGuiStyle& style = g.Style;
    // const ImGuiID id = pWindow->GetID(pLabel);

    const uint32_t widgetWidth = uint32_t(std::max(1.0f, ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x));
    const uint32_t barPlotHeight = widgetWidth / 4;

    const float normalizedBarWidth = 1.0f / static_cast<float>(m_resolvedFrameStorage);
    const float normalizedBarPadding = normalizedBarWidth * 0.05f;
    const double widgetMaxTimeInSeconds = 0.033f;
    const double normalizedBarHeightScale = m_secondsPerTick / widgetMaxTimeInSeconds;

    // Compute a reasonable maximum width for the bar plot ticks.
    const ImVec2 barPlotTickSize = ImGui::CalcTextSize("00.0ms") + ImVec2(style.ItemInnerSpacing.x, 0);
    const float halfTickHeight = 0.5f * barPlotTickSize.y;

    // Frame containing the tick marks and bar plot (but not the legend).
    const ImRect outerFrameBounds(pWindow->DC.CursorPos, pWindow->DC.CursorPos + ImVec2(float(widgetWidth), float(barPlotHeight)));
    // Add some padding according to the layout.
    const ImRect innerFrameBounds(outerFrameBounds.Min + style.FramePadding, outerFrameBounds.Max - style.FramePadding);
    // Draw the tick marks at the left.
    const ImRect barPlotTicksBounds(innerFrameBounds.Min, ImVec2(innerFrameBounds.Min.x + barPlotTickSize.x, innerFrameBounds.Max.y));
    // Draw the bar plot in the middle.
    const ImRect barPlotBounds(ImVec2(barPlotTicksBounds.Max.x, innerFrameBounds.Min.y), innerFrameBounds.Max);

    ImGui::ItemSize(outerFrameBounds, 0); // Increment cursor by outerFrameBounds
    if (!ImGui::ItemAdd(innerFrameBounds, 0)) // Draw inside innerFrameBounds
        return;

    // Draw tick marks.
    constexpr int numTickMarks = 5;
    for (int i = 0; i < numTickMarks; i++) {
        const auto tickLabel = fmt::format("{:2.1f}ms", 1000 * widgetMaxTimeInSeconds * i / (numTickMarks - 1));
        const auto tickWidth = ImGui::CalcTextSize(tickLabel.c_str()).x;
        const ImVec2 pos {
            barPlotTicksBounds.Max.x - style.ItemInnerSpacing.x - tickWidth,
            ImLerp(barPlotTicksBounds.Min.y, barPlotTicksBounds.Max.y, 1.0f - static_cast<float>(i) / (numTickMarks - 1)) - 0.5f * barPlotTickSize.y
        };
        pWindow->DrawList->AddText(pos, 0xFFFFFFFF, tickLabel.c_str());
    }

    // Draw the bar plot frame.
    ImGui::RenderFrame(barPlotBounds.Min, barPlotBounds.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    if (m_resolvedFrames.empty())
        return;

    // Draw the barplot.
    const ImU32 colorBase = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    // const ImU32 colorHovered = ImGui::GetColorU32(ImGuiCol_PlotHistogramHovered);
    for (const auto [frameIdx, frame] : iter::enumerate(m_resolvedFrames)) {
        const float t0 = frameIdx * normalizedBarWidth;
        const float t1 = (frameIdx + 1) * normalizedBarWidth;

        const auto f = frame; // Work-around Clang error regarding capturing reference to structured binding.
        const auto drawBar = [=](ImU32 color, uint64_t v0, uint64_t v1) {
            const ImVec2 tp0 = ImVec2(t0 + normalizedBarPadding, 1.0f - ImSaturate(static_cast<float>((v0 - f.startTimestamp) * normalizedBarHeightScale)));
            const ImVec2 tp1 = ImVec2(t1 - normalizedBarPadding, 1.0f - ImSaturate(static_cast<float>((v1 - f.startTimestamp) * normalizedBarHeightScale)));

            const ImVec2 pos0 = ImLerp(barPlotBounds.Min, barPlotBounds.Max, tp0);
            const ImVec2 pos1 = ImLerp(barPlotBounds.Min, barPlotBounds.Max, tp1);
            pWindow->DrawList->AddRectFilled(pos0, pos1, color);
        };

        // Draw background bar to represent time periods that are unaccounted for.
        drawBar(colorBase, frame.startTimestamp, frame.endTimestamp);

        for (const auto& [taskIdx, task] : iter::enumerate(frame.tasks)) {
            const size_t nameHash = std::hash<std::string_view>()(task.name);
            ImColor color = s_colors[nameHash % s_colors.size()];
            drawBar(color, task.startTimestamp, task.endTimestamp);
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Draw legend.
    const auto& frame = m_resolvedFrames.front();
    const float timeColumnWidth = ImGui::CalcTextSize("123.00ms").x;
    const float markerSize1D = ImGui::CalcTextSize("ASDF").y;
    const ImVec2 markerSize { markerSize1D, markerSize1D };
    if (ImGui::BeginTable("Profiler Tasks", 3)) {
        ImGui::TableSetupColumn("AAA", ImGuiTableColumnFlags_WidthFixed, timeColumnWidth);
        ImGui::TableSetupColumn("BBB", ImGuiTableColumnFlags_WidthFixed, markerSize.x);
        ImGui::TableSetupColumn("CCC", ImGuiTableColumnFlags_WidthStretch);

        // Accumulate tasks by name and then sort them by their total duration.
        std::vector<std::pair<std::string, double>> taskDurations;
        for (const auto& task : frame.tasks) {
            const double timingInSeconds = (task.endTimestamp - task.startTimestamp) * m_secondsPerTick;

            // Check if the task already exists in the list.
            bool alreadyExists = false;
            for (auto& [name, duration] : taskDurations) {
                if (name == task.name) {
                    duration += timingInSeconds;
                    alreadyExists = true;
                    break;
                }
            }
            if (!alreadyExists) {
                // Add the task to the list with its duration.
                taskDurations.emplace_back(task.name, timingInSeconds);
            }
        }
        std::sort(std::begin(taskDurations), std::end(taskDurations),
            [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

        for (const auto& [name, duration] : taskDurations) {
            ImGui::TableNextRow();

            // Time is ms
            ImGui::TableNextColumn();
            ImGui::Text("%.2fms", duration * 1000);

            // Colored marker
            ImGui::TableNextColumn();
            const size_t nameHash = std::hash<std::string_view>()(name);
            const ImColor markerColor = s_colors[nameHash % s_colors.size()];
            pWindow->DrawList->AddRectFilled(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + markerSize, markerColor);

            ImGui::TableNextColumn();
            ImGui::Text("%s", name.data());
        }
        ImGui::EndTable();
    }
}

GPUProfiler::GPUProfiler(Render::RenderContext* pRenderContext)
    : m_pRenderContext(pRenderContext)
{
    D3D12_QUERY_HEAP_DESC heapDesc {};
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count = queryHeapSize;
    pRenderContext->pDevice->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_pQueryHeap));
}

GPUProfiler::~GPUProfiler()
{
    const auto readBackBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(queryHeapSize * sizeof(uint64_t), D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
    const auto readBackBuffer = m_pRenderContext->createResource(D3D12_HEAP_TYPE_READBACK, readBackBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST);
    auto pCommandList = m_pRenderContext->commandListManager.acquireCommandList();
    pCommandList->ResolveQueryData(
        m_pQueryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        0,
        m_gpuQueryOffset,
        readBackBuffer, 0);
    pCommandList->Close();
    ID3D12CommandList* commandLists[1] = { pCommandList.Get() };
    m_pRenderContext->pGraphicsQueue->ExecuteCommandLists(1, commandLists);
    m_pRenderContext->commandListManager.recycleCommandList(m_pRenderContext->pGraphicsQueue.Get(), pCommandList);
    m_pRenderContext->waitForIdle();

    uint64_t* data;
    const CD3DX12_RANGE readRange { 0, m_gpuQueryOffset * sizeof(uint64_t) };
    RenderAPI::ThrowIfFailed(readBackBuffer->Map(0, &readRange, (void**)&data));

    const auto secondsPerTicks = getSecondsPerTick(*m_pRenderContext);
    for (const auto& task : m_tasks) {
        const auto durationInTicks = data[task.endQueryIdx] - data[task.startQueryIdx];
        const auto durationInSeconds = durationInTicks * secondsPerTicks;
        spdlog::info("{} : {} ms", task.name, durationInSeconds * 1000);
    }

    const D3D12_RANGE writeRange { 0, 0 };
    readBackBuffer->Unmap(0, &writeRange);
}

uint32_t GPUProfiler::startTask(ID3D12GraphicsCommandList5* pCommandList, std::string name)
{
    const uint32_t taskHandle = (uint32_t)m_tasks.size();
    const uint32_t startQueryIdx = addTimingQuery(pCommandList);
    m_tasks.push_back({ .name = name, .startQueryIdx = startQueryIdx });
    return taskHandle;
}

void GPUProfiler::endTask(ID3D12GraphicsCommandList5* pCommandList, uint32_t taskHandle)
{
    Task& task = m_tasks[taskHandle];
    const uint32_t endQueryIdx = addTimingQuery(pCommandList);
    task.endQueryIdx = addTimingQuery(pCommandList);
}

uint32_t GPUProfiler::addTimingQuery(ID3D12GraphicsCommandList5* pCommandList)
{
    const uint32_t queryOffset = m_gpuQueryOffset;
    pCommandList->EndQuery(m_pQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_gpuQueryOffset);
    m_gpuQueryOffset = (m_gpuQueryOffset + 1) % queryHeapSize;
    return queryOffset;
}
}
