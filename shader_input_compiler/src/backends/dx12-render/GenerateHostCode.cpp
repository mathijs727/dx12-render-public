#include "GenerateHostCode.h"
#include "AbstractSyntaxTree.h" // for Variable, Struct, AbstractSynt...
#include "ConstantBuffer.h" // for isStandardContantVariableType
#include "HLSLRegister.h" // for RegisterType, getDX12RenderReg...
#include "RegisterAllocation.h" // for BindPointBindings, ResourceBin...
#include "StringManipulation.h" // for title
#include "WriteChangeFileStream.h" // for WriteChangeFileStream
#include <tbx/disable_all_warnings.h> // for DISABLE_WARNINGS_POP, DISABLE_...
DISABLE_WARNINGS_PUSH()
#include <cppitertools/enumerate.hpp> // for Enumerable, enumerate
#include <cppitertools/zip.hpp> // for Zipped, zip
#include <fmt/core.h> // for format, format_string, vformat_to
#include <spdlog/common.h> // for format_string_t
#include <spdlog/spdlog.h> // for warn
DISABLE_WARNINGS_POP()
#include <cstddef> // for size_t
#include <cstdint> // for uint32_t
#include <exception> // for exception
#include <filesystem> // for path, operator/, operator<<
#include <fstream> // for basic_ostream, operator<<, endl
#include <functional>
#include <list> // for _List_iterator, _List_const_it...
#include <memory> // for _Simple_types, shared_ptr
#include <optional> // for optional, _Optional_construct_...
#include <span> // for span, _Span_iterator
#include <sstream>
#include <stdexcept> // for runtime_error
#include <string> // for char_traits, operator<<, string
#include <tbx/error_handling.h> // for assert_always
#include <tbx/string.h>
#include <tbx/variant_helper.h> // for make_visitor
#include <tuple> // for tuple
#include <type_traits> // for move
#include <unordered_map> // for unordered_map
#include <utility> // for pair, end, max
#include <variant> // for visit, get, holds_alternative
#include <vector> // for vector, _Vector_const_iterator

namespace dx12_render {

static std::string regularTypeCpp(const std::string& type);
static std::string typeName(const ast::VariableType& type, const ast::AbstractSyntaxTree& tree, bool preferConstantType);
static std::string constantTypeCpp(const std::string& type);
static size_t sizeOfConstantType(const std::string& type);
static size_t alignmentOfConstantType(const std::string& type);

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::Struct>& shaderStruct)
{
    return shaderStruct.metadata.cppFolder / "structs" / fmt::format("{}.h", shaderStruct->name);
}

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::Group>& group)
{
    return group.metadata.cppFolder / "groups" / fmt::format("{}.h", group->name);
}

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::ShaderInputGroup>& shaderInputGroup)
{
    return shaderInputGroup.metadata.cppFolder / "inputgroups" / fmt::format("{}.h", shaderInputGroup->name);
}

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::ShaderInputLayout>& shaderInputLayout)
{
    return shaderInputLayout.metadata.cppFolder / "inputlayouts" / fmt::format("{}.h", shaderInputLayout->name);
}

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::BindPoint>& bindPoint)
{
    return bindPoint.metadata.cppFolder / "bindpoints" / fmt::format("{}.h", bindPoint->name);
}

static std::filesystem::path getFilePath(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::Constant>& constant)
{
    return constant.metadata.cppFolder / "constants.h";
}

static void createFolderContainingFile(const std::filesystem::path& filePath)
{
    if (!std::filesystem::exists(filePath.parent_path()))
        std::filesystem::create_directories(filePath.parent_path());
}

static void addInclude(const ast::VariableType& type, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& basePath, std::ostream& outStream)
{
    const auto visitor = Tbx::make_visitor(
        [&](const ast::StructInstance& struct_) {
            outStream << "#include " << std::filesystem::relative(getFilePath(tree.structs[struct_.structIndex]), basePath) << "\n";
        },
        [&](const ast::GroupInstance& group) {
            outStream << "#include " << std::filesystem::relative(getFilePath(tree.groups[group.groupIndex]), basePath) << "\n";
        },
        [](auto) {});
    std::visit(visitor, type);
}

static void addInclude(const ast::AbstractSyntaxTree::ItemWithMetadata<ast::BindPoint>& bindPoint, const std::filesystem::path& basePath, std::ostream& outStream)
{
    outStream << "#include " << std::filesystem::relative(getFilePath(bindPoint), basePath) << "\n";
}

void generateDeviceConstants(std::span<const ast::Constant> constants, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    stream << "#pragma once" << std::endl;
    for (const auto& constant : constants) {
        stream << "#define " << constant.name << " " << constant.value << std::endl;
    }
}

void generateHostStruct(const ast::Struct& shaderStruct, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    stream << "#pragma once" << std::endl;
    stream << "#include <glm/vec2.hpp>" << std::endl;
    stream << "#include <glm/vec3.hpp>" << std::endl;
    stream << "#include <glm/vec4.hpp>" << std::endl;
    stream << "#include <DirectXPackedVector.h>" << std::endl;

    // Add include statements.
    const auto basePath = filePath.parent_path();
    for (const auto& variable : shaderStruct.variables)
        addInclude(variable.type, tree, basePath, stream);

    stream << "namespace ShaderInputs {" << std::endl;
    const auto generateStruct = [&](bool constantPacking) {
        stream << "struct " << (constantPacking ? "C" : "") << shaderStruct.name << " {" << std::endl;
        for (const auto& variable : shaderStruct.variables) {
            stream << "\t";
            if (std::holds_alternative<ast::StructInstance>(variable.type)) {
                const auto& otherStruct = tree.structs[std::get<ast::StructInstance>(variable.type).structIndex];
                stream << (constantPacking ? "C" : "") << otherStruct->name;
            } else {
                const auto& varType = std::get<ast::BasicType>(variable.type);
                if (constantPacking)
                    stream << constantTypeCpp(varType.hlslType);
                else
                    stream << regularTypeCpp(varType.hlslType);
            }
            stream << " " << variable.name;
            if (variable.arrayCount != 0)
                stream << "[" << variable.arrayCount << "]";
            stream << ";" << std::endl;
        }
        stream << "};" << std::endl;
    };
    generateStruct(true);
    generateStruct(false);
    stream << "}" << std::endl; // Namespace.
}

