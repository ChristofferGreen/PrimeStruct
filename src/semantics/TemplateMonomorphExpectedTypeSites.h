#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "TemplateMonomorphContext.h"
#include "primec/ast/Ast.h"

namespace primec {

// The type a site expects of an expression, as declared.
struct ExpectedType {
  Transform type;
  // The namespace the declaration resolves in: the callee's or struct's for a parameter or field, the current
  // definition's for a binding, an assignment or a return.
  std::string typeNamespace;
  // The current definition's template substitutions, for a declaration inside it; null for a parameter or field.
  const SubstMap *mapping = nullptr;
  const std::unordered_set<std::string> *allowedParams = nullptr;
  // `type.name` is already a resolved type (the type of a local an assignment writes to).
  bool resolved = false;
};

// What a rewrite knows about the expression it is visiting.
struct ExpectedTypeScope {
  const SubstMap &mapping;
  const std::unordered_set<std::string> &allowedParams;
  const std::string &namespacePrefix;
  Context &ctx;
  const LocalTypeMap &locals;
  const std::vector<semantics::ParameterInfo> &params;
  bool allowMathBare = false;
};

// One implicit conversion applied where an expression meets a declared type.
class ExpectedTypeConversion {
public:
  virtual ~ExpectedTypeConversion() = default;
  // Whether the program has the target type at all; the rewrite does nothing otherwise.
  virtual bool enabled(const Context &ctx) const = 0;
  // A cheap test on a declared type name, before it is resolved.
  virtual bool mayBeTarget(const std::string &typeName) const = 0;
  // Whether the brace constructor of `structName` is left alone (the target type's own constructor).
  virtual bool skipsConstructor(const std::string &) const {
    return false;
  }
  // A cheap test on a call argument, before the callee is resolved.
  virtual bool mayConvert(const Expr &arg, const ExpectedTypeScope &scope) const = 0;
  // Converts `arg` in place. `expected` has one entry per candidate callee of a call (same-arity overloads, in
  // overload order; null for a candidate that is generic or has no parameter there) and one entry at every other
  // site. Returns false with `error` set to reject the program.
  virtual bool convert(Expr &arg,
                       const std::vector<const ExpectedType *> &expected,
                       const ExpectedTypeScope &scope,
                       std::string &error) const = 0;
};

// Finds the sites where an argument of `expr` meets a declared type and offers each to the conversions: a call or
// method-call argument and its parameter (a variadic `args<T>` parameter expects a `T`), a brace-constructor
// argument and its field, a binding's initializer and the binding's type, the value of an assignment to a local or
// parameter and its type, and the value returned from the current definition and its return type. Runs on `expr`
// before its arguments are rewritten.
bool rewriteExpectedTypeSites(Expr &expr,
                              const std::vector<const ExpectedTypeConversion *> &conversions,
                              const ExpectedTypeScope &scope,
                              std::string &error);

// Whether a transform on a binding is a qualifier (`mut`, `public`, ...) rather than its type.
bool isFieldQualifierTransformName(const std::string &name);

// The type transform of a binding, parameter or field; null when it has none.
const Transform *bindingTypeTransform(const Expr &binding);

// The last segment of a path.
std::string expectedTypePathLeaf(const std::string &path);

} // namespace primec
