#pragma once
#include <filesystem>

namespace Util {

class TmpDir {
public:
    TmpDir(const std::filesystem::path& directory = std::filesystem::temp_directory_path());
    ~TmpDir();

    operator std::filesystem::path() const;

private:
    std::filesystem::path m_path;
};

}
