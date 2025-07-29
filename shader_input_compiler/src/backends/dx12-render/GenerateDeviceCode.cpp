#include "GenerateDeviceCode.h"
#include "AbstractSyntaxTree.h" // for Variable, Struct, Group, Abstr...
#include "ConstantBuffer.h" // for isCustomConstantVariableType
#include "RegisterAllocation.h" // for BindPointBindings, ResourceBin...
#include "StringManipulation.h" // for notTitle, title
#include "WriteChangeFileStream.h" // for WriteChangeFileStream
#include <tbx/disable_all_warnings.h> // for DISABLE_WARNINGS_POP, DISABLE_...
#include <tbx/string.h>
DISABLE_WARNINGS_PUSH()
#include <cppitertools/enumerate.hpp> // for Enumerable, enumerate
#include <cppitertools/zip.hpp> // for Zipped, zip
#include <fmt/core.h> // for format, format_string
#include <magic_enum/magic_enum.hpp>
DISABLE_WARNINGS_POP()
#include <algorithm> // for replace
#include <cstdint> // for uint32_t
#include <exception> // for exception
#include <filesystem> // for path, operator/, operator<<
#include <fstream> // for char_traits, basic_ostream
#include <memory> // for _Simple_types, allocator, shar...
#include <span> // std::span
#include <stdexcept> // for runtime_error
#include <string> // for operator<<, string, char_traits
#include <tbx/error_handling.h> // for assert_always
#include <tbx/variant_helper.h> // for overload, make_visitor
#include <tuple> // for tuple
#include <utility> // for begin, end
#include <variant> // for visit, get, holds_alternative
#include <vector> // for vector, _Vector_const_iterator

namespace dx12_render {

static void generateDeviceConstants(std::span<const ast::Constant> constants, const std::filesystem::path& filePath);
static void generateDeviceStruct(const ast::Struct& shaderStruct, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& filePath);
static void generateDeviceGroup(const ast::Group& group, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& filePath);
static void generateDeviceShaderInputGroup(const ast::ShaderInputGroup& shaderInputGroup, const ShaderInputGroupBindings& bindings, const ast::ShaderInputLayout& shaderInputLayout, uint32_t rootParameterOffset, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& filePath);
static void generateDeviceShaderInputLayout(const ast::ShaderInputLayout& shaderInputLayout, const ShaderInputLayoutBindings& bindings, const std::filesystem::path& filePath);

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::Struct>& shaderStruct)
{
    return shaderStruct.metadata.shaderFolder / "structs" / fmt::format("{}.hlsl", shaderStruct->name);
}

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::Group>& group)
{
    return group.metadata.shaderFolder / "groups" / fmt::format("{}.hlsl", group->name);
}

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::ShaderInputGroup>& shaderInputGroup, const ast::AbstractSyntaxTree::ItemWithMetadata<ast::ShaderInputLayout>& shaderInputLayout)
{
    return shaderInputLayout.metadata.shaderFolder / "inputgroups" / shaderInputLayout->name / fmt::format("{}.hlsl", shaderInputGroup->name);
}

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::ShaderInputLayout>& shaderInputLayout)
{
    return shaderInputLayout.metadata.shaderFolder / "inputlayouts" / fmt::format("{}.hlsl", shaderInputLayout->name);
}

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::Constant>& constant)
{
    return constant.metadata.shaderFolder / "constants.hlsl";
}

static void createFolderContainingFile(const std::filesystem::path& filePath)
{
    if (!std::filesystem::exists(filePath.parent_path()))
        std::filesystem::create_directories(filePath.parent_path());
}

static std::string getIncludeGuardName(const std::string& name)
{
    auto includeGuardName = name;
    std::replace(std::begin(includeGuardName), std::end(includeGuardName), '.', '_'); // replace all 'x' to 'y
    return "__" + includeGuardName + "__";
}

static void addIncludeGuardStart(const std::string& name, std::ostream& stream)
{
    const auto includeGuardName = getIncludeGuardName(name);
    stream << "#ifndef " << includeGuardName << std::endl;
    stream << "#define " << includeGuardName << std::endl;
}

static void addIncludeGuardEnd(const std::string& name, std::ostream& stream)
{
    const auto includeGuardName = getIncludeGuardName(name);
    stream << "#endif" << std::endl;
}

