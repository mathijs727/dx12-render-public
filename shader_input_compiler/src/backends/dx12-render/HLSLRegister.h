#pragma once
#include "AbstractSyntaxTree.h"
#include <cstdint>

namespace dx12_render {

enum class RegisterType {
    Unknown,
    ConstantBuffer,
    ShaderResource,
    UnorderedAccess,
    Sampler
};

struct HLSLRegister {
    RegisterType registerType { RegisterType::Unknown };
    uint32_t registerSpace { 0xFFFFFFFF };
    uint32_t baseRegister { 0xFFFFFFFF };
};

RegisterType getDX12RenderRegisterType(const ast::VariableType& type);

}