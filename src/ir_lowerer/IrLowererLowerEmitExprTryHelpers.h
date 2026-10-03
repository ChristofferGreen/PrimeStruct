#pragma once

#include <optional>
#include <string>

#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerReturnEmitStage.h"
#include "IrLowererLowerSumHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"

namespace primec::ir_lowerer {

std::optional<bool> tryLowerEmitExprTryHelper(
    LowerSetupStageState &setupStage,
    LowerReturnEmitStageState &stateOut,
    const CallResolutionAdapters &callResolutionAdapters,
    SumHelpersContext &sumHelpers,
    const Expr &expr,
    const LocalMap &localsIn,
    std::string &error);

} // namespace primec::ir_lowerer
