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

// A closure passed where a `Function<R, A...>` is expected converts as if wrapped in `Function<R, A...>{...}`
// (docs/spec/value-lifecycle.md, Function values): a call or method-call argument for a `Function` parameter, a
// brace-constructor field argument, a binding initializer, an assignment to a `Function` local, and the value
// returned from a definition that returns a `Function`. The argument is a lambda, a local or parameter of a callable
// struct type, a value of such a type, or a named free function (through the forwarder the lambda-closures pass
// generated for it). Runs on `expr` before its arguments are rewritten, so the inserted `/__fn_box` is
// monomorphized like an explicit conversion.
bool rewriteImplicitFunctionConversions(Expr &expr,
                                        const SubstMap &mapping,
                                        const std::unordered_set<std::string> &allowedParams,
                                        const std::string &namespacePrefix,
                                        Context &ctx,
                                        const LocalTypeMap &locals,
                                        const std::vector<semantics::ParameterInfo> &params,
                                        bool allowMathBare,
                                        std::string &error);

} // namespace primec
