#pragma once
#include "Engine/Util/WindowsForwardDeclares.h"
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace Util {

class DirectoryChangeWatcher {
public:
    DirectoryChangeWatcher(
        const std::filesystem::path& directoryPath, std::chrono::milliseconds delay = {});
    ~DirectoryChangeWatcher();

    bool hasChanged();

private:
    void setReadDirectoryChange();

private:
    struct Implementation;

    std::unique_ptr<Implementation> m_pImpl;
    

    using clock = std::chrono::system_clock;
    std::chrono::milliseconds m_delay;
    clock::time_point m_lastChange;
    bool m_timerRunning = false;
};

}
