#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/frontend/SemanticProduct.h"

namespace primec::ir_lowerer {

struct SemanticProductIndex;
struct StructSlotLayoutInfo;

enum class BufferStoreStatementEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class DispatchStatementEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class DirectCallStatementEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class AssignOrExprStatementEmitResult {
  Emitted,
  Error,
};
enum class CallableDefinitionOrchestrationResult {
  Emitted,
  Error,
};
enum class EntryCallableExecutionResult {
  Emitted,
  Error,
};
enum class FunctionTableFinalizationResult {
  Emitted,
  Error,
};

BufferStoreStatementEmitResult tryEmitBufferStoreStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const ExprLocalsValueKindFn &inferExprKind,
    const ExprLocalsPredicateFn &emitExpr,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
DispatchStatementEmitResult tryEmitDispatchStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const ExprStringFn &resolveExprPath,
    const std::function<const Definition *(const std::string &)> &findDefinitionByPath,
    const ExprLocalsValueKindFn &inferExprKind,
    const ExprLocalsPredicateFn &emitExpr,
    const Int32ProviderFn &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &isArrayCountCall,
    const ExprLocalsPredicateFn &isStringCountCall,
    const ExprLocalsPredicateFn &isVectorCapacityCall,
    const ExprLocalsPredicateFn &emitExpr,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const ActionFn &emitArrayIndexOutOfBounds,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &isArrayCountCall,
    const ExprLocalsPredicateFn &isStringCountCall,
    const ExprLocalsPredicateFn &isVectorCapacityCall,
    const ExprLocalsPredicateFn &emitExpr,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
AssignOrExprStatementEmitResult emitAssignOrExprStatementWithPop(
    const Expr &stmt,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &emitExpr,
    std::vector<IrInstruction> &instructions);
bool buildCallableDefinitionCallContext(
    const Definition &def,
    const std::unordered_set<std::string> &structNames,
    int32_t &nextLocal,
    LocalMap &definitionLocals,
    Expr &callExpr,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<bool(const Expr &, const LocalMap &, LocalInfo &, std::string &)> &inferParameterLocalInfo,
    std::string &error);
CallableDefinitionOrchestrationResult lowerCallableDefinitionOrchestration(
    const Program &program,
    const Definition &entryDef,
    const std::unordered_set<std::string> &loweredCallTargets,
    const std::function<bool(const Definition &)> &isStructDefinition,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const ExprPredicateFn &isTailCallCandidate,
    const ActionFn &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    IrFunction &function,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    std::string &error);
CallableDefinitionOrchestrationResult lowerCallableDefinitionOrchestration(
    const Program &program,
    const Definition &entryDef,
    const SemanticProgram *semanticProgram,
    const std::unordered_set<std::string> &loweredCallTargets,
    const std::function<bool(const Definition &)> &isStructDefinition,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const ExprPredicateFn &isTailCallCandidate,
    const ActionFn &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    IrFunction &function,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    std::string &error);
EntryCallableExecutionResult emitEntryCallableExecutionWithCleanup(
    const Definition &entryDef,
    bool definitionReturnsVoid,
    bool &sawReturn,
    std::optional<OnErrorHandler> &currentOnError,
    const std::optional<OnErrorHandler> &entryOnError,
    std::optional<ResultReturnInfo> &currentReturnResult,
    const std::optional<ResultReturnInfo> &entryResult,
    const ExprPredicateFn &emitStatement,
    const ActionFn &pushFileScope,
    const ActionFn &emitCurrentFileScopeCleanup,
    const ActionFn &popFileScope,
    std::vector<IrInstruction> &instructions,
    std::string &error);
FunctionTableFinalizationResult finalizeEntryFunctionTableAndLowerCallables(
    const Program &program,
    const Definition &entryDef,
    IrFunction &entryFunction,
    const std::unordered_set<std::string> &loweredCallTargets,
    const std::function<bool(const Definition &)> &isStructDefinition,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const ExprPredicateFn &isTailCallCandidate,
    const ActionFn &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    int32_t &entryIndex,
    std::string &error);
FunctionTableFinalizationResult finalizeEntryFunctionTableAndLowerCallables(
    const Program &program,
    const Definition &entryDef,
    const SemanticProgram *semanticProgram,
    IrFunction &entryFunction,
    const std::unordered_set<std::string> &loweredCallTargets,
    const std::function<bool(const Definition &)> &isStructDefinition,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const ExprPredicateFn &isTailCallCandidate,
    const ActionFn &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    int32_t &entryIndex,
    std::string &error);

} // namespace primec::ir_lowerer
