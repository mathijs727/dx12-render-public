#include "RegisterAllocation.h"
#include "AbstractSyntaxTree.h" // for Variable, AbstractSyntaxTree
#include "ConstantBuffer.h" // for ConstantBuffer, isStandardCont...
#include "DescriptorTableAllocator.h" // for DescriptorTableAllocator
#include "HLSLRegister.h" // for getDX12RenderRegisterType
#include <tbx/disable_all_warnings.h> // for DISABLE_WARNINGS_POP, DISABLE_...
DISABLE_WARNINGS_PUSH()
#include <cppitertools/enumerate.hpp> // for Enumerable, enumerate
#include <magic_enum/magic_enum.hpp> // for enum_integer, enum_count
DISABLE_WARNINGS_POP()
#include <algorithm> // for transform, sort
#include <array> // for array
#include <cstddef> // for size_t
#include <fmt/core.h> // for format, format_string
#include <iterator> // for back_insert_iterator, back_ins...
#include <memory> // for _Simple_types, make_shared
#include <numeric> // for accumulate
#include <optional> // for optional, _Optional_construct_...
#include <span> // for span
#include <tbx/error_handling.h> // for assert_always
#include <type_traits> // for move
#include <utility> // for begin, end, max, fill
#include <vector> // for vector, _Vector_const_iterator

