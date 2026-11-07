#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "TemplateMonomorphContext.h"
#include "TemplateMonomorphExpectedTypeSites.h"
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
// (docs/spec/value-lifecycle.md, Function values), at every site `rewriteExpectedTypeSites` finds. The argument is a
// lambda, a local or parameter of a callable struct type, a value of such a type, or a named free function (through
// the forwarder the lambda-closures pass generated for it). The inserted `/__fn_box` is monomorphized like an
// explicit conversion. A call converts for its first same-arity candidate callee.
class FunctionValueConversion final : public ExpectedTypeConversion {
public:
  bool enabled(const Context &ctx) const override;
  bool mayBeTarget(const std::string &typeName) const override;
  bool skipsConstructor(const std::string &structName) const override;
  bool mayConvert(const Expr &arg, const ExpectedTypeScope &scope) const override;
  bool convert(Expr &arg,
               const std::vector<const ExpectedType *> &expected,
               const ExpectedTypeScope &scope,
               std::string &error) const override;
};

} // namespace primec
