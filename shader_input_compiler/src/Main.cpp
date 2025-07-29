#include "AbstractSyntaxTree.h" // for Output, Abstrac...
#include "Parse.h" // for parseShaderInpu...
#include "ParseTree.h" // for ParseTree
#include "backends/dx12-render/GenerateDeviceCode.h" // for generateDeviceCode
#include "backends/dx12-render/GenerateHostCode.h" // for generateHostCode
#include "backends/dx12-render/RegisterAllocation.h" // for allocateRegisters
#include <tbx/disable_all_warnings.h> // for DISABLE_WARNING...
DISABLE_WARNINGS_PUSH()
#include <CLI/App.hpp> // for App
#include <CLI/Error.hpp> // for ParseError
#include <CLI/Option.hpp> // for Option
DISABLE_WARNINGS_POP()
#include <cstdlib> // for exit
#include <exception> // for exception
#include <filesystem> // for operator<<, path
#include <iostream> // for char_traits

const std::filesystem::path baseDir { BASE_DIR };

struct AppArguments {
    std::filesystem::path inputFile;
};

// static std::string snakeCase(std::string str);
static AppArguments parseApplicationArguments(int argc, const char** ppArgv);

int main(int argc, const char** ppArgv)
{
    const auto args = parseApplicationArguments(argc, ppArgv);

    const parse::ParseTree parseTree = parse::parseShaderInputFile(args.inputFile);
    if (parseTree.output.cppFolder.empty() && parseTree.output.shaderFolder.empty()) {
        std::cerr << "No #output specified in file \"" << args.inputFile << "\"!" << std::endl;
        return -1;
    }

    try {
        ast::AbstractSyntaxTree tree = parseTree.toAbstractSyntaxTree();
        const auto resourceBindingInfo = dx12_render::allocateRegisters(tree);
        generateDeviceCode(tree, resourceBindingInfo);
        generateHostCode(tree, resourceBindingInfo);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
    return 0;
}

static AppArguments parseApplicationArguments(int argc, const char** ppArgv)
{
    AppArguments out {};

    CLI::App app { "Convert *.si files into *.cpp and *.hlsl files" };
    app.add_option("file", out.inputFile, "Path of input file")->required();
    try {
        app.parse(argc, ppArgv);
    } catch (const CLI::ParseError& e) {
        app.exit(e);
        exit(1);
    }
    return out;
}
