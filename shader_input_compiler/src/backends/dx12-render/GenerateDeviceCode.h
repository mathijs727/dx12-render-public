#pragma once

namespace ast {
class AbstractSyntaxTree;
}

namespace dx12_render {

struct ResourceBindingInfo;
void generateDeviceCode(const ast::AbstractSyntaxTree&, const ResourceBindingInfo&);

}