namespace dx12_render {

static void addConstantBuffer(ast::AbstractSyntaxTree& tree)
{
    // Combine all constants into a single ConstantBuffer.
    for (auto& shaderInputGroup : tree.shaderInputGroups) {
        bool hasConstants = false;
        for (const auto& variable : shaderInputGroup->variables) {
            hasConstants |= isStandardContantVariableType(variable.type);
        }

        if (hasConstants) {
            auto pConstantBuffer = std::make_shared<ConstantBuffer>();
            shaderInputGroup->variables.push_back(ast::Variable {
                .name = "Internal",
                .type = std::make_shared<ConstantBuffer>(),
                .arrayCount = 0 });
        }
    }
}

static void flattenInputGroups(ast::AbstractSyntaxTree& tree)
{
    // Flatten variables in *Group* into containing *ShaderInputGroup*(s).
    for (ast::ShaderInputGroup& shaderInputGroup : tree.shaderInputGroups) {
        // Calling push_back() inside the for-loop cancels the iterators.
        // We only want to loop over the original variables; not the ones added during flattening.
        const size_t numVariables = shaderInputGroup.variables.size();
        for (size_t variableIndex = 0; variableIndex < numVariables; ++variableIndex) {
            const ast::Variable variable = shaderInputGroup.variables[variableIndex]; // Copy because push_back will invalidate pointers.
            if (!std::holds_alternative<ast::GroupInstance>(variable.type))
                continue;
            Tbx::assert_always(variable.arrayCount == 0);

            const auto& group = tree.groups[std::get<ast::GroupInstance>(variable.type).groupIndex];
            for (ast::Variable groupVariable : group->variables) {
                Tbx::assert_always(!std::holds_alternative<ast::GroupInstance>(groupVariable.type)); // Nested input groups are not allowed.
                groupVariable.name = getGroupVariableMangledName(variable.name, groupVariable.name);
                shaderInputGroup.variables.push_back(groupVariable);
            }
        }
    }
}

ResourceBindingInfo allocateRegisters(ast::AbstractSyntaxTree& abstractSyntaxTree)
{
    static constexpr size_t numRegisterTypes = magic_enum::enum_count<RegisterType>();

    flattenInputGroups(abstractSyntaxTree);
    addConstantBuffer(abstractSyntaxTree);

    ResourceBindingInfo out {};
    std::transform(
        std::begin(abstractSyntaxTree.bindPoints), std::end(abstractSyntaxTree.bindPoints), std::back_inserter(out.bindPoints),
        [&abstractSyntaxTree](const auto& bindPoint) {
            // Compute the maximum register requirements (per register type) for any single input group.
            std::array<uint32_t, numRegisterTypes> numBoundedRegisters {};
            std::array<uint32_t, numRegisterTypes> numUnboundedVariables {};
            for (const auto shaderInputGroupIndex : bindPoint->shaderInputGroups) {
                std::array<uint32_t, numRegisterTypes> inputGroupNumBoundedRegisters {};
                std::array<uint32_t, numRegisterTypes> inputGroupNumUnboundedVariables {};

                const auto& shaderInputGroup = abstractSyntaxTree.shaderInputGroups[shaderInputGroupIndex];
                for (const auto& variable : shaderInputGroup->variables) {
                    // Constants are allocated through the ConstantBuffer custom type. Groups have their individual variables allocated (see flattenInputGroups).
                    if (isStandardContantVariableType(variable.type) || std::holds_alternative<ast::GroupInstance>(variable.type))
                        continue;

                    const auto registerType = getDX12RenderRegisterType(variable.type);
                    if (variable.arrayCount == ast::Variable::Unbounded)
                        inputGroupNumUnboundedVariables[magic_enum::enum_integer(registerType)]++;
                    else
                        inputGroupNumBoundedRegisters[magic_enum::enum_integer(registerType)] += std::max(variable.arrayCount, 1u); // 0 if not an array
                }

                for (size_t i = 0; i < numBoundedRegisters.size(); i++) {
                    numBoundedRegisters[i] = std::max(numBoundedRegisters[i], inputGroupNumBoundedRegisters[i]);
                    numUnboundedVariables[i] = std::max(numUnboundedVariables[i], inputGroupNumUnboundedVariables[i]);
                }
            }

            // Compute how many DescriptorTableAllocators we need ahead of time.
            // NOTE(Mathijs): currently only using DescriptorTables; but root descriptors or root constants could be added here.
            std::vector<DescriptorTableAllocator> registerAllocators;
            for (const auto [registerTypeInt, numUnboundedRangesOfType] : iter::enumerate(numUnboundedVariables)) {
                for (uint32_t i = 0; i < numUnboundedRangesOfType; i++) {
                    // Try to allocate all bounded registers and one range of unbounded registers.
                    auto numDescriptors = numBoundedRegisters;
                    numDescriptors[registerTypeInt] = ast::Variable::Unbounded;
                    // We have allocated all bounded registers; subsequent iterations should only allocate unbounded ranges.
                    std::fill(std::begin(numBoundedRegisters), std::end(numBoundedRegisters), 0u);
                    registerAllocators.push_back(DescriptorTableAllocator(numDescriptors));
                }
            }
            // Handle the case were there were no unbounded variables and we need to create a descriptor table.
            if (std::accumulate(std::begin(numBoundedRegisters), std::end(numBoundedRegisters), 0u) > 0)
                registerAllocators.push_back(DescriptorTableAllocator(numBoundedRegisters));

            BindPointBindings bindPointBindings {};
            for (auto& shaderInputGroupIndex : bindPoint->shaderInputGroups) {
                // Reset the register allocators.
                for (auto& registerAllocator : registerAllocators)
                    registerAllocator.startShaderInputGroup();

                const auto& shaderInputGroup = abstractSyntaxTree.shaderInputGroups[shaderInputGroupIndex];
                // Sort the variable declarations based on their count BEFORE allocating the registers.
                // This way we always allocate unbounded registers before bounded ones.
                const auto& variables = shaderInputGroup->variables;
                std::vector<uint32_t> variableIndices(variables.size());
                std::iota(std::begin(variableIndices), std::end(variableIndices), 0);
                std::sort(std::begin(variableIndices), std::end(variableIndices),
                    [&](auto lhs, auto rhs) { return variables[lhs].arrayCount < variables[rhs].arrayCount; });
                for (const auto& variableIdx : variableIndices) {
                    const auto& variable = variables[variableIdx];
                    // Constants are allocated through the ConstantBuffer custom type. Groups have their individual variables allocated (see flattenInputGroups).
                    if (isStandardContantVariableType(variable.type) || std::holds_alternative<ast::GroupInstance>(variable.type))
                        continue;

                    for (auto& registerAllocator : registerAllocators) {
                        if (registerAllocator.tryAllocate(variable, (uint32_t)variableIdx))
                            break;
                    }
                }

                ShaderInputGroupBindings shaderInputGroupBindings;
                for (const auto& [rootParameterOffset, registerAllocator] : iter::enumerate(registerAllocators)) {
                    auto optDescriptorTable = registerAllocator.createDescriptorTable();
                    if (!optDescriptorTable)
                        continue;

                    using RootParameter = typename ShaderInputGroupBindings::RootParameter;
                    RootParameter rootParameter {
                        .rootParameterOffset = (uint32_t)rootParameterOffset,
                        .descriptorTable = std::move(*optDescriptorTable)
                    };
                    shaderInputGroupBindings.rootParameters.emplace_back(std::move(rootParameter));
                }
                bindPointBindings.shaderInputGroups.push_back(std::move(shaderInputGroupBindings));
            }
            Tbx::assert_always(bindPointBindings.shaderInputGroups.size() == bindPoint->shaderInputGroups.size());

            for (const auto& [rootParameterOffset, registerAllocator] : iter::enumerate(registerAllocators)) {
                using RootParameter = typename BindPointBindings::RootParameter;
                RootParameter rootParameter {
                    .rootParameterOffset = (uint32_t)rootParameterOffset,
                    .descriptorTableLayout = registerAllocator.createDescriptorTableLayout()
                };
                bindPointBindings.rootParameters.emplace_back(std::move(rootParameter));
            }

            return bindPointBindings;
        });
    Tbx::assert_always(out.bindPoints.size() == abstractSyntaxTree.bindPoints.size());

    std::transform(
        std::begin(abstractSyntaxTree.shaderInputLayouts), std::end(abstractSyntaxTree.shaderInputLayouts),
        std::back_inserter(out.shaderInputLayouts),
        [&](const auto& shaderInputLayout) {
            uint32_t rootParameterIndex = 0;
            ShaderInputLayoutBindings shaderInputLayoutBindings {};
            for (const ast::RootConstant& rootConstant : shaderInputLayout->rootConstants) {
                shaderInputLayoutBindings.constantRootParameterIndices.push_back(rootParameterIndex++);
            }
            for (const ast::RootConstantBufferView& rootConstantBufferView : shaderInputLayout->rootConstantBufferViews) {
                shaderInputLayoutBindings.cbvRootParameterIndices.push_back(rootParameterIndex++);
            }
            for (const ast::BindPointReference& bindPointRef : shaderInputLayout->bindPoints) {
                shaderInputLayoutBindings.bindPointsRootParameterIndices.push_back(rootParameterIndex);
                rootParameterIndex += (uint32_t)out.bindPoints[bindPointRef.bindPointIndex].rootParameters.size();
            }
            return shaderInputLayoutBindings;
        });

    return out;
}

std::string getGroupVariableMangledName(std::string_view groupInstanceName, std::string_view variableName)
{
    return fmt::format("__{}_{}", groupInstanceName, variableName);
}

}
