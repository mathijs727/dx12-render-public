#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
DISABLE_WARNINGS_POP()
#include <Engine/Render/Scene.h>
#include <filesystem>
#include <iostream>
#include <tbx/error_handling.h>

int main()
{
    Tbx::assert_always(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));

    std::filesystem::path inFile, outFile;
    CLI::App app { "Add engine optimized texture & mesh representations" };
    app.add_option("in", inFile, "Input GLTF/GLB file")->required();
    app.add_option("out", outFile, "Output binary file")->required();
    try {
        app.parse(__argc, __argv);
    } catch (const CLI::ParseError& e) {
        app.exit(e);
    }

    if (inFile.extension() == ".gltf") {
        Render::Scene::gltf2binary(inFile, outFile);
    } else if (inFile.extension() == ".glb") {
        Render::Scene::glb2binary(inFile, outFile);
    } else {
        std::cerr << "Unsupported file extension " << inFile.extension() << std::endl;
        return 1;
    }

    return 0;
}
