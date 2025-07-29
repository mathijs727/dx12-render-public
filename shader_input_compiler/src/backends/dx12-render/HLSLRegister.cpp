#include "HLSLRegister.h"
#include "AbstractSyntaxTree.h" // for ByteAddressBuffer, CustomType, RWByt...
#include <memory> // for shared_ptr
#include <stdexcept> // for runtime_error
#include <tbx/variant_helper.h> // for make_visitor
#include <variant> // for visit

namespace dx12_render {

RegisterType getDX12RenderRegisterType(const ast::VariableType& type)
{
    const auto visitor = Tbx::make_visitor(
        // Register allocation should use the custom types (ConstantBuffer) instead of using the BasicTypes directly.
        //[&](ast::BasicType) { return RegisterType::ConstantBuffer; },
        [&](const std::shared_ptr<ast::CustomType>&) { return RegisterType::ConstantBuffer; },
        [&](ast::Texture2D) { return RegisterType::ShaderResource; },
        [&](ast::RWTexture2D) { return RegisterType::UnorderedAccess; },
        [&](ast::ByteAddressBuffer) { return RegisterType::ShaderResource; },
        [&](ast::RWByteAddressBuffer) { return RegisterType::UnorderedAccess; },
        [&](ast::StructuredBuffer) { return RegisterType::ShaderResource; },
        [&](ast::RWStructuredBuffer) { return RegisterType::UnorderedAccess; },
        [&](ast::RaytracingAccelerationStructure) { return RegisterType::ShaderResource; },
        [](auto) -> RegisterType { throw std::runtime_error("failed determine register type"); });
    return std::visit(visitor, type);
}

}