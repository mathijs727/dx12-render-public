#pragma once
#include <cstddef>
#include <filesystem>
#include <vector>

namespace Util {

std::vector<std::byte> readFile(const std::filesystem::path& filePath);

}
