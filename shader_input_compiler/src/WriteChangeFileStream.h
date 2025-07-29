#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Only write to a file if there are any changes.
class WriteChangeFileStream : public std::ostream {
public:
    WriteChangeFileStream(const std::filesystem::path& filePath)
        : std::ostream(new std::stringbuf())
        , m_filePath(filePath)
    {
    }
    ~WriteChangeFileStream()
    {
        auto* pStringBuf = dynamic_cast<std::stringbuf*>(rdbuf());
        auto str = pStringBuf->str();
        if (std::filesystem::exists(m_filePath)) {
            std::ifstream originalFile { m_filePath };
            std::string originalFileContents { (std::istreambuf_iterator<char>(originalFile)), (std::istreambuf_iterator<char>()) };
            if (originalFileContents.size() == str.size() && std::equal(std::begin(str), std::end(str), std::begin(originalFileContents))) {
                delete rdbuf();
                return;
            }
        }

        std::ofstream outFile { m_filePath };
        outFile << str;
        delete rdbuf();
    }

private:
    std::filesystem::path m_filePath;
};
