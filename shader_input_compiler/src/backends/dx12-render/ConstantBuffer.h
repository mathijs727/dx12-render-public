#pragma once
#include "AbstractSyntaxTree.h"
#include <memory> // for shared_ptr
#include <vector> // for vector

namespace dx12_render {

struct ConstantBuffer : public ast::CustomType {
    //std::vector<ast::VariableRef> variables;
};

bool isStandardContantVariableType(const ast::VariableType& type);
bool isCustomConstantVariableType(const ast::VariableType& type);
std::shared_ptr<ConstantBuffer> getConstantBufferVariableType(const ast::VariableType& type);

}