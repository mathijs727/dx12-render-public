#include "ParseTree.h"
#include "AbstractSyntaxTree.h" // for ShaderInputGroup, ShaderInputL...
#include <tbx/disable_all_warnings.h> // for DISABLE_WARNINGS_POP, DISABLE_...
DISABLE_WARNINGS_PUSH()
#include <fmt/core.h> // for format, format_string
DISABLE_WARNINGS_POP()
#include <list> // for _List_iterator, _List_const_it...
#include <stdexcept> // for runtime_error
#include <stdint.h> // for uint32_t
#include <string> // for basic_string, operator==, string
#include <tbx/variant_helper.h> // for make_visitor, overload
#include <type_traits> // for move
#include <unordered_map> // for unordered_map
#include <unordered_set> // for unordered_set
#include <utility> // for end, pair
#include <variant> // for visit, get, variant

namespace parse {

class ASTBuilder {
public:
    ASTBuilder(const ParseTree& parseTree);

    ast::AbstractSyntaxTree build();

private:
    void add(const std::unique_ptr<ParseTree>&);
    void add(const ParseTree&);
    void add(ast::BindPoint);
    void add(ast::ShaderInputLayout);
    void add(ast::Struct);
    void add(ast::Group);
    void add(ast::ShaderInputGroup);
    void add(ast::Constant);

    void addType(const std::string& name, const ast::VariableType& type);
    void resolveType(ast::Variable& variable);

private:
    ast::AbstractSyntaxTree m_ast;
    ast::AbstractSyntaxTree::Metadata m_metadata;

