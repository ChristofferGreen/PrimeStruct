#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IrLowererOnErrorHelpers.h"
#include "IrLowererLowerStatementsSourceMapStep.h"
#include "IrLowererStatementCallHelpers.h"

namespace primec::ir_lowerer {

struct LowerStatementsCallsStageInput {
  const Program *program = nullptr;
  const Definition *entryDef = nullptr;
  const SemanticProgram *semanticProgram = nullptr;
  const std::vector<std::string> *defaultEffects = nullptr;
  const std::vector<std::string> *entryDefaultEffects = nullptr;

  std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  const std::unordered_set<std::string> *structNames = nullptr;
  OnErrorByDefinition *onErrorByDef = nullptr;
  std::vector<std::string> *stringTable = nullptr;
  std::unordered_set<std::string> *loweredCallTargets = nullptr;
  std::unordered_map<std::string, std::vector<InstructionSourceRange>> *instructionSourceRangesByFunction =
      nullptr;
  std::optional<OnErrorHandler> *currentOnError = nullptr;
  std::optional<ResultReturnInfo> *currentReturnResult = nullptr;
  bool *sawReturn = nullptr;
  int32_t *nextLocal = nullptr;
  int32_t *onErrorTempCounter = nullptr;

  bool returnsVoid = false;
  bool entryHasResultInfo = false;
  const ResultReturnInfo *entryResultInfo = nullptr;

  IrFunction *function = nullptr;
  LocalMap *locals = nullptr;
  IrModule *outModule = nullptr;

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<bool(const Expr &, const LocalMap &)> emitExpr;
  std::function<bool(const Expr &, LocalMap &)> emitStatement;
  std::function<int32_t()> allocTempLocal;
  std::function<void(const std::string &, const Expr &, size_t, size_t)> appendInstructionSourceRange;

  std::function<void()> pushFileScope;
  std::function<void()> emitCurrentFileScopeCleanup;
  std::function<void()> popFileScope;

  std::function<std::string(const Expr &)> resolveExprPath;
  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  std::function<const Definition *(const Expr &)> resolveDefinitionCall;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;
  std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> emitInlineDefinitionCall;

  std::function<bool(const Expr &)> isTailCallCandidate;
  std::function<bool(const Definition &)> isStructDefinition;
  std::function<bool(const Expr &, const LocalMap &)> isArrayCountCall;
  std::function<bool(const Expr &, const LocalMap &)> isStringCountCall;
  std::function<bool(const Expr &, const LocalMap &)> isVectorCapacityCall;
  std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)>
      buildDefinitionCallContext;
  std::function<void()> resetDefinitionLoweringState;
};

bool runLowerStatementsCallsStage(const LowerStatementsCallsStageInput &input,
                                  std::string &errorOut);

} // namespace primec::ir_lowerer
