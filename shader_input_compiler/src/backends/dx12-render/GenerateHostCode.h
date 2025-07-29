#pragma once

namespace ast {
class AbstractSyntaxTree;
}

namespace dx12_render {

struct ResourceBindingInfo;
void generateHostCode(const ast::AbstractSyntaxTree& ast, const ResourceBindingInfo& resourceBinding);

}