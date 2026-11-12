#pragma once

#include <string>

#include "primec/ast/Ast.h"
#include "primec/ir_lowerer/IrLowererResultHelpers.h"
#include "primec/ir_lowerer/IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

// The type text of a parameter's first type-like transform (qualifiers and
// transforms with arguments are skipped), with template arguments trimmed and
// re-joined by ", "; empty when there is none.
std::string extractParameterTypeName(const Expr &paramExpr);

struct KeyValueConstructorRewriteOptions {
  // Also take the builtin `map(...)` constructor call as a published constructor.
  bool acceptBuiltinMap = false;
  // Rewrite to the resolved callee's own path (keeps the overload the call
  // resolved to) instead of the shared canonical constructor family path.
  bool useResolvedCalleePath = false;
};

// True for a call of the published key-value constructor surface (and, with
// `acceptBuiltinMap`, the builtin map constructor).
bool isPublishedKeyValueConstructorExpr(const Expr &callExpr, bool acceptBuiltinMap);

// Rewrites a published key-value constructor call into `rewrittenExpr`, filling
// the template arguments from the fallback kinds or the callee's parameters.
bool rewritePublishedKeyValueConstructorExpr(const Expr &callExpr,
                                             LocalInfo::ValueKind fallbackKeyKind,
                                             LocalInfo::ValueKind fallbackValueKind,
                                             const ResolveCallDefinitionFn &resolveDefinitionCall,
                                             Expr &rewrittenExpr,
                                             const KeyValueConstructorRewriteOptions &options);

} // namespace primec::ir_lowerer