void generateHostGroup(const ast::Group& group, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    stream << "#pragma once" << std::endl;
    stream << "#include <glm/vec2.hpp>" << std::endl;
    stream << "#include <glm/vec3.hpp>" << std::endl;
    stream << "#include <glm/vec4.hpp>" << std::endl;
    stream << "#include \"Engine/RenderAPI/ShaderInput.h\"" << std::endl;

    // Add include statements.
    const auto basePath = filePath.parent_path();
    for (const auto& variable : group.variables)
        addInclude(variable.type, tree, basePath, stream);

    stream << "namespace ShaderInputs {" << std::endl;
    stream << "struct " << group.name << " {" << std::endl;
    for (const ast::Variable& variable : group.variables) {
        stream << "\t" << typeName(variable.type, tree, true) << " " << variable.name;
        if (variable.arrayCount != 0)
            stream << "[" << variable.arrayCount << "]";
        stream << ";" << std::endl;
    }
    stream << "};" << std::endl;
    stream << "}" << std::endl; // Namespace.
}

static std::string descriptorRangeTypeString(RegisterType registerType)
{
    static std::unordered_map<RegisterType, std::string> names = {
        { RegisterType::ConstantBuffer, "D3D12_DESCRIPTOR_RANGE_TYPE_CBV" },
        { RegisterType::ShaderResource, "D3D12_DESCRIPTOR_RANGE_TYPE_SRV" },
        { RegisterType::UnorderedAccess, "D3D12_DESCRIPTOR_RANGE_TYPE_UAV" },
        { RegisterType::Sampler, "D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER" }
    };
    return names.find(registerType)->second;
}

static std::string shaderVisibilityString(ast::ShaderStage shaderStage)
{
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_shader_visibility
    static std::unordered_map<ast::ShaderStage, std::string> names = {
        { ast::ShaderStage::Vertex, "D3D12_SHADER_VISIBILITY_VERTEX" },
        { ast::ShaderStage::Geometry, "D3D12_SHADER_VISIBILITY_GEOMETRY" },
        { ast::ShaderStage::Pixel, "D3D12_SHADER_VISIBILITY_PIXEL" },
        { ast::ShaderStage::Compute, "D3D12_SHADER_VISIBILITY_ALL" }, // Compute only has one stage
        { ast::ShaderStage::RayTracing, "D3D12_SHADER_VISIBILITY_ALL" },
    };
    return names.find(shaderStage)->second;
}

