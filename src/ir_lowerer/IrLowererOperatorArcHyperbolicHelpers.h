#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

enum class OperatorArcHyperbolicEmitResult { Handled, NotHandled, Error };

using EmitArcHyperbolicExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferArcHyperbolicExprKindWithLocalsFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using AllocArcHyperbolicTempLocalFn = std::function<int32_t()>;

OperatorArcHyperbolicEmitResult emitArcHyperbolicOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitArcHyperbolicExprWithLocalsFn &emitExpr,
    const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
    const AllocArcHyperbolicTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

} // namespace primec::ir_lowerer
