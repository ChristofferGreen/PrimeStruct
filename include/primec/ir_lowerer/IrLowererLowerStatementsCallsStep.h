#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "primec/ir_lowerer/IrLowererStatementCallHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

struct SemanticProductIndex;

struct LowerStatementsCallsStepInput {
  const SemanticProgram *semanticProgram = nullptr;
  const SemanticProductIndex *semanticIndex = nullptr;

  ExprLocalsValueKindFn inferExprKind;
  std::function<std::string(const Expr &, const LocalMap &)> inferStructExprPath;
  ExprLocalsPredicateFn emitExpr;
  Int32ProviderFn allocTempLocal;

  ExprStringFn resolveExprPath;
  std::function<const Definition *(const std::string &)> findDefinitionByPath;
  std::function<const Definition *(const std::string &)> resolveDestroyHelperForStruct;
  std::function<const Definition *(const std::string &)> resolveMoveHelperForStruct;

  ExprLocalsPredicateFn isArrayCountCall;
  ExprLocalsPredicateFn isStringCountCall;
  ExprLocalsPredicateFn isVectorCapacityCall;
  ResolveStructSlotLayoutFn resolveStructSlotLayout;
  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  std::function<const Definition *(const Expr &)> resolveDefinitionCall;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;
  std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> emitInlineDefinitionCall;
  ActionFn emitArrayIndexOutOfBounds;
  ActionFn emitVectorCapacityExceeded;
  ActionFn emitVectorPopOnEmpty;
  ActionFn emitVectorIndexOutOfBounds;
  ActionFn emitVectorReserveNegative;
  ActionFn emitVectorReserveExceeded;

  std::vector<IrInstruction> *instructions = nullptr;
};

bool runLowerStatementsCallsStep(const LowerStatementsCallsStepInput &input,
                                 const Expr &stmt,
                                 const LocalMap &localsIn,
                                 std::string &errorOut);

} // namespace primec::ir_lowerer
