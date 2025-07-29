#pragma once
#include <chrono>

namespace Core {

class Stopwatch {
public:
    using FrameTime = std::chrono::duration<float, std::milli>;

    void start();
    FrameTime restart();
    FrameTime timeSinceStart() const;

private:
    using high_res_clock = std::chrono::high_resolution_clock;
    std::chrono::high_resolution_clock::time_point m_startTime { high_res_clock::now() };
};

}