static void generateHostShaderInputGroup(const ast::ShaderInputGroup& shaderInputGroup, const ShaderInputGroupBindings& bindings, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    stream << "#pragma once" << std::endl;
    stream << "#include \"Engine/RenderAPI/ShaderInput.h\"" << std::endl;
    stream << "#include \"Engine/Render/RenderContext.h\"" << std::endl;
    stream << "#include <tbx/move_only.h>" << std::endl
           << std::endl;

    // Add include statements.
    const auto basePath = filePath.parent_path();
    addInclude(tree.bindPoints[shaderInputGroup.bindPointIndex], basePath, stream);
    for (const auto& variable : shaderInputGroup.variables)
        addInclude(variable.type, tree, basePath, stream);

    stream << "namespace ShaderInputs {" << std::endl;
    stream << "struct " << shaderInputGroup.name << " {" << std::endl;

    const auto addGenerateBindingsCode = [&](bool transient) {
        stream << "\tinline " << shaderInputGroup.bindPointName << " generate" << (transient ? "Transient" : "Persistent") << "Bindings(Render::RenderContext& renderContext) const {" << std::endl;
        stream << "\t\t" << shaderInputGroup.bindPointName << " out {};" << std::endl;
        const auto cbvSrvUavDescriptorAllocator = transient ? "renderContext.getCurrentCbvSrvUavDescriptorTransientAllocator()" : "renderContext.cbvSrvUavDescriptorStaticAllocator";

        for (const auto& rootParameter : bindings.rootParameters) {
            const auto& descriptorTable = rootParameter.descriptorTable;

            stream << "\t\t{" << std::endl;
            stream << "\t\t\tauto descriptorAllocation = " << cbvSrvUavDescriptorAllocator << ".allocate(" << rootParameter.descriptorTable.numKnownDescriptors;
            if (descriptorTable.optUnboundedVariableIdx)
                stream << " + (uint32_t)m_" << shaderInputGroup.variables[*descriptorTable.optUnboundedVariableIdx].name << ".size()";
            stream << ");" << std::endl;

            stream << "\t\t\tconst auto descriptorIncrementSize = renderContext.pCbvSrvUavDescriptorBaseAllocatorCPU->descriptorIncrementSize;" << std::endl;
            for (const auto& descriptor : descriptorTable.descriptors) {
                const auto& variable = shaderInputGroup.variables[descriptor.variableIdx];
                const RegisterType registerType = getDX12RenderRegisterType(variable.type);
                if (registerType == RegisterType::ConstantBuffer)
                    stream << "\t\t\t{" << std::endl;
                else if (variable.arrayCount == 0) {
                    stream << "\t\t\tif (m_" << variable.name << ") {" << std::endl;
                } else {
                    stream << "\t\t\tif (!m_" << variable.name << ".empty()) {" << std::endl;
                }
                stream << "\t\t\t\tCD3DX12_CPU_DESCRIPTOR_HANDLE descriptor;" << std::endl;
                stream << "\t\t\t\tdescriptor.InitOffsetted(descriptorAllocation.firstCPUDescriptor, " << descriptor.descriptorOffset << ", descriptorIncrementSize);" << std::endl;
                if (registerType == RegisterType::ConstantBuffer) {
                    if (transient) {
                        stream << "\t\t\t\tauto& allocator = renderContext.singleFrameBufferAllocator;" << std::endl;
                        stream << "\t\t\t\tconst auto desc = allocator.allocateCBV(m_constants);" << std::endl;
                    } else {
                        stream << "\t\t\t\tout.pConstantBuffer = renderContext.createBufferWithData(m_constants, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);" << std::endl;
                        stream << "\t\t\t\tD3D12_CONSTANT_BUFFER_VIEW_DESC desc { .BufferLocation =  out.pConstantBuffer->GetGPUVirtualAddress(), .SizeInBytes = (UINT)Util::roundUpToClosestMultiplePowerOf2(sizeof(m_constants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) };" << std::endl;
                    }
                    stream << "\t\t\t\trenderContext.pDevice->CreateConstantBufferView(&desc, descriptor);" << std::endl;
                } else if (registerType == RegisterType::UnorderedAccess || registerType == RegisterType::ShaderResource) {
                    const auto createViewFunc = registerType == RegisterType::UnorderedAccess ? "CreateUnorderedAccessView" : "CreateShaderResourceView";
                    const auto extraArg = registerType == RegisterType::UnorderedAccess ? ", nullptr" : "";
                    if (variable.arrayCount == 0) {
                        stream << fmt::format("\t\t\t\trenderContext.pDevice->{0}(m_{1}->pResource{2}, &m_{1}->desc, descriptor);",
                            createViewFunc, variable.name, extraArg)
                               << std::endl;
                    } else {
                        stream << "\t\t\t\tfor (size_t i = 0; i < ";
                        if (variable.arrayCount == ast::Variable::Unbounded)
                            stream << "m_" << variable.name << ".size()";
                        else
                            stream << variable.arrayCount;
                        stream << "; ++i) {" << std::endl;
                        stream << fmt::format("\t\t\t\t\trenderContext.pDevice->{0}(m_{1}[i].pResource{2}, &m_{1}[i].desc, descriptor);",
                            createViewFunc, variable.name, extraArg)
                               << std::endl;
                        stream << "\t\t\t\t\tdescriptor = descriptor.Offset(1, descriptorIncrementSize);" << std::endl;
                        stream << "\t\t\t\t}" << std::endl;
                    }
                } else {
                    // stream << "\t\t\t\tthrow std::runtime_error(\"not implemented yet\");" << std::endl;
                    stream << "\t\t\t\tspdlog::error(\"not implemented yet\");" << std::endl;
                }
                stream << "\t\t\t}" << std::endl;
            }
            stream << "\t\t\tout.rootParameter" << rootParameter.rootParameterOffset << " = descriptorAllocation;" << std::endl;
            stream << "\t\t}" << std::endl;
        }
        if (!transient)
            stream << "\t\tout.pParent = &renderContext;" << std::endl;
        stream << "\t\treturn out;" << std::endl;
        stream << "\t}" << std::endl;
    };
    addGenerateBindingsCode(true);
    addGenerateBindingsCode(false);

    // Generate setters.
    stream << "public:" << std::endl;
    for (const auto& variable : shaderInputGroup.variables) {
        // Custom variables only used for resource binding; not visible to user.
        if (isCustomConstantVariableType(variable.type))
            continue;

        // Function signature changes when passing an array of items.
        stream << "\tinline void set" << title(variable.name) << "(";
        if (variable.arrayCount > 0)
            stream << "std::span<const " << typeName(variable.type, tree, true) << "> " << variable.name;
        else if (std::holds_alternative<ast::BasicType>(variable.type))
            stream << typeName(variable.type, tree, true) << " " << variable.name;
        else
            stream << "const " << typeName(variable.type, tree, false) << "& " << variable.name;
        stream << ") {" << std::endl;

        const std::function<void(const ast::Variable&, std::string, std::string)> generateStructBindingsRecursive = [&](const ast::Variable& variable, std::string prefix1 = "", std::string prefix2 = "") {
            if (!isStandardContantVariableType(variable.type))
                return;

            if (variable.arrayCount == 0) {
                if (std::holds_alternative<ast::BasicType>(variable.type)) {
                    stream << "\t\tm_constants." << prefix1 << variable.name << " = " << prefix2 << variable.name << ";" << std::endl;
                } else if (std::holds_alternative<ast::StructInstance>(variable.type)) {
                    const ast::Struct& str = tree.structs[std::get<ast::StructInstance>(variable.type).structIndex];
                    for (const auto& childVariable : str.variables) {
                        generateStructBindingsRecursive(childVariable, prefix1 + variable.name + "_", prefix2 + variable.name + ".");
                    }
                }
            } else if (variable.arrayCount > 0) {
                for (uint32_t i = 0; i < variable.arrayCount; ++i) {
                    if (std::holds_alternative<ast::BasicType>(variable.type)) {
                        stream << "\t\tm_constants." << prefix1 << variable.name << i << " = " << prefix2 << variable.name << "[" << std::to_string(i) << "];" << std::endl;
                    } else if (std::holds_alternative<ast::StructInstance>(variable.type)) {
                        const ast::Struct& str = tree.structs[std::get<ast::StructInstance>(variable.type).structIndex];
                        for (const auto& childVariable : str.variables) {
                            generateStructBindingsRecursive(childVariable, prefix1 + variable.name + std::to_string(i) + "_", prefix2 + variable.name + "[" + std::to_string(i) + "].");
                        }
                    }
                }
            }
        };

        const auto visitor = Tbx::make_visitor(
            [&](const ast::BasicType& basicType) {
                generateStructBindingsRecursive(variable, "", "");
            },
            [&](const ast::StructInstance& structRef) {
                generateStructBindingsRecursive(variable, "", "");
            },
            [&](const ast::GroupInstance& groupRef) {
                Tbx::assert_always(variable.arrayCount == 0);
                const auto& group = tree.groups[groupRef.groupIndex];
                for (const ast::Variable& groupVariable : group->variables) {
                    const auto mangledVariableName = getGroupVariableMangledName(variable.name, groupVariable.name);
                    stream << "\t\tset" << title(mangledVariableName) << "(" << variable.name << "." << groupVariable.name << ");" << std::endl;
                }
            },
            [&](auto) {
                // m_{variable.name} is also a std::span<...> so no need to copy.
                stream << "\t\tm_" << variable.name << " = " << variable.name << ";" << std::endl;
            });
        std::visit(visitor, variable.type);
        stream << "\t}" << std::endl;
    }

    // Generate member variable declarations.
    stream << "private:" << std::endl;
    for (const auto& variable : shaderInputGroup.variables) {
        if (!isCustomConstantVariableType(variable.type) &&
            !isStandardContantVariableType(variable.type) && 
            !std::holds_alternative<ast::GroupInstance>(variable.type)) {
            if (variable.arrayCount == 0) {
                stream << "\tstd::optional<" << typeName(variable.type, tree, false) << "> m_" << variable.name << ";" << std::endl;
            } else {
                stream << "\tstd::span<const " << typeName(variable.type, tree, false);
                if (variable.arrayCount != ast::Variable::Unbounded)
                    stream << ", " << variable.arrayCount;
                stream << "> m_" << variable.name << ";" << std::endl;
            }
        }
    }

    // Generate member variable for constants.
    // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules
    stream << "\tstruct Constants {" << std::endl;
    size_t constantMemoryCursor = 0;
    const auto addPadding = [&](size_t numBytes) {
        stream << "\t\tuint8_t __padding" << constantMemoryCursor << "[" << numBytes << "];" << std::endl;
        constantMemoryCursor += numBytes;
    };
    const auto addPaddingIfNecessary = [&](size_t size, size_t alignment) {
        // Add padding to align the start of the item.
        if (size_t offAlignment = constantMemoryCursor % alignment)
            addPadding(alignment - offAlignment);

        // If the item straddles the 16-byte boundary, then add padding to make it 16-byte aligned.
        const bool spansAlignmentBoundary = (constantMemoryCursor ^ (constantMemoryCursor + size - 1)) >> 4;
        if ((constantMemoryCursor % 16) && spansAlignmentBoundary)
            addPadding(16 - (constantMemoryCursor % 16));
    };
    const std::function<void(const ast::Variable&, std::string)> generateConstantTypes = [&](const ast::Variable& variable, std::string prefix = "") {
        if (!isStandardContantVariableType(variable.type))
            return;

        if (std::holds_alternative<ast::BasicType>(variable.type)) {
            const auto& basicType = std::get<ast::BasicType>(variable.type);
            const auto itemSize = sizeOfConstantType(basicType.hlslType);
            const auto itemAlignment = alignmentOfConstantType(basicType.hlslType);
            if (variable.arrayCount == 0) {
                addPaddingIfNecessary(itemSize, itemAlignment);
                constantMemoryCursor += itemSize;
                stream << "\t\t" << constantTypeCpp(basicType.hlslType) << " " << prefix << variable.name << ";" << std::endl;
            } else {
                for (uint32_t i = 0; i < variable.arrayCount; ++i) {
                    addPaddingIfNecessary(1, 16); // Array items are always 16-byte aligned.
                    constantMemoryCursor += itemSize;
                    stream << "\t\t" << constantTypeCpp(basicType.hlslType) << " " << prefix << variable.name << std::to_string(i) << ";" << std::endl;
                }
            }
        } else if (std::holds_alternative<ast::StructInstance>(variable.type)) {
            const ast::Struct& str = tree.structs[std::get<ast::StructInstance>(variable.type).structIndex];
            if (variable.arrayCount == 0) {
                addPaddingIfNecessary(1, 16); // Structs are always 16-byte aligned.
                for (const auto& childVariable : str.variables) {
                    generateConstantTypes(childVariable, prefix + variable.name + "_");
                }
            } else {
                for (uint32_t i = 0; i < variable.arrayCount; ++i) {
                    addPaddingIfNecessary(1, 16); // Structs are always 16-byte aligned.
                    for (const auto& childVariable : str.variables) {
                        generateConstantTypes(childVariable, prefix + variable.name + std::to_string(i) + "_");
                    }
                }
            }
        }
    };
    for (const auto& variable : shaderInputGroup.variables) {
        generateConstantTypes(variable, "");
    }
    stream << "\t};" << std::endl;
    stream << "\tConstants m_constants;" << std::endl;

    stream << "};" << std::endl;
    stream << "}" << std::endl; // Namespace.
}

