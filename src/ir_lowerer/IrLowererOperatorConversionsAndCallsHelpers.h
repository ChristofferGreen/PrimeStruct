#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

using EmitConversionsAndCallsExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferConversionsAndCallsExprKindWithLocalsFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using EmitConversionsAndCallsCompareToZeroFn = std::function<bool(LocalInfo::ValueKind, bool)>;
using AllocConversionsAndCallsTempLocalFn = std::function<int32_t()>;
using EmitConversionsAndCallsFloatToIntNonFiniteFn = std::function<void()>;
using ResolveConversionsAndCallsStringTableTargetFn =
    std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)>;
using ConversionsAndCallsValueKindFromTypeNameFn = std::function<LocalInfo::ValueKind(const std::string &)>;
using ConversionsAndCallsGetMathConstantNameFn = std::function<bool(const std::string &, std::string &)>;
using InferConversionsAndCallsStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using ResolveConversionsAndCallsStructSlotCountFn = std::function<bool(const std::string &, int32_t &)>;
using EmitConversionsAndCallsStructCopyFromPtrsFn = std::function<bool(int32_t, int32_t, int32_t)>;

bool emitConversionsAndCallsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    int32_t &nextLocal,
    const EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero,
    const AllocConversionsAndCallsTempLocalFn &allocTempLocal,
    const EmitConversionsAndCallsFloatToIntNonFiniteFn &emitFloatToIntNonFinite,
    const ResolveConversionsAndCallsStringTableTargetFn &resolveStringTableTarget,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName,
    const ConversionsAndCallsGetMathConstantNameFn &getMathConstantName,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ResolveConversionsAndCallsStructSlotCountFn &resolveStructSlotCount,
    const EmitConversionsAndCallsStructCopyFromPtrsFn &emitStructCopyFromPtrs,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error);

} // namespace primec::ir_lowerer
