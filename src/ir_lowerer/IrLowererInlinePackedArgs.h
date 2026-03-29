#pragma once

#include "IrLowererInlineParamHelpers.h"

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
    const EmitInlineParameterExprFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const AllocInlineParameterTempLocalFn &allocTempLocal,
    const EmitInlineParameterInstructionFn &emitInstruction,
    std::string &error,
    const InferInlineParameterExprLocalInfoFn &inferExprLocalInfo = {});

} // namespace primec::ir_lowerer
