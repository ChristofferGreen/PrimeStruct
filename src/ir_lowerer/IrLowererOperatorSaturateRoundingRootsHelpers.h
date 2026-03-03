#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

enum class OperatorSaturateRoundingRootsEmitResult { Handled, NotHandled, Error };

using EmitSaturateExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferSaturateExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using AllocSaturateTempLocalFn = std::function<int32_t()>;

OperatorSaturateRoundingRootsEmitResult emitSaturateRoundingRootsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitSaturateExprWithLocalsFn &emitExpr,
    const InferSaturateExprKindWithLocalsFn &inferExprKind,
    const AllocSaturateTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

} // namespace primec::ir_lowerer
