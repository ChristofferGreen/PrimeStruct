#pragma once

#include <functional>
#include <string>

#include "IrLowererFlowHelpers.h"

namespace primec::ir_lowerer {

using EmitExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using LowerExprEmitUnaryPassthroughCallFn =
    std::function<UnaryPassthroughCallResult(const Expr &, const LocalMap &, const EmitExprWithLocalsFn &, std::string &)>;
using LowerExprEmitMovePassthroughCallFn = LowerExprEmitUnaryPassthroughCallFn;
using LowerExprEmitUploadPassthroughCallFn = LowerExprEmitUnaryPassthroughCallFn;
using LowerExprEmitReadbackPassthroughCallFn = LowerExprEmitUnaryPassthroughCallFn;

struct LowerExprEmitSetupInput {};

bool runLowerExprEmitSetup(const LowerExprEmitSetupInput &input,
                           LowerExprEmitMovePassthroughCallFn &emitMovePassthroughCallOut,
                           LowerExprEmitUploadPassthroughCallFn &emitUploadPassthroughCallOut,
                           LowerExprEmitReadbackPassthroughCallFn &emitReadbackPassthroughCallOut,
                           std::string &errorOut);

} // namespace primec::ir_lowerer
