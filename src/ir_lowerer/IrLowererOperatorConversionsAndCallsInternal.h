#pragma once

#include "IrLowererOperatorConversionsAndCallsHelpers.h"

namespace primec::ir_lowerer {

struct ConversionsAndCallsOperatorContext {
  const LocalMap &localsIn;
  int32_t &nextLocal;
  const EmitConversionsAndCallsExprWithLocalsFn &emitExpr;
  const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind;
  const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero;
  const AllocConversionsAndCallsTempLocalFn &allocTempLocal;
  const EmitConversionsAndCallsFloatToIntNonFiniteFn &emitFloatToIntNonFinite;
  const EmitConversionsAndCallsPointerIndexOutOfBoundsFn &emitPointerIndexOutOfBounds;
  const EmitConversionsAndCallsArrayIndexOutOfBoundsFn &emitArrayIndexOutOfBounds;
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
};

bool emitConversionsAndCallsMemoryAndPointerExpr(
    const Expr &expr,
    ConversionsAndCallsOperatorContext &context,
    bool &handled);

bool emitConversionsAndCallsCollectionAndMutationExpr(
    const Expr &expr,
    ConversionsAndCallsOperatorContext &context,
    bool &handled);

} // namespace primec::ir_lowerer
