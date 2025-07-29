#include "Engine/Util/FilePicker.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <nfd.h>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <array>

namespace Util {
std::optional<std::filesystem::path> pickSaveFile(const FileFilterListItem& filter)
{
    return pickSaveFile(std::array { filter });
}
std::optional<std::filesystem::path> pickSaveFile(std::span<const FileFilterListItem> filterList)
{
    std::vector<nfdu8filteritem_t> filterListC;
    for (const auto& filterListItem : filterList)
        filterListC.push_back({ filterListItem.name.c_str(), filterListItem.fileTypes.c_str() });

    nfdu8char_t* pOutPath = nullptr;
    const nfdresult_t result = NFD_SaveDialogU8(&pOutPath, filterListC.data(), (nfdfiltersize_t)filterListC.size(), nullptr, nullptr);

    if (result == NFD_OKAY) {
        std::filesystem::path outPath { pOutPath };
        free(pOutPath);
        return outPath;
    } else if (result != NFD_CANCEL) {
        spdlog::error("Native file dialog error: {}", NFD_GetError());
    }

    return {};
}

std::optional<std::filesystem::path> pickOpenFile(const FileFilterListItem& filter)
{
    return pickOpenFile(std::array { filter });
}

std::optional<std::filesystem::path> pickOpenFile(std::span<const FileFilterListItem> filterList)
{
    std::vector<nfdu8filteritem_t> filterListC;
    for (const auto& filterListItem : filterList)
        filterListC.push_back({ filterListItem.name.c_str(), filterListItem.fileTypes.c_str() });

    nfdu8char_t* pOutPath = nullptr;
    const nfdresult_t result = NFD_OpenDialog(&pOutPath, filterListC.data(), (nfdfiltersize_t)filterListC.size(), nullptr);

    if (result == NFD_OKAY) {
        std::filesystem::path outPath { pOutPath };
        free(pOutPath);
        return outPath;
    } else if (result != NFD_CANCEL) {
        spdlog::error("Native file dialog error: {}", NFD_GetError());
    }

    return {};
}

}
