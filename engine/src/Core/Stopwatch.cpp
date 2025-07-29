#include "Engine/Core/Stopwatch.h"

namespace Core {

void Stopwatch::start()
{
    m_startTime = high_res_clock::now();
}

Stopwatch::FrameTime Stopwatch::restart()
{
    const auto now = high_res_clock::now();
    const FrameTime out = now - m_startTime;
    m_startTime = now;
    return out;
}

Stopwatch::FrameTime Stopwatch::timeSinceStart() const
{
    return high_res_clock::now() - m_startTime;
}

}
