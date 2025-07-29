#pragma once
#include "Engine/Util/DirectoryChangeWatcher.h"
#include <filesystem>
#include <string>
#include <thread>

namespace Render {

class ShaderHotReload {
public:
    ShaderHotReload(
        const std::filesystem::path& shaderSourceFolder,
        const std::filesystem::path& shaderBinaryFolder,
        const std::filesystem::path& buildFolder,
        const std::string& cmakeShaderTargetName);

    bool shouldReloadShaders();

private:
    Util::DirectoryChangeWatcher m_shaderBinaryWatcher;
    std::jthread m_shaderCompileThread;
};

}