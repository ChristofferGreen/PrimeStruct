#pragma once

#include <string>

#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererLowerReturnEmitStage.h"
#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"

namespace primec::ir_lowerer {

bool emitPrintArgImpl(
    LowerSetupStageState &setupStage,
    LowerReturnEmitStageState &stateOut,
    const CallResolutionAdapters &callResolutionAdapters,
    const Expr &arg,
    const LocalMap &localsIn,
    const PrintBuiltin &builtin,
    std::string &error);

} // namespace primec::ir_lowerer
