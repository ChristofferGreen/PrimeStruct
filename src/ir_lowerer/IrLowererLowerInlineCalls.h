#pragma once

#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerReturnEmitStage.h"
#include "primec/ir_lowerer/IrLowererOnErrorHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/support/Diagnostics.h"

namespace primec::ir_lowerer {

bool emitInlineDefinitionCallImpl(
    LowerSetupStageState &setupStage,
    LowerReturnEmitStageState &stateOut,
    const CallResolutionAdapters &callResolutionAdapters,
    DiagnosticSinkReport *diagnosticInfo,
    OnErrorByDefinition *onErrorByDefPtr,
    const Expr &callExpr,
    const Definition &callee,
    const LocalMap &callerLocals,
    bool requireValue,
    std::string &error);

} // namespace primec::ir_lowerer
