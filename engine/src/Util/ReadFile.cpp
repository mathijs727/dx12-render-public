#include "Engine/Util/ReadFile.h"
#include <cassert>
#include <fstream>

namespace Util {

// https://stackoverflow.com/questions/15366319/how-to-read-the-binary-file-in-c
std::vector<std::byte> readFile(const std::filesystem::path& filePath)
{
    // open the file:
    assert(std::filesystem::exists(filePath));
    std::ifstream file(filePath, std::ios::binary);

    // get its size:
    file.seekg(0, std::ios::end);
    const std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // read the data:
    std::vector<std::byte> fileData(fileSize);
    file.read((char*)&fileData[0], fileSize);
    return fileData;
}

}
