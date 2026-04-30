#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "IrLowererRuntimeErrorHelpers.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

struct LayoutFieldBinding;
struct SemanticProductTargetAdapter;

using EmitConversionsAndCallsExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using EmitConversionsAndCallsStatementWithLocalsFn = std::function<bool(const Expr &, LocalMap &)>;
using InferConversionsAndCallsExprKindWithLocalsFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using CombineConversionsAndCallsNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using EmitConversionsAndCallsCompareToZeroFn = std::function<bool(LocalInfo::ValueKind, bool)>;
using AllocConversionsAndCallsTempLocalFn = std::function<int32_t()>;
using EmitConversionsAndCallsFloatToIntNonFiniteFn = std::function<void()>;
using EmitConversionsAndCallsPointerIndexOutOfBoundsFn = std::function<void()>;
using EmitConversionsAndCallsArrayIndexOutOfBoundsFn = std::function<void()>;
using ResolveConversionsAndCallsStringTableTargetFn =
    std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)>;
using ConversionsAndCallsValueKindFromTypeNameFn = std::function<LocalInfo::ValueKind(const std::string &)>;
using ConversionsAndCallsGetMathConstantNameFn = std::function<bool(const std::string &, std::string &)>;
using InferConversionsAndCallsStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using ResolveConversionsAndCallsStructTypeNameFn =
    std::function<bool(const std::string &, const std::string &, std::string &)>;
using ResolveConversionsAndCallsStructSlotCountFn = std::function<bool(const std::string &, int32_t &)>;
using ResolveConversionsAndCallsStructFieldInfoFn =
    std::function<bool(const std::string &, const std::string &, int32_t &, int32_t &, std::string &)>;
using ResolveConversionsAndCallsStructFieldBindingFn =
    std::function<bool(const std::string &, const std::string &, LayoutFieldBinding &)>;
using EmitConversionsAndCallsStructCopyFromPtrsFn = std::function<bool(int32_t, int32_t, int32_t)>;
using HasConversionsAndCallsNamedArgumentsFn =
    std::function<bool(const std::vector<std::optional<std::string>> &)>;
using ResolveConversionsAndCallsDefinitionCallFn = std::function<const Definition *(const Expr &)>;
using ResolveConversionsAndCallsExprPathFn = std::function<std::string(const Expr &)>;
using LowerConversionsAndCallsMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;
using IsConversionsAndCallsBindingMutableFn = std::function<bool(const Expr &)>;
using ConversionsAndCallsBindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using HasConversionsAndCallsExplicitBindingTypeTransformFn = std::function<bool(const Expr &)>;
using ConversionsAndCallsBindingValueKindFn = std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using ApplyConversionsAndCallsStructArrayInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using ApplyConversionsAndCallsStructValueInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using EnterConversionsAndCallsScopedBlockFn = std::function<void()>;
using ExitConversionsAndCallsScopedBlockFn = std::function<void()>;
using IsConversionsAndCallsReturnCallFn = std::function<bool(const Expr &)>;
using IsConversionsAndCallsBlockCallFn = std::function<bool(const Expr &)>;
using IsConversionsAndCallsMatchCallFn = std::function<bool(const Expr &)>;
using IsConversionsAndCallsIfCallFn = std::function<bool(const Expr &)>;

bool emitConversionsAndCallsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    int32_t &nextLocal,
    const EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero,
    const AllocConversionsAndCallsTempLocalFn &allocTempLocal,
    const EmitConversionsAndCallsFloatToIntNonFiniteFn &emitFloatToIntNonFinite,
    const EmitConversionsAndCallsPointerIndexOutOfBoundsFn &emitPointerIndexOutOfBounds,
    const EmitConversionsAndCallsArrayIndexOutOfBoundsFn &emitArrayIndexOutOfBounds,
    const ResolveConversionsAndCallsStringTableTargetFn &resolveStringTableTarget,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName,
    const ConversionsAndCallsGetMathConstantNameFn &getMathConstantName,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ResolveConversionsAndCallsStructTypeNameFn &resolveStructTypeName,
    const ResolveConversionsAndCallsStructSlotCountFn &resolveStructSlotCount,
    const ResolveConversionsAndCallsStructFieldInfoFn &resolveStructFieldInfo,
    const ResolveConversionsAndCallsStructFieldBindingFn &resolveStructFieldBinding,
    const EmitConversionsAndCallsStructCopyFromPtrsFn &emitStructCopyFromPtrs,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error,
    const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall = {},
    const SemanticProductTargetAdapter *semanticProductTargets = nullptr);

bool emitConversionsAndCallsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    int32_t &nextLocal,
    const EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero,
    const AllocConversionsAndCallsTempLocalFn &allocTempLocal,
    const EmitConversionsAndCallsFloatToIntNonFiniteFn &emitFloatToIntNonFinite,
    const EmitConversionsAndCallsPointerIndexOutOfBoundsFn &emitPointerIndexOutOfBounds,
    const EmitConversionsAndCallsArrayIndexOutOfBoundsFn &emitArrayIndexOutOfBounds,
    const ResolveConversionsAndCallsStringTableTargetFn &resolveStringTableTarget,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName,
    const ConversionsAndCallsGetMathConstantNameFn &getMathConstantName,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ResolveConversionsAndCallsStructTypeNameFn &resolveStructTypeName,
    const ResolveConversionsAndCallsStructSlotCountFn &resolveStructSlotCount,
    const ResolveConversionsAndCallsStructFieldInfoFn &resolveStructFieldInfo,
    const EmitConversionsAndCallsStructCopyFromPtrsFn &emitStructCopyFromPtrs,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error);

bool emitConversionsAndCallsControlExprTail(
    const Expr &expr,
    const LocalMap &localsIn,
    const EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const EmitConversionsAndCallsStatementWithLocalsFn &emitStatement,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const CombineConversionsAndCallsNumericKindsFn &combineNumericKinds,
    const HasConversionsAndCallsNamedArgumentsFn &hasNamedArguments,
    const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall,
    const ResolveConversionsAndCallsExprPathFn &resolveExprPath,
    const LowerConversionsAndCallsMatchToIfFn &lowerMatchToIf,
    const IsConversionsAndCallsBindingMutableFn &isBindingMutable,
    const ConversionsAndCallsBindingKindFn &bindingKind,
    const HasConversionsAndCallsExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
    const ConversionsAndCallsBindingValueKindFn &bindingValueKind,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ApplyConversionsAndCallsStructArrayInfoFn &applyStructArrayInfo,
    const ApplyConversionsAndCallsStructValueInfoFn &applyStructValueInfo,
    const EnterConversionsAndCallsScopedBlockFn &enterScopedBlock,
    const ExitConversionsAndCallsScopedBlockFn &exitScopedBlock,
    const IsConversionsAndCallsReturnCallFn &isReturnCall,
    const IsConversionsAndCallsBlockCallFn &isBlockCall,
    const IsConversionsAndCallsMatchCallFn &isMatchCall,
    const IsConversionsAndCallsIfCallFn &isIfCall,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error);

} // namespace primec::ir_lowerer
