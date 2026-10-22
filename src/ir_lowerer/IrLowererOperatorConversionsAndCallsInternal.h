#pragma once

#include "primec/ir_lowerer/IrLowererOperatorConversionsAndCallsHelpers.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

struct ConversionsAndCallsOperatorContext {
  const LocalMap &localsIn;
  int32_t &nextLocal;
  const ExprLocalsPredicateFn &emitExpr;
  const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind;
  const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero;
  const Int32ProviderFn &allocTempLocal;
  const ActionFn &emitFloatToIntNonFinite;
  const ActionFn &emitPointerIndexOutOfBounds;
  const ActionFn &emitArrayIndexOutOfBounds;
  const ResolveConversionsAndCallsStringTableTargetFn &resolveStringTableTarget;
  const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName;
  const ConversionsAndCallsGetMathConstantNameFn &getMathConstantName;
  const InferConversionsAndCallsStructExprPathFn &inferStructExprPath;
  const ResolveConversionsAndCallsStructTypeNameFn &resolveStructTypeName;
  const ResolveConversionsAndCallsStructSlotCountFn &resolveStructSlotCount;
  const ResolveConversionsAndCallsStructFieldInfoFn &resolveStructFieldInfo;
  const ResolveConversionsAndCallsStructFieldBindingFn &resolveStructFieldBinding;
  const EmitConversionsAndCallsStructCopyFromPtrsFn &emitStructCopyFromPtrs;
  std::vector<IrInstruction> &instructions;
  std::string &error;
  const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall;
  const SemanticProductTargetAdapter *semanticProductTargets = nullptr;
  std::string currentScopePath = {};
  EmitConversionsAndCallsOwnedStructAssignFn emitOwnedStructAssign = {};
};

// True when `expr` is a pointer (or reference) to u8 element storage: such pointers address single bytes,
// so their arithmetic steps one byte and their loads and stores are byte accesses.
bool pointerTargetsBytes(const Expr &expr, ConversionsAndCallsOperatorContext &context);

bool emitConversionsAndCallsMemoryAndPointerExpr(
    const Expr &expr,
    ConversionsAndCallsOperatorContext &context,
    bool &handled);

bool emitConversionsAndCallsCollectionAndMutationExpr(
    const Expr &expr,
    ConversionsAndCallsOperatorContext &context,
    bool &handled);

} // namespace primec::ir_lowerer