static void addInclude(const ast::VariableType& type, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& basePath, std::ostream& outStream)
{
    const auto visitor = Tbx::make_visitor(
        [&](const ast::StructInstance& s) {
            outStream << "#include " << std::filesystem::relative(getFilePath(tree.structs[s.structIndex]), basePath) << "\n";
        },
        [&](const ast::GroupInstance& group) {
            outStream << "#include " << std::filesystem::relative(getFilePath(tree.groups[group.groupIndex]), basePath) << "\n";
        },
        [&](const ast::StructuredBuffer& buffer) {
            if (std::holds_alternative<ast::StructInstance>(buffer.dataType))
                outStream << "#include " << std::filesystem::relative(getFilePath(tree.structs[std::get<ast::StructInstance>(buffer.dataType).structIndex]), basePath) << "\n";
        },
        [&](const ast::RWStructuredBuffer& buffer) {
            if (std::holds_alternative<ast::StructInstance>(buffer.dataType))
                outStream << "#include " << std::filesystem::relative(getFilePath(tree.structs[std::get<ast::StructInstance>(buffer.dataType).structIndex]), basePath) << "\n";
        },
        [](auto) {});
    std::visit(visitor, type);
}

void generateDeviceCode(const ast::AbstractSyntaxTree& tree, const ResourceBindingInfo& resourceBindingInfo)
{
    if (!tree.constants.empty()) {
        // Cluster constants based on shader folder.
        auto sortedConstantsWithMetadata = tree.constants;
        std::sort(std::begin(sortedConstantsWithMetadata), std::end(sortedConstantsWithMetadata),
            [](const auto& lhs, const auto& rhs) { return lhs.metadata.shaderFolder < rhs.metadata.shaderFolder; });
        std::vector<ast::Constant> sortedConstants(sortedConstantsWithMetadata.size());
        std::transform(std::begin(sortedConstantsWithMetadata), std::end(sortedConstantsWithMetadata), std::begin(sortedConstants),
            [](const auto& constantWithMetadata) { return constantWithMetadata.item; });
        size_t firstConstantInFile = 0;
        for (size_t lastConstantInFile = 0; lastConstantInFile < sortedConstantsWithMetadata.size(); lastConstantInFile++) {
            if (sortedConstantsWithMetadata[firstConstantInFile].metadata.shaderFolder != sortedConstantsWithMetadata[lastConstantInFile].metadata.shaderFolder) {
                if (sortedConstantsWithMetadata[firstConstantInFile].metadata.shouldExport) {
                    generateDeviceConstants(
                        std::span(sortedConstants).subspan(firstConstantInFile, lastConstantInFile - firstConstantInFile),
                        getFilePath(sortedConstantsWithMetadata[firstConstantInFile]));
                }
                firstConstantInFile = lastConstantInFile;
            }
        }
        if (sortedConstantsWithMetadata[firstConstantInFile].metadata.shouldExport) {
            generateDeviceConstants(
                std::span(sortedConstants).subspan(firstConstantInFile),
                getFilePath(sortedConstantsWithMetadata[firstConstantInFile]));
        }
    }

    for (const auto& shaderStruct : tree.structs) {
        if (shaderStruct.metadata.shouldExport) {
            const auto filePath = getFilePath(shaderStruct);
            createFolderContainingFile(filePath);
            generateDeviceStruct(shaderStruct, tree, filePath);
        }
    }

    for (const auto& group : tree.groups) {
        if (group.metadata.shouldExport) {
            const auto filePath = getFilePath(group);
            createFolderContainingFile(filePath);
            generateDeviceGroup(group, tree, filePath);
        }
    }

    for (const auto& [shaderInputLayout, shaderInputLayoutBindings] : iter::zip(tree.shaderInputLayouts, resourceBindingInfo.shaderInputLayouts)) {
        for (const auto& [bindPointReference, rootParameterIndex] : iter::zip(shaderInputLayout->bindPoints, shaderInputLayoutBindings.bindPointsRootParameterIndices)) {
            const auto& bindPoint = tree.bindPoints[bindPointReference.bindPointIndex];
            const auto& bindPointBindings = resourceBindingInfo.bindPoints[bindPointReference.bindPointIndex];

            for (const auto& [shaderInputGroupIndex, shaderInputGroupBindings] : iter::zip(bindPoint->shaderInputGroups, bindPointBindings.shaderInputGroups)) {
                const auto& shaderInputGroup = tree.shaderInputGroups[shaderInputGroupIndex];

                if (shaderInputGroup.metadata.shouldExport) {
                    const auto filePath = getFilePath(shaderInputGroup, shaderInputLayout);
                    createFolderContainingFile(filePath);

                    generateDeviceShaderInputGroup(shaderInputGroup, shaderInputGroupBindings, shaderInputLayout, rootParameterIndex, tree, filePath);
                }
            }
        }

        if (shaderInputLayout.metadata.shouldExport) {
            const auto filePath = getFilePath(shaderInputLayout);
            createFolderContainingFile(filePath);
            generateDeviceShaderInputLayout(shaderInputLayout, shaderInputLayoutBindings, filePath);
        }
    }
}

