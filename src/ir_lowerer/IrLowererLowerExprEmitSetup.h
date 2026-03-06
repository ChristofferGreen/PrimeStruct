#pragma once

#include <functional>
#include <string>

#include "IrLowererFlowHelpers.h"

namespace primec::ir_lowerer {

using EmitExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using LowerExprEmitMovePassthroughCallFn =
    std::function<UnaryPassthroughCallResult(const Expr &, const LocalMap &, const EmitExprWithLocalsFn &, std::string &)>;

struct LowerExprEmitSetupInput {};

bool runLowerExprEmitSetup(const LowerExprEmitSetupInput &input,
                           LowerExprEmitMovePassthroughCallFn &emitMovePassthroughCallOut,
                           std::string &errorOut);

} // namespace primec::ir_lowerer
