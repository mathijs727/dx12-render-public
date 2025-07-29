#pragma once
#include <filesystem>

inline void createCleanDir(const std::filesystem::path& dirPath)
{
    if (std::filesystem::exists(dirPath))
        std::filesystem::remove_all(dirPath);
    std::filesystem::create_directories(dirPath);
}

inline void tryDeleteFile(const std::filesystem::path& filePath)
{
    if (std::filesystem::exists(filePath))
        std::filesystem::remove(filePath);
}