static void generateHostBindPoint(const ast::BindPoint& bindPoint, const BindPointBindings& bindings, const ast::AbstractSyntaxTree& tree, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    stream << "#pragma once" << std::endl;
    stream << "#include \"Engine/RenderAPI/Descriptor/DescriptorAllocation.h\"" << std::endl;
    stream << "#include \"Engine/RenderAPI/MaResource.h\"" << std::endl;
    stream << "#include \"Engine/Render/RenderContext.h\"" << std::endl;
    stream << "#include <tbx/move_only.h>" << std::endl
           << std::endl;

    stream << "namespace ShaderInputs {" << std::endl;
    stream << "struct " << bindPoint.name << " {" << std::endl;
    for (const auto& rootParameter : bindings.rootParameters) {
        stream << "\tRenderAPI::DescriptorAllocation rootParameter" << rootParameter.rootParameterOffset << ";" << std::endl;
    }
    stream << "\tRenderAPI::D3D12MAResource pConstantBuffer;" << std::endl;

    // Destructor.
    stream << "\n\t" << bindPoint.name << "() = default;" << std::endl;
    stream << "\t~" << bindPoint.name << "() {" << std::endl;
    stream << "\t\tif (pParent) {" << std::endl;
    for (const auto& rootParameter : bindings.rootParameters) {
        stream << "\t\t\tpParent->cbvSrvUavDescriptorStaticAllocator.release(rootParameter" << rootParameter.rootParameterOffset << ");" << std::endl;
    }
    stream << "\t\t}" << std::endl;
    stream << "\t}" << std::endl;
    stream << "\tNO_COPY(" << bindPoint.name << ");" << std::endl;
    stream << "\tDEFAULT_MOVE(" << bindPoint.name << ");" << std::endl;

    stream << "\n\tTbx::MovePointer<Render::RenderContext> pParent;" << std::endl;
    stream << "};" << std::endl;
    stream << "}" << std::endl; // Namespace.
}

