#pragma once

#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

enum class MissingReturnBehavior { Error, Void };

struct ReturnInferenceOptions {
  MissingReturnBehavior missingReturnBehavior = MissingReturnBehavior::Error;
  bool includeDefinitionReturnExpr = false;
};

using InferBindingIntoLocalsFn = std::function<bool(const Expr &, bool, LocalMap &, std::string &)>;
using InferValueKindFromLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ExpandMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;

bool inferDefinitionReturnType(const Definition &def,
                               LocalMap localsForInference,
                               const InferBindingIntoLocalsFn &inferBindingIntoLocals,
                               const InferValueKindFromLocalsFn &inferExprKindFromLocals,
                               const InferValueKindFromLocalsFn &inferArrayElementKindFromLocals,
                               const ExpandMatchToIfFn &expandMatchToIf,
                               const ReturnInferenceOptions &options,
                               ReturnInfo &outInfo,
                               std::string &error);

} // namespace primec::ir_lowerer
