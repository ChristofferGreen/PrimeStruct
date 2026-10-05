#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

using InferInlineParameterLocalInfoFn = std::function<bool(const Expr &, LocalInfo &, std::string &)>;
using EmitInlineParameterStringValueFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::StringSource &, int32_t &, bool &)>;
using InferInlineParameterStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using InferInlineParameterExprKindFn =
    ExprLocalsValueKindFn;
using InferInlineParameterExprLocalInfoFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo &, std::string &)>;
using ResolveInlineParameterDefinitionCallFn = std::function<const Definition *(const Expr &)>;
using ResolveInlineParameterStructSlotLayoutFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using EmitInlineParameterStructCopySlotsFn = std::function<bool(int32_t, int32_t, int32_t)>;
using TrackInlineParameterFileHandleFn = std::function<void(int32_t)>;
// Runs the `Copy` lifecycle helper of `structPath` (when it has one) to fill the copy at
// `destPtrLocal` from the value at `srcPtrLocal`; sets `ranHelper` to whether there was one.
using EmitInlineParameterStructCopyHelperFn = std::function<bool(
    int32_t destPtrLocal, int32_t srcPtrLocal, const std::string &structPath, bool &ranHelper)>;

bool emitInlineDefinitionCallParameters(
    const std::vector<Expr> &callParams,
    const std::vector<const Expr *> &orderedArgs,
    const std::vector<const Expr *> &packedArgs,
    size_t packedParamIndex,
    const LocalMap &callerLocals,
    int32_t &nextLocal,
    LocalMap &calleeLocals,
    const InferInlineParameterLocalInfoFn &inferCallParameterLocalInfo,
    const ExprPredicateFn &isStringBinding,
    const EmitInlineParameterStringValueFn &emitStringValueForCall,
    const InferInlineParameterStructExprPathFn &inferStructExprPath,
    const InferInlineParameterExprKindFn &inferExprKind,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const ExprLocalsPredicateFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const Int32ProviderFn &allocTempLocal,
    const EmitInstructionFn &emitInstruction,
    const TrackInlineParameterFileHandleFn &trackFileHandleLocal,
    std::string &error,
    const InferInlineParameterExprLocalInfoFn &inferExprLocalInfo = {});

bool emitInlineDefinitionCallParameters(
    const std::vector<Expr> &callParams,
    const std::vector<const Expr *> &orderedArgs,
    const std::vector<const Expr *> &packedArgs,
    size_t packedParamIndex,
    const std::string &calleePath,
    const LocalMap &callerLocals,
    int32_t &nextLocal,
    LocalMap &calleeLocals,
    const InferInlineParameterLocalInfoFn &inferCallParameterLocalInfo,
    const ExprPredicateFn &isStringBinding,
    const EmitInlineParameterStringValueFn &emitStringValueForCall,
    const InferInlineParameterStructExprPathFn &inferStructExprPath,
    const InferInlineParameterExprKindFn &inferExprKind,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const ExprLocalsPredicateFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const Int32ProviderFn &allocTempLocal,
    const EmitInstructionFn &emitInstruction,
    const TrackInlineParameterFileHandleFn &trackFileHandleLocal,
    std::string &error,
    const InferInlineParameterExprLocalInfoFn &inferExprLocalInfo);

bool emitInlineDefinitionCallParameters(
    const std::vector<Expr> &callParams,
    const std::vector<const Expr *> &orderedArgs,
    const std::vector<const Expr *> &packedArgs,
    size_t packedParamIndex,
    const LocalMap &callerLocals,
    int32_t &nextLocal,
    LocalMap &calleeLocals,
    const InferInlineParameterLocalInfoFn &inferCallParameterLocalInfo,
    const ExprPredicateFn &isStringBinding,
    const EmitInlineParameterStringValueFn &emitStringValueForCall,
    const InferInlineParameterStructExprPathFn &inferStructExprPath,
    const InferInlineParameterExprKindFn &inferExprKind,
    const ResolveInlineParameterDefinitionCallFn &resolveDefinitionCall,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const ExprLocalsPredicateFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const Int32ProviderFn &allocTempLocal,
    const EmitInstructionFn &emitInstruction,
    const TrackInlineParameterFileHandleFn &trackFileHandleLocal,
    std::string &error,
    const InferInlineParameterExprLocalInfoFn &inferExprLocalInfo);

bool emitInlineDefinitionCallParameters(
    const std::vector<Expr> &callParams,
    const std::vector<const Expr *> &orderedArgs,
    const std::vector<const Expr *> &packedArgs,
    size_t packedParamIndex,
    const std::string &calleePath,
    const LocalMap &callerLocals,
    int32_t &nextLocal,
    LocalMap &calleeLocals,
    const InferInlineParameterLocalInfoFn &inferCallParameterLocalInfo,
    const ExprPredicateFn &isStringBinding,
    const EmitInlineParameterStringValueFn &emitStringValueForCall,
    const InferInlineParameterStructExprPathFn &inferStructExprPath,
    const InferInlineParameterExprKindFn &inferExprKind,
    const ResolveInlineParameterDefinitionCallFn &resolveDefinitionCall,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const ExprLocalsPredicateFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const Int32ProviderFn &allocTempLocal,
    const EmitInstructionFn &emitInstruction,
    const TrackInlineParameterFileHandleFn &trackFileHandleLocal,
    std::string &error,
    const InferInlineParameterExprLocalInfoFn &inferExprLocalInfo = {},
    // TODO-5300 round 6: a struct args-pack element (e.g. args<Entry<K,V>>)
    // passed directly as a struct-typed call argument re-emits its bounds
    // check via emitArrayVectorIndexedAccess, which needs a real
    // instructionCount/patchInstructionImm pair to backpatch its
    // JumpIfZero placeholders to the correct forward target. Callers that
    // have direct access to the target IrFunction's instructions (the
    // production ir_lowerer pipeline) must bind these to it; a caller that
    // leaves them unbound keeps the previous (pre-fix) no-op behavior.
    const SizeProviderFn &instructionCount = {},
    const PatchInstructionImmFn &patchInstructionImm = {},
    const ActionFn &emitArrayIndexOutOfBounds = {},
    // `[T copy]` parameters of a type with a `Copy` helper are filled through it.
    const EmitInlineParameterStructCopyHelperFn &emitStructCopyHelper = {});

} // namespace primec::ir_lowerer