static void generateHostShaderInputLayout(const ast::ShaderInputLayout& shaderInputLayout, const ShaderInputLayoutBindings& shaderInputLayoutBindings, const ast::AbstractSyntaxTree& tree, const ResourceBindingInfo& bindings, const std::filesystem::path& filePath)
{
    WriteChangeFileStream stream { filePath };
    stream << "#pragma once" << std::endl;

    // Add include statements.
    const auto basePath = filePath.parent_path();
    for (const auto& bindPointReference : shaderInputLayout.bindPoints)
        addInclude(tree.bindPoints[bindPointReference.bindPointIndex], basePath, stream);

    stream << "namespace ShaderInputs {" << std::endl;
    stream << "struct " << shaderInputLayout.name << " {" << std::endl;
    for (const auto& [bindPointReference, rootParameterStartIndex] : iter::zip(shaderInputLayout.bindPoints, shaderInputLayoutBindings.bindPointsRootParameterIndices)) {
        const auto& bindPoint = tree.bindPoints[bindPointReference.bindPointIndex];
        const auto& bindPointBindings = bindings.bindPoints[bindPointReference.bindPointIndex];

        const auto generateBindingCode = [&](const std::string& modeString) {
            stream << "\tstatic inline void bind" << title(bindPointReference.name) << modeString << "(ID3D12GraphicsCommandList* pCommandList, const " << bindPoint->name << "& shaderInputGroup) {" << std::endl;
            for (const auto& rootParameter : bindPointBindings.rootParameters) {
                const uint32_t rootParameterIndex = rootParameterStartIndex + rootParameter.rootParameterOffset;
                stream << "\t\tif (shaderInputGroup.rootParameter" << rootParameter.rootParameterOffset << ".numDescriptors > 0) {" << std::endl;
                stream << fmt::format("\t\t\tpCommandList->Set{}RootDescriptorTable({}, shaderInputGroup.rootParameter{}.firstGPUDescriptor);",
                    modeString, rootParameterIndex, rootParameter.rootParameterOffset)
                       << std::endl;
                stream << "\t\t}" << std::endl;
            }
            stream << "\t}" << std::endl;
        };
        generateBindingCode("Graphics");
        generateBindingCode("Compute");
    }

    // Get Constant root parameter index.
    for (const auto& [rootConstant, rootParameterIndex] : iter::zip(shaderInputLayout.rootConstants, shaderInputLayoutBindings.constantRootParameterIndices)) {
        stream << "\tstatic inline uint32_t get" << title(rootConstant.name) << "RootParameterIndex() {" << std::endl;
        stream << "\t\treturn " << rootParameterIndex << ";" << std::endl;
        stream << "\t}" << std::endl;
    }

    // Get ConstantBufferView root parameter index.
    for (const auto& [rootConstantBufferView, rootParameterIndex] : iter::zip(shaderInputLayout.rootConstantBufferViews, shaderInputLayoutBindings.cbvRootParameterIndices)) {
        stream << "\tstatic inline uint32_t get" << title(rootConstantBufferView.name) << "RootParameterIndex() {" << std::endl;
        stream << "\t\treturn " << rootParameterIndex << ";" << std::endl;
        stream << "\t}" << std::endl;
    }

    // Bind to a ShaderBindingTable
    if (shaderInputLayout.options.localRootSignature) {
        // Collect shader inputs (currently DescriptorTables) sorted by rootParameterIndex.
        struct RootParameter {
            uint32_t bindPointIdx = (uint32_t)-1;
            uint32_t rootParameterOffset;
        };
        std::vector<RootParameter> shaderInputs;
        {
            uint32_t bindPointIdx = 0;
            for (const auto& [bindPointReference, rootParameterStartIndex] : iter::zip(shaderInputLayout.bindPoints, shaderInputLayoutBindings.bindPointsRootParameterIndices)) {
                const auto& bindPointBindings = bindings.bindPoints[bindPointReference.bindPointIndex];
                for (const auto& rootParameter : bindPointBindings.rootParameters) {
                    const uint32_t rootParameterIndex = rootParameterStartIndex + rootParameter.rootParameterOffset;
                    if (rootParameterIndex >= shaderInputs.size())
                        shaderInputs.resize(rootParameterIndex + 1);
                    shaderInputs[rootParameterIndex] = { .bindPointIdx = bindPointIdx, .rootParameterOffset = rootParameter.rootParameterOffset };
                }
                ++bindPointIdx;
            }
        }

        stream << "\n\tstatic std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, " << shaderInputs.size() << "> getShaderBindings(";
        for (const auto& [bindPointIdx, bindPointReference] : iter::enumerate(shaderInputLayout.bindPoints)) {
            const auto& bindPoint = tree.bindPoints[bindPointReference.bindPointIndex];
            stream << "const " << bindPoint->name << "& shaderInputGroup" << bindPointIdx;
            if (bindPointIdx != shaderInputLayout.bindPoints.size() - 1)
                stream << ", ";
        }
        stream << ") {" << std::endl;
        stream << "\t\treturn {" << std::endl;
        for (const auto& rootParameter : shaderInputs) {
            if (rootParameter.bindPointIdx == (uint32_t)-1) {
                stream << "\t\t\t0," << std::endl;
            } else {
                stream << "\t\t\tshaderInputGroup" << rootParameter.bindPointIdx << ".rootParameter" << rootParameter.rootParameterOffset << ".firstGPUDescriptor," << std::endl;
            }
        }
        stream << "\t\t};" << std::endl;
        stream << "\t}" << std::endl;
    }

    // Create root signature.
    stream << "\tstatic inline WRL::ComPtr<ID3D12RootSignature> getRootSignature(ID3D12Device* pDevice) {" << std::endl;
    stream << "\t\tusing namespace RenderAPI;" << std::endl;
    stream << "\t\tstatic WRL::ComPtr<ID3D12RootSignature> s_pRootSignature = nullptr;" << std::endl;
    stream << "\t\tif (!s_pRootSignature ) {" << std::endl;

    uint32_t numDescriptorRanges = 0, numRootParameters = 0;
    for (const auto& [bindPointReference, rootParameterStartIndex] : iter::zip(shaderInputLayout.bindPoints, shaderInputLayoutBindings.bindPointsRootParameterIndices)) {
        const auto& bindPointBindings = bindings.bindPoints[bindPointReference.bindPointIndex];

        for (const auto& rootParameter : bindPointBindings.rootParameters) {
            numDescriptorRanges += (uint32_t)rootParameter.descriptorTableLayout.ranges.size();

            const uint32_t rootParameterIndex = rootParameterStartIndex + rootParameter.rootParameterOffset;
            numRootParameters = std::max(numRootParameters, rootParameterIndex + 1);
        }
    }
    for (uint32_t rootParameterIndex : shaderInputLayoutBindings.constantRootParameterIndices)
        numRootParameters = std::max(numRootParameters, rootParameterIndex + 1);
    for (uint32_t rootParameterIndex : shaderInputLayoutBindings.cbvRootParameterIndices)
        numRootParameters = std::max(numRootParameters, rootParameterIndex + 1);
    stream << "\t\t\tstd::array<D3D12_ROOT_PARAMETER, " << numRootParameters << "> rootParameters;" << std::endl;
    stream << "\t\t\tstd::array<D3D12_DESCRIPTOR_RANGE, " << numDescriptorRanges << "> descriptorRanges;" << std::endl;
    stream << std::endl;

    bool requiresInputAssembler = false;
    uint32_t currentDescriptorRange = 0;
    for (const auto& [bindPointReference, rootParameterStartIndex] : iter::zip(shaderInputLayout.bindPoints, shaderInputLayoutBindings.bindPointsRootParameterIndices)) {
        const auto& bindPointBindings = bindings.bindPoints[bindPointReference.bindPointIndex];
        for (const auto& rootParameter : bindPointBindings.rootParameters) {
            const uint32_t rootParameterIndex = rootParameterStartIndex + rootParameter.rootParameterOffset;
            const uint32_t shaderRegisterSpace = rootParameterIndex + (shaderInputLayout.options.localRootSignature ? 500 : 0);

            const uint32_t firstDescriptorRange = currentDescriptorRange;
            for (const auto& descriptorRange : rootParameter.descriptorTableLayout.ranges) {
                stream << "\t\t\tdescriptorRanges[" << currentDescriptorRange << "].BaseShaderRegister = " << descriptorRange.baseDescriptorOffset << ";" << std::endl;
                stream << "\t\t\tdescriptorRanges[" << currentDescriptorRange << "].RegisterSpace = " << shaderRegisterSpace << ";" << std::endl;
                stream << "\t\t\tdescriptorRanges[" << currentDescriptorRange << "].RangeType = " << descriptorRangeTypeString(descriptorRange.type) << ";" << std::endl;
                stream << "\t\t\tdescriptorRanges[" << currentDescriptorRange << "].NumDescriptors = " << descriptorRange.numDescriptors << ";" << std::endl;
                stream << "\t\t\tdescriptorRanges[" << currentDescriptorRange << "].OffsetInDescriptorsFromTableStart = " << descriptorRange.baseDescriptorOffset << ";" << std::endl;
                ++currentDescriptorRange;
            }
            stream << "\t\t\trootParameters[" << rootParameterIndex << "].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;" << std::endl;
            bool allSameType = true;
            auto firstShaderStage = bindPointReference.shaderStages[0];
            for (const auto shaderStage : bindPointReference.shaderStages) {
                if (shaderStage != ast::ShaderStage::Compute && shaderStage != ast::ShaderStage::RayTracing)
                    requiresInputAssembler = true;
                if (shaderStage != firstShaderStage)
                    allSameType = false;
            }
            stream << "\t\t\trootParameters[" << rootParameterIndex << "].ShaderVisibility = " << (allSameType ? shaderVisibilityString(firstShaderStage) : "D3D12_SHADER_VISIBILITY_ALL") << ";" << std::endl;
            stream << "\t\t\trootParameters[" << rootParameterIndex << "].DescriptorTable.pDescriptorRanges =  &descriptorRanges[" << firstDescriptorRange << "];" << std::endl;
            stream << "\t\t\trootParameters[" << rootParameterIndex << "].DescriptorTable.NumDescriptorRanges = " << rootParameter.descriptorTableLayout.ranges.size() << ";" << std::endl;
            stream << std::endl;
        }
    }
    // Static samplers.
    const uint32_t staticSamplerRegisterSpace = 500 + (shaderInputLayout.options.localRootSignature ? 500 : 0);
    if (!shaderInputLayout.staticSamplers.empty()) {
        stream << "\t\t\tstd::array<D3D12_STATIC_SAMPLER_DESC, " << shaderInputLayout.staticSamplers.size() << "> staticSamplers;" << std::endl;
        for (const auto& [samplerIdx, staticSampler] : iter::enumerate(shaderInputLayout.staticSamplers)) {
            auto options = staticSampler.options;
            const auto parseOption = [&](std::string optionName, std::string defaultValue) {
                std::string optionValue = std::move(defaultValue);
                if (auto iter = options.find(optionName); iter != std::end(options)) {
                    optionValue = iter->second;
                    options.erase(iter);
                }
                stream << "\t\t\tstaticSamplers[" << samplerIdx << "]." << optionName << " = " << optionValue << ";" << std::endl;
            };
            // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_sampler_desc
            parseOption("Filter", "D3D12_FILTER_MIN_MAG_MIP_POINT");
            parseOption("AddressU", "D3D12_TEXTURE_ADDRESS_MODE_WRAP");
            parseOption("AddressV", "D3D12_TEXTURE_ADDRESS_MODE_WRAP");
            parseOption("AddressW", "D3D12_TEXTURE_ADDRESS_MODE_WRAP");
            parseOption("MipLODBias", "0.0f");
            parseOption("MaxAnisotropy", "1");
            parseOption("ComparisonFunc", "(D3D12_COMPARISON_FUNC)0");
            parseOption("BorderColor", "D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK");
            parseOption("MinLOD", "0.0f");
            parseOption("MaxLOD", "1000.0f"); // Some high value
            stream << "\t\t\tstaticSamplers[" << samplerIdx << "].ShaderRegister = " << samplerIdx << ";" << std::endl;
            stream << "\t\t\tstaticSamplers[" << samplerIdx << "].RegisterSpace = " << staticSamplerRegisterSpace << ";" << std::endl;
            // TODO: ...
            stream << "\t\t\tstaticSamplers[" << samplerIdx << "].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;// TODO" << std::endl;

            for (const auto& [key, _] : options) {
                spdlog::warn("Unrecognized StaticSampler argument: {}", key);
            }
        }
    }
    // Root constants.
    const uint32_t rootConstantRegisterSpace = 501 + (shaderInputLayout.options.localRootSignature ? 500 : 0);
    for (const auto& [rootConstant, rootParameterIndex] : iter::zip(shaderInputLayout.rootConstants, shaderInputLayoutBindings.constantRootParameterIndices)) {
        stream << "\t\t\trootParameters[" << rootParameterIndex << "].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;" << std::endl;
        bool allSameType = true;
        auto firstShaderStage = rootConstant.shaderStages[0];
        for (const auto shaderStage : rootConstant.shaderStages) {
            if (shaderStage != ast::ShaderStage::Compute && shaderStage != ast::ShaderStage::RayTracing)
                requiresInputAssembler = true;
            if (shaderStage != firstShaderStage)
                allSameType = false;
        }
        stream << "\t\t\trootParameters[" << rootParameterIndex << "].ShaderVisibility = " << (allSameType ? shaderVisibilityString(firstShaderStage) : "D3D12_SHADER_VISIBILITY_ALL") << ";" << std::endl;

        const auto setConstantParam = [&](const std::string& name, auto value) {
            stream << "\t\t\trootParameters[" << rootParameterIndex << "].Constants." << name << " = " << value << ";" << std::endl;
        };
        setConstantParam("ShaderRegister", rootParameterIndex);
        setConstantParam("RegisterSpace", rootConstantRegisterSpace);
        setConstantParam("Num32BitValues", rootConstant.num32Bitvalues);
    }
    // Root ConstantBufferViews.
    const uint32_t rootCBVRegisterSpace = 502 + (shaderInputLayout.options.localRootSignature ? 500 : 0);
    for (const auto& [rootConstant, rootParameterIndex] : iter::zip(shaderInputLayout.rootConstantBufferViews, shaderInputLayoutBindings.cbvRootParameterIndices)) {
        stream << "\t\t\trootParameters[" << rootParameterIndex << "].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;" << std::endl;
        bool allSameType = true;
        auto firstShaderStage = rootConstant.shaderStages[0];
        for (const auto shaderStage : rootConstant.shaderStages) {
            if (shaderStage != ast::ShaderStage::Compute && shaderStage != ast::ShaderStage::RayTracing)
                requiresInputAssembler = true;
            if (shaderStage != firstShaderStage)
                allSameType = false;
        }
        stream << "\t\t\trootParameters[" << rootParameterIndex << "].ShaderVisibility = " << (allSameType ? shaderVisibilityString(firstShaderStage) : "D3D12_SHADER_VISIBILITY_ALL") << ";" << std::endl;

        const auto setDescriptorParam = [&](const std::string& name, auto value) {
            stream << "\t\t\trootParameters[" << rootParameterIndex << "].Descriptor." << name << " = " << value << ";" << std::endl;
        };
        setDescriptorParam("ShaderRegister", rootParameterIndex);
        setDescriptorParam("RegisterSpace", rootCBVRegisterSpace);
    }

    stream << "\t\t\tCD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc {};" << std::endl;
    stream << "\t\t\tD3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;" << std::endl;
    if (shaderInputLayout.options.localRootSignature)
        stream << "\t\t\trootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;" << std::endl;
    if (requiresInputAssembler)
        stream << "\t\t\trootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;" << std::endl;

    stream << "\t\t\trootSignatureDesc.Init_1_0(UINT(rootParameters.size()), rootParameters.data(), 0, nullptr, rootSignatureFlags);" << std::endl;
    if (shaderInputLayout.staticSamplers.empty()) {
    } else {
        stream << "\t\t\trootSignatureDesc.Init_1_0(UINT(rootParameters.size()), rootParameters.data(), UINT(staticSamplers.size()), staticSamplers.data(), rootSignatureFlags);" << std::endl;
    }
    stream << "\t\t\tWRL::ComPtr<ID3DBlob> pRootSignatureBlob, pErrorBlob;" << std::endl;
    stream << "\t\t\tRenderAPI::ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &pRootSignatureBlob, &pErrorBlob));" << std::endl;
    stream << "\t\t\tRenderAPI::ThrowIfFailed(pDevice->CreateRootSignature(0, pRootSignatureBlob->GetBufferPointer(), pRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&s_pRootSignature)));" << std::endl;

    stream << "\t\t}" << std::endl;
    stream << "\t\treturn s_pRootSignature;" << std::endl;
    stream << "\t}" << std::endl;
    stream << "};" << std::endl;
    stream << "}" << std::endl; // Namespace.
}

