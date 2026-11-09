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
enum class EntryCallableExecutionResult {
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
// Makes the entry function 0 of the module's function table.
void finalizeEntryFunctionTable(IrFunction &entryFunction,
                                std::vector<IrFunction> &outFunctions,
                                int32_t &entryIndex);

} // namespace primec::ir_lowerer