static char registerTypeChar(const ast::VariableType& type);
static std::string typeName(const ast::VariableType& type, const ast::AbstractSyntaxTree& tree);

void generateDeviceConstants(std::span<const ast::Constant> constants, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    addIncludeGuardStart("CONSTANTS", stream);
    for (const auto& constant : constants) {
        stream << "#define " << constant.name << " " << constant.value << std::endl;
    }
    addIncludeGuardEnd("CONSTANTS", stream);
}

static void generateDeviceStruct(const ast::Struct& shaderStruct, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    addIncludeGuardStart(shaderStruct.name, stream);

    // Add include statements.
    const auto basePath = filePath.parent_path();
    for (const auto& variable : shaderStruct.variables)
        addInclude(variable.type, tree, basePath, stream);

    stream << "struct " << shaderStruct.name << " {" << std::endl;
    for (const auto& variable : shaderStruct.variables) {
        stream << "\t" << typeName(variable.type, tree) << " " << variable.name;
        if (variable.arrayCount != 0)
            stream << "[" << variable.arrayCount << "]";
        stream << ";" << std::endl;
    }
    stream << "};" << std::endl;

    addIncludeGuardEnd(shaderStruct.name, stream);
}

void generateDeviceGroup(const ast::Group& group, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    addIncludeGuardStart(group.name, stream);

    // Add include statements.
    const auto basePath = filePath.parent_path();
    for (const auto& variable : group.variables)
        addInclude(variable.type, tree, basePath, stream);

    stream << "struct " << group.name << " {" << std::endl;
    for (const auto& variable : group.variables) {
        // Group may not contain unbounded arrays.
        if (variable.arrayCount == ast::Variable::Unbounded)
            throw std::runtime_error(fmt::format("Unbounded array `{}` in Group `{}` is not allowed", variable.name, group.name));

        stream << "\t" << typeName(variable.type, tree) << " " << variable.name;
        if (variable.arrayCount > 0) {
            stream << "[" << variable.arrayCount << "]";
        }
        stream << ";" << std::endl;
    }
    stream << "};" << std::endl;

    addIncludeGuardEnd(group.name, stream);
}