void generateHostCode(const ast::AbstractSyntaxTree& tree, const ResourceBindingInfo& resourceBindingInfo)
{
    if (!tree.constants.empty()) {
        auto sortedConstantsWithMetadata = tree.constants;
        std::sort(std::begin(sortedConstantsWithMetadata), std::end(sortedConstantsWithMetadata),
            [](const auto& lhs, const auto& rhs) { return lhs.metadata.cppFolder < rhs.metadata.cppFolder; });
        std::vector<ast::Constant> sortedConstants(sortedConstantsWithMetadata.size());
        std::transform(std::begin(sortedConstantsWithMetadata), std::end(sortedConstantsWithMetadata), std::begin(sortedConstants),
            [](const auto& constantWithMetadata) { return constantWithMetadata.item; });

        size_t firstConstantInFile = 0;
        for (size_t lastConstantInFile = 0; firstConstantInFile < lastConstantInFile; firstConstantInFile++) {
            if (sortedConstantsWithMetadata[firstConstantInFile].metadata.cppFolder != sortedConstantsWithMetadata[lastConstantInFile].metadata.cppFolder) {
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
            dx12_render::generateHostStruct(shaderStruct, tree, filePath);
        }
    }

    for (const auto& group : tree.groups) {
        if (group.metadata.shouldExport) {
            const auto filePath = getFilePath(group);
            createFolderContainingFile(filePath);
            dx12_render::generateHostGroup(group, tree, filePath);
        }
    }

    for (const auto& [bindPoint, bindPointBindings] : iter::zip(tree.bindPoints, resourceBindingInfo.bindPoints)) {
        for (const auto& [shaderInputGroupIndex, shaderInputGroupBindings] : iter::zip(bindPoint->shaderInputGroups, bindPointBindings.shaderInputGroups)) {
            const auto& shaderInputGroup = tree.shaderInputGroups[shaderInputGroupIndex];
            if (shaderInputGroup.metadata.shouldExport) {
                const auto filePath = getFilePath(shaderInputGroup);
                createFolderContainingFile(filePath);
                generateHostShaderInputGroup(shaderInputGroup, shaderInputGroupBindings, tree, filePath);
            }
        }

        if (bindPoint.metadata.shouldExport) {
            const auto filePath = getFilePath(bindPoint);
            createFolderContainingFile(filePath);
            dx12_render::generateHostBindPoint(bindPoint, bindPointBindings, tree, filePath);
        }
    }

    for (const auto& [shaderInputLayout, shaderInputLayoutBindings] : iter::zip(tree.shaderInputLayouts, resourceBindingInfo.shaderInputLayouts)) {
        if (shaderInputLayout.metadata.shouldExport) {
            const auto filePath = getFilePath(shaderInputLayout);
            createFolderContainingFile(filePath);
            dx12_render::generateHostShaderInputLayout(shaderInputLayout, shaderInputLayoutBindings, tree, resourceBindingInfo, filePath);
        }
    }
}

static std::string regularTypeCpp(const std::string& type)
{
    static const std::unordered_map<std::string, std::string> mapping {
        { "bool", "uint32_t" },
        { "half2", "DirectX::PackedVector::XMHALF2" },
        { "half4", "DirectX::PackedVector::XMHALF4" },
        { "float", "float" },
        { "float2", "glm::vec2" },
        { "float3", "glm::vec3" },
        { "float4", "glm::vec4" },
        { "float3x3", "glm::mat3" },
        { "float4x4", "glm::mat4" },
        { "int", "int32_t" },
        { "int32_t", "int32_t" },
        { "int64_t", "int64_t" },
        { "int2", "glm::ivec2" },
        { "int3", "glm::ivec3" },
        { "int4", "glm::ivec4" },
        { "uint", "uint32_t" },
        { "uint8_t", "uint8_t" },
        { "uint16_t", "uint16_t" },
        { "uint32_t", "uint32_t" },
        { "uint64_t", "uint64_t" },
        { "uint2", "glm::uvec2" },
        { "uint3", "glm::uvec3" },
        { "uint4", "glm::uvec4" },
    };
    if (auto iter = mapping.find(type); iter != std::end(mapping))
        return iter->second;
    else
        throw std::runtime_error(fmt::format("unknown type '{}' encountered", type));
}

// Some variables need to be stored differently (float3 is stored as float4).
static std::string constantTypeCpp(const std::string& type)
{
    std::unordered_map<std::string, std::string> mapping;
    mapping["bool"] = "uint32_t";
    mapping["float3x3"] = "glm::mat3x4";
    if (auto iter = mapping.find(type); iter != std::end(mapping)) {
        return iter->second;
    } else {
        return regularTypeCpp(type);
    }
}

static size_t sizeOfConstantType(const std::string& type)
{
    // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules
    std::unordered_map<std::string, size_t> mapping;
    mapping["bool"] = 4;
    mapping["half2"] = 2 * 2;
    mapping["half4"] = 4 * 2;
    mapping["float"] = 4;
    mapping["float2"] = 2 * 4;
    mapping["float3"] = 3 * 4;
    mapping["float4"] = 4 * 4;
    mapping["float3x3"] = 12 * 4;
    mapping["float4x4"] = 16 * 4;
    mapping["int"] = 4;
    mapping["int32_t"] = 4;
    mapping["int64_t"] = 8;
    mapping["int2"] = 2 * 4;
    mapping["int3"] = 3 * 4;
    mapping["int4"] = 4 * 4;
    mapping["uint"] = 4;
    mapping["uint8_t"] = 4; // min element size is 4 bytes
    mapping["uint16_t"] = 4; // min element size is 4 bytes
    mapping["uint32_t"] = 4;
    mapping["uint64_t"] = 8;
    mapping["uint2"] = 2 * 4;
    mapping["uint3"] = 3 * 4;
    mapping["uint4"] = 4 * 4;
    if (auto iter = mapping.find(type); iter != std::end(mapping)) {
        return iter->second;
    } else {
        throw std::runtime_error(fmt::format("unknown type '{}' encountered", type));
    }
}
static size_t alignmentOfConstantType(const std::string& type)
{
    // https://maraneshi.github.io/HLSL-ConstantBufferLayoutVisualizer/
    // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules
    std::unordered_map<std::string, size_t> mapping;
    mapping["bool"] = 4;
    mapping["half2"] = 4;
    mapping["half4"] = 4;
    mapping["float"] = 4;
    mapping["float2"] = 4;
    mapping["float3"] = 4;
    mapping["float4"] = 4;
    mapping["float3x3"] = 16;
    mapping["float4x4"] = 16;
    mapping["int"] = 4;
    mapping["int32_t"] = 4;
    mapping["int64_t"] = 8;
    mapping["int2"] = 4;
    mapping["int3"] = 4;
    mapping["int4"] = 4;
    mapping["uint"] = 4;
    mapping["uint8_t"] = 4; // min element size is 4 bytes
    mapping["uint16_t"] = 4; // min element size is 4 bytes
    mapping["uint32_t"] = 4;
    mapping["uint64_t"] = 8;
    mapping["uint2"] = 4;
    mapping["uint3"] = 4;
    mapping["uint4"] = 4;
    if (auto iter = mapping.find(type); iter != std::end(mapping)) {
        return iter->second;
    } else {
        throw std::runtime_error(fmt::format("unknown type '{}' encountered", type));
    }
}

static std::string typeName(const ast::VariableType& type, const ast::AbstractSyntaxTree& tree, bool preferConstantType)
{
    const auto visitor = Tbx::make_visitor(
        [](const ast::UnresolvedType&) -> std::string { throw std::runtime_error("unresolved type encountered"); },
        [](const std::shared_ptr<ast::CustomType>&) -> std::string { throw std::runtime_error("custom type has no type name"); },
        [&](const ast::StructInstance& structRef) -> std::string { return tree.structs[structRef.structIndex]->name; },
        [&](const ast::GroupInstance& groupRef) -> std::string { return tree.groups[groupRef.groupIndex]->name; },
        [&](const ast::BasicType& bt) -> std::string { return preferConstantType ? constantTypeCpp(bt.hlslType) : regularTypeCpp(bt.hlslType); },
        [](const ast::Texture2D) -> std::string { return "RenderAPI::SRVDesc"; },
        [](const ast::RWTexture2D) -> std::string { return "RenderAPI::UAVDesc"; },
        [](const ast::ByteAddressBuffer) -> std::string { return "RenderAPI::SRVDesc"; },
        [](const ast::RWByteAddressBuffer) -> std::string { return "RenderAPI::UAVDesc"; },
        [](const ast::StructuredBuffer) -> std::string { return "RenderAPI::SRVDesc"; },
        [](const ast::RWStructuredBuffer) -> std::string { return "RenderAPI::UAVDesc"; },
        [](const ast::RaytracingAccelerationStructure) -> std::string { return "RenderAPI::SRVDesc"; });
    return std::visit(visitor, type);
}
}
