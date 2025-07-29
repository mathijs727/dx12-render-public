#pragma once
#include <filesystem>
#include <optional>
#include <span>
#include <string.h>

namespace Util {

struct FileFilterListItem {
    std::string name;
    std::string fileTypes;
};

[[nodiscard]] std::optional<std::filesystem::path> pickSaveFile(const FileFilterListItem& filter);
[[nodiscard]] std::optional<std::filesystem::path> pickSaveFile(std::span<const FileFilterListItem> filterList = {});

[[nodiscard]] std::optional<std::filesystem::path> pickOpenFile(const FileFilterListItem& filter);
[[nodiscard]] std::optional<std::filesystem::path> pickOpenFile(std::span<const FileFilterListItem> filterList = {});

}