static void generateDeviceShaderInputGroup(
    const ast::ShaderInputGroup& shaderInputGroup, const ShaderInputGroupBindings& resourceBinding,
    const ast::ShaderInputLayout& shaderInputLayout, uint32_t rootParameterOffset,
    const ast::AbstractSyntaxTree& tree, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    addIncludeGuardStart(shaderInputGroup.name, stream);

    // Add include statements.
    const auto basePath = filePath.parent_path();
    for (const auto& variable : shaderInputGroup.variables)
        addInclude(variable.type, tree, basePath, stream);

    for (const auto& [rootParameterBaseIdx, rootParameter] : iter::enumerate(resourceBinding.rootParameters)) {
        const auto rootParameterIdx = rootParameterBaseIdx + rootParameterOffset;
        const auto shaderRegisterSpace = rootParameterIdx + (shaderInputLayout.options.localRootSignature ? 500 : 0);
        for (const auto& descriptor : rootParameter.descriptorTable.descriptors) {
            const auto shaderBaseRegister = descriptor.descriptorOffset;

            const auto& variable = shaderInputGroup.variables[descriptor.variableIdx];
            if (isCustomConstantVariableType(variable.type)) {
                const auto& pConstantBuffer = getConstantBufferVariableType(variable.type);

                stream << "cbuffer CONSTANT_DATA : register(b" << shaderBaseRegister << ", space" << shaderRegisterSpace << ") {" << std::endl;
                for (const auto& constVariable : shaderInputGroup.variables) {
                    if (isStandardContantVariableType(constVariable.type)) {
                        stream << "\t" << typeName(constVariable.type, tree) << " _" << constVariable.name;
                        if (constVariable.arrayCount != 0)
                            stream << "[" << constVariable.arrayCount << "]";
                        stream << ";" << std::endl;
                    }
                }
                stream << "};" << std::endl;
            } else {
                stream << typeName(variable.type, tree) << " _" << variable.name;
                if (variable.arrayCount == ast::Variable::Unbounded)
                    stream << "[]";
                else if (variable.arrayCount != 0)
                    stream << "[" << variable.arrayCount << "]";
                stream << " : register(" << registerTypeChar(variable.type) << shaderBaseRegister;
                stream << ", space" << shaderRegisterSpace << ");" << std::endl;
            }
        }
    }

    // Write wrapper class with getters for the variables.
    stream << "class " << shaderInputGroup.name << " {" << std::endl;
    for (const auto& variable : shaderInputGroup.variables) {
        if (isCustomConstantVariableType(variable.type))
            continue;

        if (std::holds_alternative<ast::GroupInstance>(variable.type)) {
            const ast::Group& group = tree.groups[std::get<ast::GroupInstance>(variable.type).groupIndex];
            stream << "\t" << typeName(variable.type, tree) << " get" << title(variable.name) << "() {" << std::endl;
            stream << "\t\t" << group.name << " outGroup;" << std::endl;
            for (const auto& groupVariable : group.variables) {
                const auto mangledVariableName = getGroupVariableMangledName(variable.name, groupVariable.name);
                stream << "\t\toutGroup." << groupVariable.name << " = get" << title(mangledVariableName) << "();" << std::endl;
            }
            stream << "\t\treturn outGroup;" << std::endl;
            stream << ";\n\t}" << std::endl;
            continue;
        }

        // Regular variable.
        stream << "\t" << typeName(variable.type, tree) << " get" << title(variable.name) << "(";
        if (variable.arrayCount != 0)
            stream << "int idx";
        stream << ") {" << std::endl;

        stream << "\t\treturn _" << variable.name;
        if (variable.arrayCount != 0)
            stream << "[idx]";
        stream << ";\n\t}" << std::endl;
    }
    stream << "};" << std::endl;
    stream << shaderInputGroup.name << " g_" << notTitle(shaderInputGroup.name) << ";" << std::endl;

    addIncludeGuardEnd(shaderInputGroup.name, stream);
}

static void generateDeviceShaderInputLayout(const ast::ShaderInputLayout& shaderInputLayout, const ShaderInputLayoutBindings& bindings, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    addIncludeGuardStart(shaderInputLayout.name, stream);

    // Static samplers.
    const uint32_t staticSamplerRegisterSpace = 500 + (shaderInputLayout.options.localRootSignature ? 500 : 0);
    for (const auto& [shaderRegister, staticSampler] : iter::enumerate(shaderInputLayout.staticSamplers)) {
        const std::string samplerIncludeGuard = "_sampler_" + staticSampler.name;
        stream << "#ifndef " << samplerIncludeGuard << std::endl;
        stream << "#define " << samplerIncludeGuard << std::endl;
        stream << "SamplerState g_" << staticSampler.name << " : register(s" << shaderRegister << ", space" << staticSamplerRegisterSpace << ");" << std::endl;
        stream << "#endif // " << samplerIncludeGuard << std::endl;
    }
    stream << std::endl;
    // Root constants.
    const uint32_t rootConstantRegisterSpace = 501 + (shaderInputLayout.options.localRootSignature ? 500 : 0);
    for (const auto& [rootConstant, rootParameterIndex] : iter::zip(shaderInputLayout.rootConstants, bindings.constantRootParameterIndices)) {
        const std::string rootConstantIncludeGuard = "_rootConstant_" + rootConstant.name;
        stream << "#ifndef " << rootConstantIncludeGuard << std::endl;
        stream << "#define " << rootConstantIncludeGuard << std::endl;
        stream << "#define ROOT_CONSTANT_" << Tbx::toUpper(rootConstant.name) << " register(b" << rootParameterIndex << ", space" << rootConstantRegisterSpace << ")" << std::endl;
        stream << "#endif // " << rootConstantIncludeGuard << std::endl;
    }
    stream << std::endl;
    // Root ConstantBufferViews.
    const uint32_t rootCBVRegisterSpace = 502 + (shaderInputLayout.options.localRootSignature ? 500 : 0);
    for (const auto& [rootConstantBufferView, rootParameterIndex] : iter::zip(shaderInputLayout.rootConstantBufferViews, bindings.cbvRootParameterIndices)) {
        const std::string rootConstantIncludeGuard = "_rootConstant_" + rootConstantBufferView.name;
        stream << "#ifndef " << rootConstantIncludeGuard << std::endl;
        stream << "#define " << rootConstantIncludeGuard << std::endl;
        stream << "#define ROOT_CBV_" << rootConstantBufferView.name << " register(b" << rootParameterIndex << ", space" << rootCBVRegisterSpace << ")" << std::endl;
        stream << "#endif // " << rootConstantIncludeGuard << std::endl;
    }
    stream << std::endl;

    addIncludeGuardEnd(shaderInputLayout.name, stream);
}

