#pragma once

#include "primec/ir_lowerer/IrLowererOperatorClampMinMaxTrigHelpers.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

OperatorClampMinMaxTrigEmitResult emitAngleTrigOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const ExprLocalsPredicateFn &emitExpr,
    const InferClampMinMaxTrigExprKindWithLocalsFn &inferExprKind,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

} // namespace primec::ir_lowerer
