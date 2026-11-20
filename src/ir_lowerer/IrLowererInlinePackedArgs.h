#pragma once

#include "primec/ir_lowerer/IrLowererInlineParamHelpers.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

bool emitInlinePackedCallParameter(
    const Expr &param,
    LocalInfo &paramInfo,
    const std::vector<const Expr *> &packedArgs,
    const LocalMap &callerLocals,
    int32_t &nextLocal,
    LocalMap &calleeLocals,
    const EmitInlineParameterStringValueFn &emitStringValueForCall,
    const InferInlineParameterStructExprPathFn &inferStructExprPath,
    const InferInlineParameterExprKindFn &inferExprKind,
    const ResolveInlineParameterDefinitionCallFn &resolveDefinitionCall,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const ExprLocalsPredicateFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const Int32ProviderFn &allocTempLocal,
    const EmitInstructionFn &emitInstruction,
    std::string &error,
    const InferInlineParameterExprLocalInfoFn &inferExprLocalInfo = {},
    // Whether an argument is a temporary the pack takes over; a call-shaped argument is one when unset.
    const ExprPredicateFn &isTemporaryCopySource = {},
    // Runs the `Copy` helper of a struct (see EmitInlineParameterStructCopyHelperFn) and tells whether a struct
    // owns something to destroy; a pack of an owning struct owns its elements and does not alias a caller's pack.
    const EmitInlineParameterStructCopyHelperFn &emitStructCopyHelper = {},
    const std::function<bool(const std::string &)> &structOwnsResources = {});

} // namespace primec::ir_lowerer
