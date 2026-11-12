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
  // Gives the builtin vector record pushed at `destPtrLocal` (a slot copy of the one at
  // `srcPtrLocal`) a buffer of its own; `elementTypeText` is the pushed vector's type.
  std::function<bool(int32_t, int32_t, const std::string &, const std::string &, const LocalMap &)>
      emitPushedVectorCopy;
  // Finishes copying a struct pushed from a place (its slots are already at `destPtrLocal`): runs
  // its `Copy` helper or its fields' copies, so the element owns what it holds.
  std::function<bool(int32_t, int32_t, const std::string &, const LocalMap &)> emitPushedStructCopy;
  // Whether a struct-valued payload is a temporary the vector takes over (a constructor or a call
  // result) rather than a place that keeps owning it.
  std::function<bool(const Expr &, const LocalMap &)> isTemporaryStructSource;
  // The struct path (or nested builtin vector path) of a vector element type that needs
  // destroying when `pop` or `clear` drops it; empty when the element owns nothing. Returns false
  // when the element type cannot be resolved.
  std::function<bool(const std::string &, const std::string &, std::string &)>
      resolveDroppedVectorElementStructPath;
  // Destroys the value of `structPath` at the address in the given local.
  std::function<bool(int32_t, const std::string &, const LocalMap &)> emitStructDestroyFromPtr;
  // The element type text a specialization of the stdlib vector struct was made for (`String`), spelled as at its
  // first use; empty when `structPath` is no such specialization.
  std::function<std::string(const std::string &)> resolveVectorRecordElementTypeText;

  std::vector<IrInstruction> *instructions = nullptr;
};

bool runLowerStatementsCallsStep(const LowerStatementsCallsStepInput &input,
                                 const Expr &stmt,
                                 const LocalMap &localsIn,
                                 std::string &errorOut);

} // namespace primec::ir_lowerer
