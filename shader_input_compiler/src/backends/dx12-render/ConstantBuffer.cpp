#include "ConstantBuffer.h"
#include "AbstractSyntaxTree.h"
#include <variant>

namespace dx12_render {

bool isStandardContantVariableType(const ast::VariableType& type)
{
    return (std::holds_alternative<ast::StructInstance>(type) || std::holds_alternative<ast::BasicType>(type));
}
bool isCustomConstantVariableType(const ast::VariableType& type)
{
    return std::holds_alternative<std::shared_ptr<ast::CustomType>>(type);
}

std::shared_ptr<ConstantBuffer> getConstantBufferVariableType(const ast::VariableType& type)
{
    auto pCustomType = std::get<std::shared_ptr<ast::CustomType>>(type);
    return std::dynamic_pointer_cast<ConstantBuffer>(pCustomType);
}

}