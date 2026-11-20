#pragma once

#include <string>
#include <vector>

#include "TemplateMonomorphContext.h"
#include "primec/ast/Ast.h"
#include "SemanticsHelpers.h"

namespace primec {

// An `[auto]` local whose initializer is `Result.ok`, `Result.map` or `Result.and_then` is the stdlib `Result<T, E>`
// sum when `/std/result/*` is visible from the definition: the type is written out here (`[Result<Box, i32>]`) so
// the sum is instantiated like any declared one and its drop destroys the payload (docs/spec/value-lifecycle.md).
// `binding` is left alone, and stays a packed builtin `Result`, when the sum is not imported or when `T` or `E`
// cannot be read from the initializer and the definition (the error type of `Result.ok` comes from the definition's
// `on_error` or its `Result` return type).
void materializeAutoResultBindingType(Expr &binding,
                                      const std::vector<semantics::ParameterInfo> &params,
                                      const LocalTypeMap &locals,
                                      const std::string &namespacePrefix,
                                      bool allowMathBare,
                                      Context &ctx);

} // namespace primec
