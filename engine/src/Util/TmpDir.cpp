#include "Engine/Util/TmpDir.h"
#include <random>
#include <sstream>

namespace Util {

TmpDir::TmpDir(const std::filesystem::path& parentDirectory)
{
    // https://stackoverflow.com/questions/3379956/how-to-create-a-temporary-directory-in-c
    std::random_device dev;
    std::mt19937 prng(dev());
    std::uniform_int_distribution<uint64_t> rand(0);
    while (true) {
        std::stringstream ss;
        ss << std::hex << rand(prng);
        m_path = parentDirectory / ss.str();
        if (std::filesystem::create_directory(m_path))
            break;
    }
}

TmpDir::~TmpDir()
{
    std::filesystem::remove_all(m_path);
}

TmpDir::operator std::filesystem::path() const
{
    return m_path;
}

}
