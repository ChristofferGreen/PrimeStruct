#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

enum class OperatorClampMinMaxTrigEmitResult { Handled, NotHandled, Error };

using InferClampMinMaxTrigExprKindWithLocalsFn =
    ExprLocalsValueKindFn;
using CombineClampMinMaxTrigNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;

OperatorClampMinMaxTrigEmitResult emitClampMinMaxTrigOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const ExprLocalsPredicateFn &emitExpr,
    const InferClampMinMaxTrigExprKindWithLocalsFn &inferExprKind,
    const CombineClampMinMaxTrigNumericKindsFn &combineNumericKinds,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

} // namespace primec::ir_lowerer
