#pragma once
#include "Engine/Core/Profiling.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <imgui.h>
DISABLE_WARNINGS_POP()
#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>

namespace UI {

// User interface modelled after LegitProfiler:
// https://github.com/Raikiri/LegitProfiler
class ImguiBarProfilerLegendRight {
public:
    ImguiBarProfilerLegendRight(uint32_t numFrames, std::chrono::duration<double, std::milli> tickPeriod);

    void addFrame(std::deque<Core::ProfileTask>&& tasks, uint64_t frameStart, uint64_t frameEnd);
    void draw(const char* pLabel, ImVec2 frameSize, std::chrono::duration<double, std::milli> scale);

private:
    struct Frame {
        uint64_t start, end;
        std::deque<Core::ProfileTask> tasks;
    };

    const uint32_t m_numHistoricFrames;
    std::deque<Frame> m_history;
    std::unordered_map<std::string, ImColor> m_colorMap;

    const std::chrono::duration<double, std::milli> m_tickPeriod;
};

}
