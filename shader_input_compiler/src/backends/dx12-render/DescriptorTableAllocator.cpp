
#include "DescriptorTableAllocator.h"
#include "AbstractSyntaxTree.h" // for Variable
#include "backends/dx12-render/HLSLRegister.h" // for RegisterType (p...
#include "backends/dx12-render/RegisterAllocation.h" // for DescriptorTable...
#include <tbx/disable_all_warnings.h> // for DISABLE_WARNING...
DISABLE_WARNINGS_PUSH()
#include <cppitertools/enumerate.hpp> // for Enumerable, enu...
DISABLE_WARNINGS_POP()
#include <algorithm> // for sort
#include <cassert> // for assert
#include <iterator> // for back_insert_ite...
#include <limits> // for numeric_limits
#include <memory> // for _Simple_types
#include <type_traits> // for move
#include <utility> // for end, begin, max

namespace dx12_render {

DescriptorTableAllocator::DescriptorTableAllocator(std::span<const uint32_t> descriptorsPerRegisterType)
{
    // Create ranges.
    for (const auto [registerTypeInt, numDescriptors] : iter::enumerate(descriptorsPerRegisterType)) {
        const RegisterType registerType = static_cast<RegisterType>(registerTypeInt);
        if (numDescriptors > 0)
            m_ranges.push_back(DescriptorRangeAllocator { .type = registerType, .maxSize = numDescriptors });
    }

    // Sort ranges by size such that unbounded descriptor range should always come last.
    std::sort(std::begin(m_ranges), std::end(m_ranges), [](const auto& lhs, const auto& rhs) { return lhs.maxSize < rhs.maxSize; });
    assert(m_ranges.size() < 2 || m_ranges[m_ranges.size() - 2].maxSize != ast::Variable::Unbounded);

    // Set the base descriptor offset (first descriptor of that range).
    uint32_t descriptorOffset = 0;
    for (auto& range : m_ranges) {
        range.baseDescriptorOffset = descriptorOffset;
        if (range.maxSize == std::numeric_limits<uint32_t>::max()) // Prevent unsigned integer overflow.
            descriptorOffset = range.maxSize;
        else
            descriptorOffset += range.maxSize;
    }
}

bool DescriptorTableAllocator::tryAllocate(const ast::Variable& variable, uint32_t variableIdx)
{
    for (auto& range : m_ranges) {
        if (range.type == getDX12RenderRegisterType(variable.type)) {
            // Check if there is space left in this DescriptorRange to add the new descriptor(s).
            const uint32_t descriptorCount = variable.arrayCount == 0 ? 1 : variable.arrayCount;
            if (descriptorCount != ast::Variable::Unbounded) {
                const uint32_t spaceLeft = range.maxSize - range.currentOffsetInRange;
                if (spaceLeft < descriptorCount)
                    return false;
            } else {
                if (range.currentOffsetInRange == ast::Variable::Unbounded)
                    return false;
            }

            // Add new descriptors to the descriptor range.
            // clang-format off
            range.currentBindings.push_back({
                .variableIdx = variableIdx,
                .descriptorOffset = range.baseDescriptorOffset + range.currentOffsetInRange,
                .numDescriptors = descriptorCount
            });
            // clang-format on
            if (descriptorCount == ast::Variable::Unbounded)
                range.currentOffsetInRange = ast::Variable::Unbounded;
            else
                range.currentOffsetInRange += descriptorCount;

            // TODO(Mathijs): more elegant solution that has better time complexity.
            for (const auto& newShaderStage : m_currentShaderStages) {
                if (std::find(std::begin(range.shaderStages), std::end(range.shaderStages), newShaderStage) == std::end(range.shaderStages)) {
                    range.shaderStages.push_back(newShaderStage);
                }
            }

            return true;
        }
    }
    return false;
}

std::optional<DescriptorTable> DescriptorTableAllocator::createDescriptorTable() const
{
    // Compute the last descriptor offset that we know for sure we will use.
    // This excludes the descriptors used by an unbounded range.
    uint32_t numKnownDescriptors = 0;
    std::optional<uint32_t> optUnboundedVariableIdx;
    std::vector<typename DescriptorTable::Descriptor> bindings;
    for (const auto& range : m_ranges) {
        for (const auto& binding : range.currentBindings) {
            if (binding.numDescriptors == ast::Variable::Unbounded) {
                numKnownDescriptors = std::max(numKnownDescriptors, binding.descriptorOffset);
                assert(!optUnboundedVariableIdx);
                optUnboundedVariableIdx = binding.variableIdx;
            } else {
                numKnownDescriptors = std::max(numKnownDescriptors, binding.descriptorOffset + binding.numDescriptors);
            }
        }
        std::copy(std::begin(range.currentBindings), std::end(range.currentBindings), std::back_inserter(bindings));
    }

    if (bindings.empty())
        return {};

    return DescriptorTable {
        .descriptors = std::move(bindings),
        .numKnownDescriptors = numKnownDescriptors,
        .optUnboundedVariableIdx = optUnboundedVariableIdx
    };
}

DescriptorTableLayout DescriptorTableAllocator::createDescriptorTableLayout() const
{
    using Range = typename DescriptorTableLayout::Range;

    std::vector<Range> ranges;
    for (const auto& range : m_ranges) {
        ranges.push_back(Range {
            .baseDescriptorOffset = range.baseDescriptorOffset,
            .numDescriptors = range.maxSize,
            .type = range.type });
    }
    return DescriptorTableLayout { .ranges = ranges };
}

void DescriptorTableAllocator::startShaderInputGroup()
{
    for (auto& range : m_ranges) {
        range.currentOffsetInRange = 0;
        range.currentBindings.clear();
    }
}

}
