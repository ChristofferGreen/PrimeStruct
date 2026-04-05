



struct Definition;
struct IrFunction;
struct OnErrorHandler;
struct Program;
struct ResultReturnInfo;
struct ReturnInfo;
struct SemanticProgram;
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
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);
DispatchStatementEmitResult tryEmitDispatchStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<std::string(const Expr &)> &resolveExprPath,
    const std::function<const Definition *(const std::string &)> &findDefinitionByPath,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error);
DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error);
inline DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  return tryEmitDirectCallStatement(
      stmt,
      localsIn,
      isArrayCountCall,
      isStringCountCall,
      isVectorCapacityCall,
      [](const Expr &, const LocalMap &) { return true; },
      resolveMethodCallDefinition,
      resolveDefinitionCall,
      getReturnInfo,
      emitInlineDefinitionCall,
      instructions,
      error);
}
AssignOrExprStatementEmitResult emitAssignOrExprStatementWithPop(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
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
    const std::function<bool(const Expr &)> &isTailCallCandidate,
    const std::function<void()> &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const std::function<bool(const std::string &, const ReturnInfo &)> &appendReturnForDefinition,
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
    const std::function<bool(const Expr &)> &isTailCallCandidate,
    const std::function<void()> &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const std::function<bool(const std::string &, const ReturnInfo &)> &appendReturnForDefinition,
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
    const std::function<bool(const Expr &)> &emitStatement,
    const std::function<void()> &pushFileScope,
    const std::function<void()> &emitCurrentFileScopeCleanup,
    const std::function<void()> &popFileScope,
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
    const std::function<bool(const Expr &)> &isTailCallCandidate,
    const std::function<void()> &resetDefinitionLoweringState,
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
    const std::function<bool(const Expr &)> &isTailCallCandidate,
    const std::function<void()> &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    int32_t &entryIndex,
    std::string &error);
