#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "TemplateMonomorphContext.h"
#include "primec/ast/Ast.h"

namespace primec {

// Call shapes that closures and callable structs need (docs/spec/value-lifecycle.md, Lambdas):
// - `f(x)` where `f` is a local or parameter of a struct type with a `call` method becomes `f.call(x)`;
// - `receiver.field(x)` where `field` has such a type becomes `receiver.field.call(x)`;
// - `Holder{[handler] h}` for a template struct infers the struct's template arguments from its field arguments.
void rewriteCallableShapes(Expr &expr,
                           const SubstMap &mapping,
                           const std::unordered_set<std::string> &allowedParams,
                           const std::string &namespacePrefix,
                           Context &ctx,
                           const LocalTypeMap &locals,
                           const std::vector<semantics::ParameterInfo> &params,
                           bool allowMathBare);

} // namespace primec
