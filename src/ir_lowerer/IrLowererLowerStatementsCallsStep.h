#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "IrLowererStatementCallHelpers.h"

namespace primec::ir_lowerer {

struct SemanticProductIndex;

struct LowerStatementsCallsStepInput {
  const SemanticProgram *semanticProgram = nullptr;
  const SemanticProductIndex *semanticIndex = nullptr;

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<bool(const Expr &, const LocalMap &)> emitExpr;
  std::function<int32_t()> allocTempLocal;

  std::function<std::string(const Expr &)> resolveExprPath;
  std::function<const Definition *(const std::string &)> findDefinitionByPath;

  std::function<bool(const Expr &, const LocalMap &)> isArrayCountCall;
  std::function<bool(const Expr &, const LocalMap &)> isStringCountCall;
  std::function<bool(const Expr &, const LocalMap &)> isVectorCapacityCall;
  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  std::function<const Definition *(const Expr &)> resolveDefinitionCall;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;
  std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> emitInlineDefinitionCall;
  std::function<void()> emitArrayIndexOutOfBounds;

  std::vector<IrInstruction> *instructions = nullptr;
};

bool runLowerStatementsCallsStep(const LowerStatementsCallsStepInput &input,
                                 const Expr &stmt,
                                 const LocalMap &localsIn,
                                 std::string &errorOut);

} // namespace primec::ir_lowerer
