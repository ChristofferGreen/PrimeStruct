#pragma once

#include "primec/ast/Ast.h"

#include <cstdint>
#include <string>
#include <vector>

namespace primec::semantics {

// Builders for the AST nodes that semantics generates (reflection helpers and similar). Each node shape is built in
// exactly one place so every generated helper produces the same AST for the same request.

// Adds the `public` visibility transform.
void appendPublicVisibility(Definition &helper);
void appendPublicVisibility(Expr &binding);

// A binding named `bindingName` of `typeName` (`Box<i32>` becomes transform `Box` with template argument
// `i32`), optionally `mut`.
Expr makeTypeBinding(const std::string &bindingName,
                     const std::string &typeName,
                     const std::string &namespacePrefix,
                     bool isMutable = false);

Expr makeNameExpr(const std::string &name);
Expr makeBoolLiteralExpr(bool value);
Expr makeI32LiteralExpr(uint64_t value);
Expr makeU64LiteralExpr(uint64_t value);
// A `"..."utf8` literal with `"` and `\` escaped.
Expr makeStringLiteralExpr(const std::string &value);

Expr makeReturnStatementExpr(Expr valueExpr);
// A positional two-argument call, such as `equal(left, right)` or `and(left, right)`.
Expr makeBinaryCallExpr(const std::string &name, Expr left, Expr right);
// `Type{}`; a template spelling such as `Box<i32>` splits into name and template arguments.
Expr makeBraceConstructorCall(const std::string &typeName);
// `receiver.field`.
Expr makeFieldAccessExpr(const std::string &receiverName, const std::string &fieldName);
// `comparison(left.field, right.field)`.
Expr makeFieldComparisonExpr(const std::string &comparisonName,
                             const std::string &leftReceiverName,
                             const std::string &rightReceiverName,
                             const std::string &fieldName);
// A call with a body block: `name() { ... }`.
Expr makeEnvelopeExpr(const std::string &name, std::vector<Expr> bodyArguments);
Expr makeIfStatementExpr(Expr conditionExpr, Expr thenEnvelopeExpr, Expr elseEnvelopeExpr);
// `Result.ok()`.
Expr makeOkResultExpr();

} // namespace primec::semantics
