#pragma once

#include "primec/ir_lowerer/IrLowererOperatorArcHyperbolicHelpers.h"

namespace primec::ir_lowerer {

OperatorArcHyperbolicEmitResult emitExpOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const ExprLocalsPredicateFn &emitExpr,
    const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

OperatorArcHyperbolicEmitResult emitLogOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const ExprLocalsPredicateFn &emitExpr,
    const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

OperatorArcHyperbolicEmitResult emitHyperbolicOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const ExprLocalsPredicateFn &emitExpr,
    const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

OperatorArcHyperbolicEmitResult emitArcHyperbolicBuiltinExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const ExprLocalsPredicateFn &emitExpr,
    const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

} // namespace primec::ir_lowerer
