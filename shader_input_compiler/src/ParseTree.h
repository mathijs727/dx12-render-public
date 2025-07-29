#pragma once
#include "AbstractSyntaxTree.h" // for AbstractSyntaxTree, BindPoint, Group
#include <memory> // for unique_ptr
#include <variant> // for variant
#include <vector> // for vector

namespace parse {
struct ParseTree;
using Statement = std::variant<std::unique_ptr<ParseTree>, ast::BindPoint, ast::ShaderInputLayout, ast::Group, ast::ShaderInputGroup, ast::Struct, ast::Constant>;

struct ParseTree {
    ast::Output output;
    std::vector<Statement> statements;

    ast::AbstractSyntaxTree toAbstractSyntaxTree() const;
};
}