    std::unordered_set<std::string> m_shaderInputGroups;
    std::unordered_set<std::string> m_groups;
    std::unordered_map<std::string, ast::VariableType> m_typeMapping;
    std::unordered_map<std::string, uint32_t> m_shaderInputLayouts;
    std::unordered_map<std::string, uint32_t> m_bindPoints;
};

ast::AbstractSyntaxTree ParseTree::toAbstractSyntaxTree() const
{
    ASTBuilder builder { *this };
    return builder.build();
}

ASTBuilder::ASTBuilder(const ParseTree& parseTree)
{
    const auto addBasicType = [&](std::string typeName) {
        m_typeMapping[typeName] = ast::BasicType { .hlslType = typeName };
    };

    addBasicType("bool");
    addBasicType("half2");
    addBasicType("float");
    addBasicType("float2");
    addBasicType("float3");
    addBasicType("float4");
    addBasicType("float3x3");
    addBasicType("float4x4");
    addBasicType("int");
    addBasicType("int32_t");
    addBasicType("int64_t");
    addBasicType("int2");
    addBasicType("int3");
    addBasicType("int4");
    addBasicType("uint");
    addBasicType("uint8_t");
    addBasicType("uint16_t");
    addBasicType("uint32_t");
    addBasicType("uint64_t");
    addBasicType("uint2");
    addBasicType("uint3");
    addBasicType("uint4");

    add(parseTree);
}

void ASTBuilder::add(const std::unique_ptr<ParseTree>& pParseTree)
{
    add(*pParseTree);
}

void ASTBuilder::add(const ParseTree& parseTree)
{
    auto oldMetadata = m_metadata;
    m_metadata = parseTree.output;
    for (const auto& statement : parseTree.statements) {
        const auto visitor = Tbx::make_visitor([&](const auto& node) { add(node); });
        std::visit(visitor, statement);
    }

    m_metadata = oldMetadata;
}

void ASTBuilder::add(ast::BindPoint bindPoint)
{
    if (m_bindPoints.find(bindPoint.name) == std::end(m_bindPoints)) {
        m_bindPoints[bindPoint.name] = (uint32_t)m_ast.bindPoints.size();
        m_ast.bindPoints.emplace_back(m_metadata, std::move(bindPoint));
    } else {
        throw std::runtime_error(fmt::format("BindPoint with name `{}` already exists", bindPoint.name));
    }
}

void ASTBuilder::add(ast::ShaderInputLayout sil)
{
    std::unordered_set<std::string> bindPointNames;
    for (auto& bindPointReference : sil.bindPoints) {
        if (bindPointNames.find(bindPointReference.name) == std::end(bindPointNames))
            bindPointNames.insert(bindPointReference.name);
        else
            throw std::runtime_error(fmt::format("BindPoint name `{}` already exists in ShaderInputLayout `{}`", bindPointReference.name, sil.name));

        if (auto iter = m_bindPoints.find(bindPointReference.bindPointName); iter != std::end(m_bindPoints)) {
            bindPointReference.bindPointIndex = iter->second;
        } else {
            throw std::runtime_error(fmt::format("no such bindpoint `{}`", bindPointReference.bindPointName));
        }
    }

    if (m_shaderInputLayouts.find(sil.name) == std::end(m_shaderInputLayouts)) {
        m_shaderInputLayouts[sil.name] = (uint32_t)m_ast.shaderInputLayouts.size();
        m_ast.shaderInputLayouts.emplace_back(m_metadata, std::move(sil));
    } else {
        throw std::runtime_error(fmt::format("ShaderInputLayout with name `{}` already exists", sil.name));
    }
}

void ASTBuilder::add(ast::Struct shaderStruct)
{
    for (auto& variable : shaderStruct.variables)
        resolveType(variable);
    addType(shaderStruct.name, ast::StructInstance { (uint32_t)m_ast.structs.size() });
    m_ast.structs.emplace_back(m_metadata, shaderStruct);
}

void ASTBuilder::add(ast::Group group)
{
    for (auto& variable : group.variables)
        resolveType(variable);

    if (m_groups.find(group.name) == std::end(m_groups))
        m_groups.insert(group.name);
    else
        throw std::runtime_error(fmt::format("Group with name `{}` already exists", group.name));

    addType(group.name, ast::GroupInstance { (uint32_t)m_ast.groups.size() });
    m_ast.groups.emplace_back(m_metadata, group);
}

void ASTBuilder::add(ast::ShaderInputGroup sig)
{
    for (auto& variable : sig.variables)
        resolveType(variable);

    if (m_shaderInputGroups.find(sig.name) == std::end(m_shaderInputGroups))
        m_shaderInputGroups.insert(sig.name);
    else
        throw std::runtime_error(fmt::format("ShaderInputGroup with name `{}` already exists", sig.name));

    if (auto iter = m_bindPoints.find(sig.bindPointName); iter != std::end(m_bindPoints)) {
        auto& bindPoint = m_ast.bindPoints[iter->second];
        bindPoint->shaderInputGroups.push_back((uint32_t)m_ast.shaderInputGroups.size());
        sig.bindPointIndex = iter->second;
        m_ast.shaderInputGroups.emplace_back(m_metadata, std::move(sig));
    } else {
        throw std::runtime_error(fmt::format("no such bindpoint `{}`", sig.bindPointName));
    }
}

void ASTBuilder::add(ast::Constant constant)
{
   m_ast.constants.emplace_back(m_metadata, constant);
}

ast::AbstractSyntaxTree ASTBuilder::build()
{
    return std::move(m_ast);
}

void ASTBuilder::addType(const std::string& name, const ast::VariableType& type)
{
    if (m_typeMapping.contains(name)) {
        throw std::runtime_error(fmt::format("redefinition of \'{}\'", name));
    } else {
        m_typeMapping[name] = type;
    }
}

void ASTBuilder::resolveType(ast::Variable& variable)
{
    const auto makeStructuredType = Tbx::make_visitor(
        [&](const ast::BasicType& basicType) -> ast::StructuredType { return basicType; },
        [&](const ast::StructInstance& structType) -> ast::StructuredType { return structType; },
        [&](auto) -> ast::StructuredType { throw std::runtime_error("Unsupported type in (RW)StructuredBuffer"); });

    const auto visitor = Tbx::make_visitor(
        [&](const ast::UnresolvedType& unresolvedType) -> ast::VariableType {
            if (auto iter = m_typeMapping.find(unresolvedType.typeName); iter != std::end(m_typeMapping)) {
                return iter->second;
            } else {
                throw std::runtime_error(fmt::format("unknown type name \'{}\'", unresolvedType.typeName));
            }
        },
        [&](ast::StructuredBuffer structuredBuffer) -> ast::VariableType {
            const auto unresolvedType = std::get<ast::UnresolvedType>(structuredBuffer.dataType);
            if (auto iter = m_typeMapping.find(unresolvedType.typeName); iter != std::end(m_typeMapping)) {
                structuredBuffer.dataType = std::visit(makeStructuredType, iter->second);
                return structuredBuffer;
            } else {
                throw std::runtime_error(fmt::format("unknown type name \'{}\' in StructuredBuffer", unresolvedType.typeName));
            }
        },
        [&](ast::RWStructuredBuffer rwStructuredBuffer) -> ast::VariableType {
            const auto unresolvedType = std::get<ast::UnresolvedType>(rwStructuredBuffer.dataType);
            if (auto iter = m_typeMapping.find(unresolvedType.typeName); iter != std::end(m_typeMapping)) {
                rwStructuredBuffer.dataType = std::visit(makeStructuredType, iter->second);
                return rwStructuredBuffer;
            } else {
                throw std::runtime_error(fmt::format("unknown type name \'{}\' in StructuredBuffer", unresolvedType.typeName));
            }
        },
        [](auto varType) -> ast::VariableType { return varType; });
    variable.type = std::visit(visitor, variable.type);
}
}