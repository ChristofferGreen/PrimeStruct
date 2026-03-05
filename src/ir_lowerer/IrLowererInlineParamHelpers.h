#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "IrLowererStructTypeHelpers.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

using InferInlineParameterLocalInfoFn = std::function<bool(const Expr &, LocalInfo &, std::string &)>;
using IsInlineParameterStringBindingFn = std::function<bool(const Expr &)>;
using EmitInlineParameterStringValueFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::StringSource &, int32_t &, bool &)>;
using InferInlineParameterStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using ResolveInlineParameterStructSlotLayoutFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using EmitInlineParameterExprFn = std::function<bool(const Expr &, const LocalMap &)>;
using EmitInlineParameterStructCopySlotsFn = std::function<bool(int32_t, int32_t, int32_t)>;
using AllocInlineParameterTempLocalFn = std::function<int32_t()>;
using EmitInlineParameterInstructionFn = std::function<void(IrOpcode, uint64_t)>;
using TrackInlineParameterFileHandleFn = std::function<void(int32_t)>;

bool emitInlineDefinitionCallParameters(
    const std::vector<Expr> &callParams,
    const std::vector<const Expr *> &orderedArgs,
    const LocalMap &callerLocals,
    int32_t &nextLocal,
    LocalMap &calleeLocals,
    const InferInlineParameterLocalInfoFn &inferCallParameterLocalInfo,
    const IsInlineParameterStringBindingFn &isStringBinding,
    const EmitInlineParameterStringValueFn &emitStringValueForCall,
    const InferInlineParameterStructExprPathFn &inferStructExprPath,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const EmitInlineParameterExprFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const AllocInlineParameterTempLocalFn &allocTempLocal,
    const EmitInlineParameterInstructionFn &emitInstruction,
    const TrackInlineParameterFileHandleFn &trackFileHandleLocal,
    std::string &error);

} // namespace primec::ir_lowerer
