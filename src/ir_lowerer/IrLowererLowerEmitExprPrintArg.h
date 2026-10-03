#pragma once

#include <string>

#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerReturnEmitStage.h"
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
