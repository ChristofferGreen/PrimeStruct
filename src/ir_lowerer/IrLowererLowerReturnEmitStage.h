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
#include "IrLowererStringCallHelpers.h"

namespace primec::ir_lowerer {

struct LowerReturnEmitInlineContext {
  std::string defPath;
  bool returnsVoid = false;
  bool returnsArray = false;
  LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
  int32_t returnLocal = -1;
  std::vector<size_t> returnJumps;
};

using LowerReturnEmitExprFn = std::function<bool(const Expr &, const LocalMap &)>;
using LowerReturnEmitStatementFn = std::function<bool(const Expr &, LocalMap &)>;
using LowerReturnEmitAllocTempLocalFn = std::function<int32_t()>;
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

struct LowerReturnEmitStageInput {
  LowerSetupStageState *setupStage = nullptr;
  OnErrorByDefinition *onErrorByDef = nullptr;
};

struct LowerReturnEmitStageState {
  LowerReturnEmitInlineContext *activeInlineContext = nullptr;
  std::unordered_set<std::string> inlineStack;

  LowerReturnCallsEmitFileErrorWhyFn emitFileErrorWhy;
  LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;

  std::function<bool(const Expr &)> hasExplicitBindingTypeTransform;
  std::function<bool(const Expr &)> emitFloatLiteral;
  LowerReturnEmitCompareToZeroFn emitCompareToZero;
  ResolveDefinitionCallFn resolveDefinitionCall;
  ResolveResultExprInfoWithLocalsFn resolveResultExprInfo;
  LowerReturnEmitStringValueForCallFn emitStringValueForCall;

  LowerReturnEmitExprFn emitExpr;
  LowerReturnEmitStatementFn emitStatement;
  LowerReturnEmitInlineDefinitionCallFn emitInlineDefinitionCall;
  LowerReturnEmitAllocTempLocalFn allocTempLocal;
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
