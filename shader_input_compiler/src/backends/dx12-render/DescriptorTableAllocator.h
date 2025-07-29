#pragma once
#include "RegisterAllocation.h" // for DescriptorTableInstance, DescriptorT...
#include <optional> // for optional
#include <span> // for span
#include <cstdint> // for uint32_t
#include <vector> // for vector
namespace ast {
enum class ShaderStage;
struct Variable;
}
namespace dx12_render {
enum class RegisterType;
}

namespace dx12_render {

class DescriptorTableAllocator {
private:
    struct DescriptorRangeAllocator {
        // Persistent between input groups binding to the same bind point.
        RegisterType type;
        uint32_t maxSize;
        uint32_t baseDescriptorOffset = 0;
        std::vector<ast::ShaderStage> shaderStages;

        // Get cleared every time startShaderInputGroup() is called.
        uint32_t currentOffsetInRange = 0;
        std::vector<typename DescriptorTable::Descriptor> currentBindings {};
    };

public:
    DescriptorTableAllocator(std::span<const uint32_t> descriptorsPerRegisterType);

    bool tryAllocate(const ast::Variable& variable, uint32_t variableIdx);
    void startShaderInputGroup();

    std::optional<DescriptorTable> createDescriptorTable() const;
    DescriptorTableLayout createDescriptorTableLayout() const;

private:
    std::vector<ast::ShaderStage> m_currentShaderStages;
    std::vector<DescriptorRangeAllocator> m_ranges;
};

}