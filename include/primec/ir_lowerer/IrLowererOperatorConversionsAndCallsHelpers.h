#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ir_lowerer/IrLowererRuntimeErrorHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

struct LayoutFieldBinding;
struct SemanticProductTargetAdapter;

using EmitConversionsAndCallsStatementWithLocalsFn = std::function<bool(const Expr &, LocalMap &)>;
using InferConversionsAndCallsExprKindWithLocalsFn =
    ExprLocalsValueKindFn;
using CombineConversionsAndCallsNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using EmitConversionsAndCallsCompareToZeroFn = std::function<bool(LocalInfo::ValueKind, bool)>;
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
// Assigns the struct at `srcPtrLocal` to the one at `destPtrLocal` with ownership: the old value
// is destroyed and the new one copied (or moved from a temporary). `destDropFlagLocal` is the
// destination local's drop flag, or -1 when the destination always owns its value.
using EmitConversionsAndCallsOwnedStructAssignFn = std::function<bool(int32_t destPtrLocal,
                                                                      int32_t srcPtrLocal,
                                                                      int32_t slotCount,
                                                                      const std::string &structPath,
                                                                      const Expr &rhsExpr,
                                                                      int32_t destDropFlagLocal)>;
using HasConversionsAndCallsNamedArgumentsFn =
    std::function<bool(const std::vector<std::optional<std::string>> &)>;
using ResolveConversionsAndCallsDefinitionCallFn = std::function<const Definition *(const Expr &)>;
using LowerConversionsAndCallsMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;
using ConversionsAndCallsBindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using ConversionsAndCallsBindingValueKindFn = std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;

bool emitConversionsAndCallsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    int32_t &nextLocal,
    const ExprLocalsPredicateFn &emitExpr,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero,
    const Int32ProviderFn &allocTempLocal,
    const ActionFn &emitFloatToIntNonFinite,
    const ActionFn &emitPointerIndexOutOfBounds,
    const ActionFn &emitArrayIndexOutOfBounds,
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
    const SemanticProductTargetAdapter *semanticProductTargets = nullptr,
    std::string currentScopePath = {},
    const EmitConversionsAndCallsOwnedStructAssignFn &emitOwnedStructAssign = {});

bool emitConversionsAndCallsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    int32_t &nextLocal,
    const ExprLocalsPredicateFn &emitExpr,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero,
    const Int32ProviderFn &allocTempLocal,
    const ActionFn &emitFloatToIntNonFinite,
    const ActionFn &emitPointerIndexOutOfBounds,
    const ActionFn &emitArrayIndexOutOfBounds,
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
    const ExprLocalsPredicateFn &emitExpr,
    const EmitConversionsAndCallsStatementWithLocalsFn &emitStatement,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const CombineConversionsAndCallsNumericKindsFn &combineNumericKinds,
    const HasConversionsAndCallsNamedArgumentsFn &hasNamedArguments,
    const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall,
    const ExprStringFn &resolveExprPath,
    const LowerConversionsAndCallsMatchToIfFn &lowerMatchToIf,
    const ExprPredicateFn &isBindingMutable,
    const ConversionsAndCallsBindingKindFn &bindingKind,
    const ExprPredicateFn &hasExplicitBindingTypeTransform,
    const ConversionsAndCallsBindingValueKindFn &bindingValueKind,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ExprLocalInfoVisitorFn &applyStructArrayInfo,
    const ExprLocalInfoVisitorFn &applyStructValueInfo,
    const ActionFn &enterScopedBlock,
    const ActionFn &exitScopedBlock,
    const ExprPredicateFn &isReturnCall,
    const ExprPredicateFn &isBlockCall,
    const ExprPredicateFn &isMatchCall,
    const ExprPredicateFn &isIfCall,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error);

} // namespace primec::ir_lowerer
