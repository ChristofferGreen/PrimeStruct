#pragma once

#include "IrLowererOperatorClampMinMaxTrigHelpers.h"

namespace primec::ir_lowerer {

OperatorClampMinMaxTrigEmitResult emitAngleTrigOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitClampMinMaxTrigExprWithLocalsFn &emitExpr,
    const InferClampMinMaxTrigExprKindWithLocalsFn &inferExprKind,
    const AllocClampMinMaxTrigTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

} // namespace primec::ir_lowerer
