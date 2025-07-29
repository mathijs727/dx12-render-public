#pragma once
#include <cstdint> // for uint32_t
#include <optional> // for optional
#include <string>
#include <string_view>
#include <vector> // for vector

namespace ast {
class AbstractSyntaxTree;
}
namespace dx12_render {
enum class RegisterType;
}

namespace dx12_render {

// Different types of root parameters. For now only DescriptorTable but may be extended to CBV or direct variables.
struct DescriptorTable {
    struct Descriptor {
        uint32_t variableIdx;
        uint32_t descriptorOffset;
        uint32_t numDescriptors;
    };
    std::vector<Descriptor> descriptors;
    uint32_t numKnownDescriptors; // Number of descriptors EXCLUDING unbounded descriptor ranges.
    std::optional<uint32_t> optUnboundedVariableIdx;
};
struct DescriptorTableLayout {
    struct Range {
        uint32_t baseDescriptorOffset;
        uint32_t numDescriptors;
        RegisterType type;
    };
    std::vector<Range> ranges;
};

struct ShaderInputGroupBindings {
    struct RootParameter {
        uint32_t rootParameterOffset;
        DescriptorTable descriptorTable;
    };
    std::vector<RootParameter> rootParameters;
};

struct BindPointBindings {
    struct RootParameter {
        uint32_t rootParameterOffset;
        DescriptorTableLayout descriptorTableLayout;
    };
    std::vector<RootParameter> rootParameters;
    std::vector<ShaderInputGroupBindings> shaderInputGroups;
};

struct ShaderInputLayoutBindings {
    std::vector<uint32_t> bindPointsRootParameterIndices;
    std::vector<uint32_t> constantRootParameterIndices;
    std::vector<uint32_t> cbvRootParameterIndices;
};

struct ResourceBindingInfo {
    std::vector<BindPointBindings> bindPoints;
    std::vector<ShaderInputLayoutBindings> shaderInputLayouts;
};

ResourceBindingInfo allocateRegisters(ast::AbstractSyntaxTree& ast);

std::string getGroupVariableMangledName(std::string_view groupInstanceName, std::string_view variableName);

}
