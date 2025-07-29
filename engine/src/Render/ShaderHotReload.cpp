#include "Engine/Render/ShaderHotReload.h"
#include <chrono>
#include <cstdlib>
#include <format>

namespace Render {

ShaderHotReload::ShaderHotReload(
    const std::filesystem::path& shaderSourceFolder,
    const std::filesystem::path& shaderBinaryFolder,
    const std::filesystem::path& buildFolder,
    const std::string& cmakeShaderTargetName)
    : m_shaderBinaryWatcher(shaderBinaryFolder, std::chrono::milliseconds(500))
{
    m_shaderCompileThread = std::jthread([=](std::stop_token stopToken) {
        Util::DirectoryChangeWatcher directoryChangeWatcher(shaderSourceFolder, std::chrono::milliseconds(250));
        // https://stackoverflow.com/questions/9964865/c-system-not-working-when-there-are-spaces-in-two-different-parameters
        const std::string compileCommand = std::format("\"\"{}\" --build \"{}\" --target {}\"", CMAKE_EXECUTABLE, buildFolder.string(), cmakeShaderTargetName);

        while (!stopToken.stop_requested()) {
            // Invoke CMake to recompile the shaders if necessary.
            if (directoryChangeWatcher.hasChanged())
                std::system(compileCommand.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
}

bool ShaderHotReload::shouldReloadShaders()
{
    return m_shaderBinaryWatcher.hasChanged();
}

}