static std::string constTypeName(const std::variant<ast::BasicType, ast::StructInstance, ast::UnresolvedType>& variant, const ast::AbstractSyntaxTree& tree)
{
    return std::visit(
        Tbx::make_visitor(
            [](const ast::BasicType& basicType) { return basicType.hlslType; },
            [&](const ast::StructInstance& s) { return tree.structs[s.structIndex]->name; },
            [&](const ast::UnresolvedType&) -> std::string { return "Unresolved"; }),
        variant);
}

static std::string typeName(const ast::VariableType& type, const ast::AbstractSyntaxTree& tree)
{
    const auto visitor = Tbx::make_visitor(
        [](const ast::UnresolvedType&) -> std::string { throw std::runtime_error("unresolved type encountered"); },
        [](const std::shared_ptr<ast::CustomType>&) -> std::string { throw std::runtime_error("custom type has no type name"); },
        [&](const ast::StructInstance& s) -> std::string { return tree.structs[s.structIndex]->name; },
        [&](const ast::GroupInstance& i) -> std::string { return tree.groups[i.groupIndex]->name; },
        [](const ast::BasicType& bt) -> std::string { return bt.hlslType; },
        [](const ast::Texture2D& tex) -> std::string { return fmt::format("Texture2D<{}>", tex.textureType); },
        [](const ast::RWTexture2D& tex) -> std::string { return fmt::format("RWTexture2D<{}>", tex.textureType); },
        [](const ast::ByteAddressBuffer&) -> std::string { return "ByteAddressBuffer"; },
        [](const ast::RWByteAddressBuffer&) -> std::string { return "RWByteAddressBuffer"; },
        [&](const ast::StructuredBuffer& sb) -> std::string { return fmt::format("StructuredBuffer<{}>", constTypeName(sb.dataType, tree)); },
        [&](const ast::RWStructuredBuffer& sb) -> std::string { return fmt::format("RWStructuredBuffer<{}>", constTypeName(sb.dataType, tree)); },
        [](const ast::RaytracingAccelerationStructure&) -> std::string { return "RaytracingAccelerationStructure"; });
    return std::visit(visitor, type);
}

static char registerTypeChar(const ast::VariableType& type)
{
    const auto visitor = Tbx::make_visitor(
        [](const ast::Texture2D& tex) -> char { return 't'; },
        [](const ast::RWTexture2D& tex) -> char { return 'u'; },
        [](const ast::ByteAddressBuffer&) -> char { return 't'; },
        [](const ast::RWByteAddressBuffer&) -> char { return 'u'; },
        [&](const ast::StructuredBuffer& sb) -> char { return 't'; },
        [&](const ast::RWStructuredBuffer& sb) -> char { return 'u'; },
        [](const ast::RaytracingAccelerationStructure&) -> char { return 't'; },
        [](auto) -> char { throw std::exception(); });
    return std::visit(visitor, type);
}
}
