#pragma once
#include "AbstractSyntaxTree.h"
#include "ParseTree.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH() // Hides error in lexy
#include <fmt/std.h> // fmt::format(std::filesystem::path)b
#include <lexy/action/parse.hpp> // lexy::parse
#include <lexy/callback.hpp> // value callbacks
#include <lexy/dsl.hpp> // lexy::dsl::*
#include <lexy/input/file.hpp> // lexy::read_file
#include <lexy_ext/report_error.hpp> // Error handling
// DISABLE_WARNINGS_POP()
#include <algorithm>
#include <filesystem>
#include <iterator>
#include <stack>
#include <stdexcept>
#include <tbx/error_handling.h>
#include <unordered_Map>
#include <variant>

namespace parse {

// Used as search path in include statements.
struct CurrentParseFile {
    ast::Output output;
    std::filesystem::path parentPath;
};
inline std::stack<CurrentParseFile> g_fileParseStack {};
inline std::unordered_map<std::string, int64_t> g_constants {};

namespace dsl = lexy::dsl;
static constexpr auto comment = LEXY_LIT("//") >> dsl::while_(dsl::code_point - dsl::ascii::newline);
static constexpr auto ws = dsl::whitespace(dsl::ascii::blank | dsl::ascii::space | dsl::newline | comment);

// https://foonathan.net/lexy/tutorial.html#_parsing_the_package_config
// lexy::token_production => no whitespace inside

// using VariableDeclaration = std::variant<ConstantDeclaration, ResourceDeclaration>;

template <typename V, typename T>
constexpr auto constructVariant = lexy::callback<V>(
    [](auto... args) -> V {
        return T { args... };
    });
template <typename T>
std::vector<T> copyIfVariant(const auto& inVector)
{
    std::vector<T> out;
    for (const auto& variant : inVector) {
        if (std::holds_alternative<T>(variant))
            out.push_back(std::get<T>(variant));
    }
    return out;
}

struct string {
    static constexpr auto rule = dsl::quoted(dsl::code_point);
    static constexpr auto value = lexy::as_string<std::string>;
};
struct name : lexy::token_production {
    static constexpr auto rule = dsl::identifier(dsl::ascii::alpha_underscore, dsl::ascii::alpha_digit_underscore);
    static constexpr auto value = lexy::as_string<std::string>;
};
struct shader_stage {
    static constexpr auto rule = dsl::p<name>;
    static constexpr auto value = lexy::callback<ast::ShaderStage>(
        [](const std::string& shaderStageString) -> ast::ShaderStage {
            if (shaderStageString == "vertex")
                return ast::ShaderStage::Vertex;
            else if (shaderStageString == "geometry")
                return ast::ShaderStage::Geometry;
            else if (shaderStageString == "fragment" || shaderStageString == "pixel")
                return ast::ShaderStage::Pixel;
            else if (shaderStageString == "compute")
                return ast::ShaderStage::Compute;
            else if (shaderStageString == "rt" || shaderStageString == "raytracing")
                return ast::ShaderStage::RayTracing;
            else
                throw std::runtime_error("Unknown shader stage");
        });
};

struct bind_point_definition {
    static constexpr auto rule = LEXY_LIT("BindPoint") >> dsl::p<name> + dsl::lit_c<'{'> + dsl::lit_c<'}'> + dsl::lit_c<';'>;
    static constexpr auto value = constructVariant<parse::Statement, ast::BindPoint>;
};

using ShaderInputLayoutDeclaration = std::variant<ast::BindPointReference, ast::RootConstant, ast::RootConstantBufferView, ast::StaticSampler>;
struct shader_input_layout_shader_stages {
    static constexpr auto rule = LEXY_LIT(".shaderStages") >> dsl::lit_c<'='> + dsl::lit_c<'['> + dsl::list(dsl::peek_not(dsl::lit_c<','> / dsl::lit_c<']'>) >> dsl::p<shader_stage>, dsl::sep(dsl::lit_c<','>)) + dsl::lit_c<']'>;
    static constexpr auto value = lexy::as_list<std::vector<ast::ShaderStage>>;
};
struct shader_input_layout_bind_point {
    static constexpr auto rule = dsl::p<name> + dsl::p<name> + dsl::lit_c<'{'> + dsl::p<shader_input_layout_shader_stages> + dsl::lit_c<'}'>;
    static constexpr auto value = constructVariant<ShaderInputLayoutDeclaration, ast::BindPointReference>;
};
struct shader_input_layout_root_constant_num_32_bit_values {
    static constexpr auto rule = LEXY_LIT(".num32BitValues") >> dsl::lit_c<'='> + dsl::integer<uint32_t>;
    static constexpr auto value = lexy::as_integer<uint32_t>;
};
struct shader_input_layout_root_constant {
    static constexpr auto rule = LEXY_LIT("RootConstant") >> dsl::p<name> + dsl::lit_c<'{'> + dsl::p<shader_input_layout_shader_stages> + dsl::lit_c<','> + dsl::p<shader_input_layout_root_constant_num_32_bit_values> + dsl::lit_c<'}'>;
    static constexpr auto value = constructVariant<ShaderInputLayoutDeclaration, ast::RootConstant>;
};
struct shader_input_layout_root_constant_buffer_view {
    static constexpr auto rule = LEXY_LIT("RootCBV") >> dsl::p<name> + dsl::lit_c<'{'> + dsl::p<shader_input_layout_shader_stages> + dsl::lit_c<'}'>;
    static constexpr auto value = constructVariant<ShaderInputLayoutDeclaration, ast::RootConstantBufferView>;
};
struct shader_input_layout_StaticSampler_list_item {
    static constexpr auto rule = dsl::lit_c<'.'> + dsl::p<name> + dsl::lit_c<'='> + dsl::p<string>;
    static constexpr auto value = lexy::construct<std::pair<std::string, std::string>>;
};
struct shader_input_layout_StaticSampler_list {
    static constexpr auto rule = dsl::list(dsl::p<shader_input_layout_StaticSampler_list_item>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::as_list<std::vector<std::pair<std::string, std::string>>>;
};
struct shader_input_layout_StaticSampler {
    static constexpr auto rule = []() {
        const auto declaration = LEXY_LIT("StaticSampler") >> dsl::p<name>;
        const auto definition = dsl::lit_c<'{'> + dsl::p<shader_input_layout_StaticSampler_list> + dsl::lit_c<'}'>;
        return declaration + definition;
    }();
    static constexpr auto value = lexy::callback<ShaderInputLayoutDeclaration>(
        [](const auto& name, const std::vector<std::pair<std::string, std::string>>& options) {
            ast::StaticSampler staticSampler { .name = name };
            for (const auto& [key, value] : options)
                staticSampler.options[key] = value;
            return staticSampler;
        });
    // static constexpr auto value = constructVariant<SILBodyDeclaration, ast::StaticSampler>;
};
struct shader_input_layout_definition_body {
    // clang-format off
    static constexpr auto rule = dsl::list(
        (dsl::peek(LEXY_LIT("StaticSampler")) >> dsl::p<shader_input_layout_StaticSampler> + dsl::lit_c<';'>) |
        (dsl::peek(LEXY_LIT("RootConstant")) >> dsl::p<shader_input_layout_root_constant> + dsl::lit_c<';'>) |
        (dsl::peek(LEXY_LIT("RootCBV")) >> dsl::p<shader_input_layout_root_constant_buffer_view> + dsl::lit_c<';'>) |
        (dsl::peek(dsl::ascii::alpha) >> dsl::p<shader_input_layout_bind_point> + dsl::lit_c<';'>));
    // clang-format on
    static constexpr auto value = lexy::as_list<std::vector<ShaderInputLayoutDeclaration>>;
};
struct shader_input_layout_option_local {
    static constexpr auto rule = LEXY_LIT("Local");
    static constexpr auto value = lexy::callback<bool>([]() { return true; });
};
struct shader_input_layout_options {
    // https://foonathan.net/lexy/tutorial.html
    static constexpr auto rule = []() {
        // const auto make_field = [](auto name, auto rule) {
        //     return name >> dsl::lit_c<'='> + rule;
        // };
        auto local_field = LEXY_MEM(localRootSignature) = dsl::p<shader_input_layout_option_local>;
        // auto shader_stages_field = make_field(LEXY_LIT("ShaderStages"), LEXY_MEM(shaderStages) = dsl::p<shader_input_group_option_shader_stages>);
        // return dsl::lit_c<'<'> + dsl::combination(bind_to_field, shader_stages_field) + dsl::lit_c<'>'>;
        return dsl::lit_c<'<'> >> local_field + dsl::lit_c<'>'>;
    }();
    static constexpr auto value = lexy::as_aggregate<ast::ShaderInputLayoutOptions>;
};
struct shader_input_layout_definition {
    static constexpr auto rule = []() {
        auto definition = LEXY_LIT("ShaderInputLayout") >> dsl::p<name> + dsl::opt(dsl::p<shader_input_layout_options>);
        auto body = dsl::lit_c<'{'> + dsl::p<shader_input_layout_definition_body> + dsl::lit_c<'}'> + dsl::lit_c<';'>;
        return definition + body;
    }();
    static constexpr auto value = lexy::callback<parse::Statement>(
        []<typename T>(const auto& name, const T& options, const std::vector<ShaderInputLayoutDeclaration>& declarations) {
            ast::ShaderInputLayout inputLayout {
                .name = name,
                .bindPoints = copyIfVariant<ast::BindPointReference>(declarations),
                .rootConstants = copyIfVariant<ast::RootConstant>(declarations),
                .rootConstantBufferViews = copyIfVariant<ast::RootConstantBufferView>(declarations),
                .staticSamplers = copyIfVariant<ast::StaticSampler>(declarations)
            };
            // lexy::nullopt if no options are provided.
            if constexpr (std::is_same_v<T, ast::ShaderInputLayoutOptions>)
                inputLayout.options = options;
            return inputLayout;
        });
};

struct variable_Texture2D {
    static constexpr auto rule = LEXY_LIT("Texture2D") >> dsl::lit_c<'<'> + dsl::p<name> + dsl::lit_c<'>'>;
    static constexpr auto value = lexy::callback<ast::VariableType>(
        [](const auto& textureType) {
            return ast::Texture2D {
                .textureType = textureType
            };
        });
};
struct variable_RWTexture2D {
    static constexpr auto rule = LEXY_LIT("RWTexture2D") >> dsl::lit_c<'<'> + dsl::p<name> + dsl::lit_c<'>'>;
    static constexpr auto value = lexy::callback<ast::VariableType>(
        [](const auto& textureType) {
            return ast::RWTexture2D {
                .textureType = textureType
            };
        });
};
struct variable_ByteAddressBuffer {
    static constexpr auto rule = LEXY_LIT("ByteAddressBuffer");
    static constexpr auto value = lexy::callback<ast::VariableType>(
        []() {
            return ast::ByteAddressBuffer {};
        });
};
struct variable_RWByteAddressBuffer {
    static constexpr auto rule = LEXY_LIT("RWByteAddressBuffer");
    static constexpr auto value = lexy::callback<ast::VariableType>(
        []() {
            return ast::RWByteAddressBuffer {};
        });
};
struct variable_StructuredBuffer {
    static constexpr auto rule = LEXY_LIT("StructuredBuffer") >> dsl::lit_c<'<'> + dsl::p<name> + dsl::lit_c<'>'>;
    static constexpr auto value = lexy::callback<ast::VariableType>(
        [](const auto& structureType) {
            return ast::StructuredBuffer {
                .dataType = ast::UnresolvedType { structureType }
            };
        });
};
struct variable_RWStructuredBuffer {
    static constexpr auto rule = LEXY_LIT("RWStructuredBuffer") >> dsl::lit_c<'<'> + dsl::p<name> + dsl::lit_c<'>'>;
    static constexpr auto value = lexy::callback<ast::VariableType>(
        [](const auto& structureType) {
            return ast::RWStructuredBuffer {
                .dataType = ast::UnresolvedType { structureType }
            };
        });
};
struct variable_RayTracingAccelerationStructure {
    static constexpr auto rule = LEXY_LIT("RaytracingAccelerationStructure");
    static constexpr auto value = lexy::callback<ast::VariableType>(
        []() {
            return ast::RaytracingAccelerationStructure {};
        });
};
struct variable_count_opt_uint {
    static constexpr auto digits_rule = dsl::peek(dsl::digits<>) >> dsl::integer<uint32_t>(dsl::digits<>);
    static constexpr auto constant_rule = dsl::p<name>;
    static constexpr auto rule = dsl::opt(digits_rule | constant_rule);
    static constexpr auto value = lexy::callback<uint32_t>(
        [](lexy::nullopt) { return ast::Variable::Unbounded; },
        [](uint32_t count) { return count; },
        [](std::string constantName) {
            const auto iter = g_constants.find(constantName);
            if (iter == std::end(g_constants)) {
                spdlog::error("Encountered undefined constant {}", constantName);
                exit(1);
            }
            return uint32_t(iter->second);
        });
};
struct variable_count {
    static constexpr auto rule = dsl::opt(dsl::lit_c<'['> >> dsl::p<variable_count_opt_uint> + dsl::lit_c<']'>);
    static constexpr auto value = lexy::callback<uint32_t>(
        [](lexy::nullopt) { return 0; },
        [](uint32_t count) { return count; });
};
struct variable_declaration {
    static constexpr auto rule = []() {
        const auto constantType = dsl::p<name>;
        const auto texture2D = dsl::p<variable_Texture2D>;
        const auto rwTexture2D = dsl::p<variable_RWTexture2D>;
        const auto byteAddressBuffer = dsl::p<variable_ByteAddressBuffer>;
        const auto rwByteAddressBuffer = dsl::p<variable_RWByteAddressBuffer>;
        const auto structuredBuffer = dsl::p<variable_StructuredBuffer>;
        const auto rwStructuredBuffer = dsl::p<variable_RWStructuredBuffer>;
        const auto rtAccel = dsl::p<variable_RayTracingAccelerationStructure>;

        const auto variableType = (texture2D | rwTexture2D | byteAddressBuffer | rwByteAddressBuffer | structuredBuffer | rwStructuredBuffer | rtAccel | dsl::else_ >> constantType);
        return variableType + dsl::p<name> + dsl::p<variable_count> + dsl::lit_c<';'>;
    }();
    static constexpr auto value = lexy::callback<ast::Variable>(
        [](const ast::VariableType& variableType, const auto& name, uint32_t count) {
            return ast::Variable {
                .name = name,
                .type = variableType,
                .arrayCount = count
            };
        },
        [](const std::string& typeName, const auto& name, uint32_t count) {
            const ast::UnresolvedType astType {
                .typeName = typeName
            };
            return ast::Variable {
                .name = name,
                .type = ast::UnresolvedType { astType },
                .arrayCount = count
            };
        });
};

struct group_definition_body {
    static constexpr auto rule = dsl::list(dsl::peek_not(dsl::lit_c<'}'>) >> dsl::p<variable_declaration>);
    static constexpr auto value = lexy::as_list<std::vector<ast::Variable>>;
};
struct group_definition {
    static constexpr auto rule = []() {
        const auto declaration = LEXY_LIT("Group") >> dsl::p<name>;
        const auto body = dsl::lit_c<'{'> + dsl::p<group_definition_body> + dsl::lit_c<'}'> + dsl::lit_c<';'>;
        return declaration + body;
    }();
    static constexpr auto value = lexy::callback<parse::Statement>(
        [](const auto& name, const std::vector<ast::Variable>& variableDeclarations) {
            return ast::Group { .name = name, .variables = variableDeclarations };
        });
};

struct shader_input_group_definition_body {
    static constexpr auto rule = dsl::list(dsl::peek_not(dsl::lit_c<'}'>) >> dsl::p<variable_declaration>);
    static constexpr auto value = lexy::as_list<std::vector<ast::Variable>>;
};
struct shader_input_group_definition {
    static constexpr auto rule = []() {
        const auto declaration = LEXY_LIT("ShaderInputGroup") >> dsl::p<name> + dsl::lit_c<'<'> + LEXY_LIT("BindTo") + dsl::lit_c<'='> + dsl::p<name> + dsl::lit_c<'>'>;
        const auto body = dsl::lit_c<'{'> + dsl::p<shader_input_group_definition_body> + dsl::lit_c<'}'> + dsl::lit_c<';'>;
        return declaration + body;
    }();
    static constexpr auto value = lexy::callback<parse::Statement>(
        [](const auto& name, const auto& bindPointName, const std::vector<ast::Variable>& variableDeclarations) {
            ast::ShaderInputGroup shaderInputGroup {
                .name = name,
                .bindPointName = bindPointName,
            };
            std::copy(std::begin(variableDeclarations), std::end(variableDeclarations), std::back_inserter(shaderInputGroup.variables));
            return shaderInputGroup;
        });
};

struct struct_variable_declaration {
    static constexpr auto rule = []() {
        return dsl::p<name> + dsl::p<name> + dsl::p<variable_count> + dsl::lit_c<';'>;
    }();
    static constexpr auto value = lexy::callback<ast::Variable>(
        [](const std::string& type, const std::string& name, uint32_t count) {
            return ast::Variable {
                .name = name,
                .type = ast::UnresolvedType { type },
                .arrayCount = count
            };
        });
};
struct shader_struct_definition_body {
    static constexpr auto rule = dsl::list(dsl::peek_not(dsl::lit_c<'}'>) >> dsl::p<struct_variable_declaration>);
    static constexpr auto value = lexy::as_list<std::vector<ast::Variable>>;
};
struct shader_struct_definition {
    static constexpr auto rule = []() {
        auto definition = LEXY_LIT("struct") >> dsl::p<name>;
        auto body = dsl::lit_c<'{'> + dsl::p<shader_struct_definition_body> + dsl::lit_c<'}'> + dsl::lit_c<';'>;
        return definition + body;
    }();
    // static constexpr auto value = constructVariant<parse::Statement, ast::Struct>;
    static constexpr auto value = lexy::callback<parse::Statement>(
        [](const auto& name, const std::vector<ast::Variable>& variableDeclarations) {
            return ast::Struct {
                .name = name,
                .variables = variableDeclarations
            };
        });
};

ParseTree parseShaderInputFile(const std::filesystem::path& filePath);
struct include_statement {
    static constexpr auto rule = []() {
        const auto filePath = dsl::identifier(dsl::ascii::alpha_digit_underscore / dsl::lit_c<'.'> / dsl::lit_c<'/'>);
        return LEXY_LIT("#include") >> dsl::lit_c<'"'> + filePath + dsl::lit_c<'"'>;
    }();
    static constexpr auto value = lexy::callback<Statement>(
        [](auto lexeme) {
            const std::string fileName { std::begin(lexeme), std::end(lexeme) };
            return std::make_unique<ParseTree>(parseShaderInputFile(g_fileParseStack.top().parentPath / fileName));
        });
};
struct Dummy { };
struct output_statement {
    static constexpr auto rule = []() {
        const auto symbols = dsl::ascii::alpha_digit_underscore / dsl::lit_c<'.'> / dsl::lit_c<'/'>;
        const auto filePath = dsl::identifier(symbols, symbols);
        return LEXY_LIT("#output") >> LEXY_LIT("\"") + filePath + dsl::lit_c<'"'> + ws + dsl::lit_c<'"'> + filePath + dsl::lit_c<'"'>;
    }();
    static constexpr auto value = lexy::callback<Dummy>(
        [](auto lexemeCpp, auto lexemeShader) {
            // Move implementation to separate function because it needs entry point which is defined *after* this class.
            const auto& baseFilePath = g_fileParseStack.top().parentPath;
            const std::string cppRelativeFolder { std::begin(lexemeCpp), std::end(lexemeCpp) };
            const std::string shaderRelativeFolder { std::begin(lexemeShader), std::end(lexemeShader) };
            auto& output = g_fileParseStack.top().output;
            output.cppFolder = std::filesystem::absolute(baseFilePath / cppRelativeFolder);
            output.shaderFolder = std::filesystem::absolute(baseFilePath / shaderRelativeFolder);
            output.shouldExport = (g_fileParseStack.size() == 1);
            return Dummy {};
        });
};
struct constant_definition {
    static constexpr auto rule = LEXY_LIT("#constant") >> dsl::p<name> + dsl::integer<int64_t>;
    static constexpr auto value = lexy::callback<parse::Statement>(
        [](std::string name, int64_t value) {
            g_constants[name] = value;
            return ast::Constant { .name = name, .value = value };
        });
};
struct top_level_statement {
    static constexpr auto rule = dsl::p<include_statement> | dsl::p<bind_point_definition> | dsl::p<shader_input_layout_definition> | dsl::p<group_definition> | dsl::p<shader_input_group_definition> | dsl::p<shader_struct_definition> | dsl::p<constant_definition>;
    static constexpr auto value = lexy::forward<Statement>;
};
struct top_level_statements {
    static constexpr auto rule = dsl::list(dsl::peek_not(dsl::eof) >> dsl::p<top_level_statement>);
    static constexpr auto value = lexy::as_list<std::vector<Statement>>;
};
struct entrypoint {
    static constexpr auto whitespace = dsl::ascii::blank | dsl::ascii::space | dsl::newline | comment;
    static constexpr auto rule = dsl::opt(dsl::p<output_statement>) + dsl::p<top_level_statements>;
    static constexpr auto value = lexy::callback<ParseTree>(
        [](auto /* output */, std::vector<Statement> statements) {
            return ParseTree { .statements = std::move(statements) };
        });
};

inline ParseTree parseShaderInputFile(const std::filesystem::path& filePath)
{
    if (!std::filesystem::exists(filePath))
        throw std::runtime_error(fmt::format("'{}' file not found", filePath));

    const auto filePathString = filePath.string();
    const auto file = lexy::read_file<lexy::utf8_encoding>(filePathString.c_str());
    if (!file)
        throw std::runtime_error(fmt::format("error when reading '{}'", filePath));

    if (g_fileParseStack.empty()) {
        g_fileParseStack.push(CurrentParseFile {
            .output = ast::Output {
                .shouldExport = true },
            .parentPath = filePath.parent_path() });
    } else {
        auto parseFile = g_fileParseStack.top();
        parseFile.parentPath = filePath.parent_path();
        g_fileParseStack.push(parseFile);
    }
    ParseTree out = lexy::parse<entrypoint>(file.buffer(), lexy_ext::report_error.path("")).value();
    out.output = g_fileParseStack.top().output;
    g_fileParseStack.pop();
    return out;
}

}

DISABLE_WARNINGS_POP()
