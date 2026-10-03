#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "IrLowererCallHelpers.h"
#include "IrLowererCountAccessHelpers.h"
#include "IrLowererLowerExprEmitSetup.h"
#include "IrLowererLowerReturnCallsSetup.h"
#include "IrLowererLowerSetupStage.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStatementBindingHelpers.h"
#include "IrLowererStringCallHelpers.h"
#include "primec/support/Diagnostics.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

struct LowerReturnEmitInlineContext {
  std::string defPath;
  bool returnsVoid = false;
  bool returnsArray = false;
  LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
  int32_t returnLocal = -1;
  std::vector<size_t> returnJumps;
};

using LowerReturnEmitStatementFn = std::function<bool(const Expr &, LocalMap &)>;
using LowerReturnEmitStructCopyFn = std::function<bool(int32_t, int32_t, int32_t)>;
using LowerReturnEmitFileScopeCleanupFn = std::function<void(const std::vector<int32_t> &)>;
using LowerReturnEmitSimpleFn = std::function<void()>;
using LowerReturnEmitEmitBlockFn = std::function<bool(const Expr &, LocalMap &)>;
using LowerReturnEmitCompareToZeroFn = std::function<bool(LocalInfo::ValueKind, bool)>;
using LowerReturnEmitStringValueForCallFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::StringSource &, int32_t &, bool &)>;
using LowerReturnEmitInlineDefinitionCallFn =
    std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>;
using LowerReturnEmitAppendInstructionSourceRangeFn =
    std::function<void(const std::string &, const Expr &, size_t, size_t)>;

struct LowerReturnEmitStageState;

struct LowerReturnEmitStageInput {
  LowerSetupStageState *setupStage = nullptr;
  OnErrorByDefinition *onErrorByDef = nullptr;
  DiagnosticSinkReport *diagnosticInfo = nullptr;
  std::function<bool(LowerReturnEmitStageState &, std::string &)> consumeStage;
};

struct LowerReturnEmitStageState {
  LowerReturnEmitInlineContext *activeInlineContext = nullptr;
  std::unordered_set<std::string> inlineStack;
  bool hasMathImport = false;

  LowerReturnCallsEmitFileErrorWhyFn emitFileErrorWhy;
  LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;

  std::function<bool(const Expr &)> hasExplicitBindingTypeTransform;
  std::function<bool(const Expr &)> emitFloatLiteral;
  LowerReturnEmitCompareToZeroFn emitCompareToZero;
  GetSetupMathBuiltinNameFn getMathBuiltinName;
  GetSetupMathConstantNameFn getMathConstantName;
  ResolveDefinitionCallFn resolveDefinitionCall;
  ResolveResultExprInfoWithLocalsFn resolveResultExprInfo;
  LowerReturnEmitStringValueForCallFn emitStringValueForCall;
  EmitPrintArgForStatementFn emitPrintArg;

  ExprLocalsPredicateFn emitExpr;
  LowerReturnEmitStatementFn emitStatement;
  LowerReturnEmitInlineDefinitionCallFn emitInlineDefinitionCall;
  Int32ProviderFn allocTempLocal;
  LowerReturnEmitStructCopyFn emitStructCopyFromPtrs;
  LowerReturnEmitStructCopyFn emitStructCopySlots;
  LowerReturnEmitFileScopeCleanupFn emitFileScopeCleanup;
  LowerReturnEmitSimpleFn emitFileScopeCleanupAll;
  LowerReturnEmitSimpleFn pushFileScope;
  LowerReturnEmitSimpleFn popFileScope;
  LowerReturnEmitEmitBlockFn emitBlock;
  LowerReturnEmitAppendInstructionSourceRangeFn appendInstructionSourceRange;
};

bool runLowerReturnEmitStage(const LowerReturnEmitStageInput &input,
                             LowerReturnEmitStageState &stateOut,
                             std::string &errorOut);

} // namespace primec::ir_lowerer
