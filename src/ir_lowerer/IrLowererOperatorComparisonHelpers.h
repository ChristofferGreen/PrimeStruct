#pragma once

#include <functional>
#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

enum class OperatorComparisonEmitResult { Handled, NotHandled, Error };

using ComparisonKindFn = std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using EmitComparisonToZeroFn = std::function<bool(LocalInfo::ValueKind, bool)>;

OperatorComparisonEmitResult emitComparisonOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        const ExprLocalsPredicateFn &emitExpr,
                                                        const ExprLocalsValueKindFn &inferExprKind,
                                                        const ComparisonKindFn &comparisonKind,
                                                        const EmitComparisonToZeroFn &emitCompareToZero,
                                                        const Int32ProviderFn &allocTempLocal,
                                                        std::vector<IrInstruction> &instructions,
                                                        std::string &error);

} // namespace primec::ir_lowerer
