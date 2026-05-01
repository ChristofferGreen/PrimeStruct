



struct ReturnInfo;

struct OnErrorHandler {
  std::string errorType;
  std::string handlerPath;
  std::vector<Expr> boundArgs = {};
};

struct ResultReturnInfo {
  bool isResult = false;
  bool hasValue = false;
};

class OnErrorScope {
 public:
  OnErrorScope(std::optional<OnErrorHandler> &targetIn, std::optional<OnErrorHandler> next);
  ~OnErrorScope();

 private:
  std::optional<OnErrorHandler> &target;
  std::optional<OnErrorHandler> previous;
};

class ResultReturnScope {
 public:
  ResultReturnScope(std::optional<ResultReturnInfo> &targetIn, std::optional<ResultReturnInfo> next);
  ~ResultReturnScope();

 private:
  std::optional<ResultReturnInfo> &target;
  std::optional<ResultReturnInfo> previous;
};

void emitFileCloseIfValid(std::vector<IrInstruction> &instructions, int32_t localIndex);
void emitFileScopeCleanup(std::vector<IrInstruction> &instructions, const std::vector<int32_t> &scope);
void emitAllFileScopeCleanup(std::vector<IrInstruction> &instructions,
                             const std::vector<std::vector<int32_t>> &fileScopeStack);
bool emitDestroyHelperFromPtr(
    int32_t valuePtrLocal,
    const std::string &structPath,
    const Definition *destroyHelper,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error);
bool emitMoveHelperFromPtrs(
    int32_t destPtrLocal,
    int32_t srcPtrLocal,
    const std::string &structPath,
    const Definition *moveHelper,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error);
bool emitStructCopyFromPtrs(std::vector<IrInstruction> &instructions,
                            int32_t destPtrLocal,
                            int32_t srcPtrLocal,
                            int32_t slotCount);
bool emitStructCopySlots(std::vector<IrInstruction> &instructions,
                         int32_t destBaseLocal,
                         int32_t srcPtrLocal,
                         int32_t slotCount,
                         const std::function<int32_t()> &allocTempLocal);
bool emitVectorDestroySlot(
    std::vector<IrInstruction> &instructions,
    int32_t dataPtrLocal,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    const std::string &structPath,
    const Definition *destroyHelper,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error);
bool emitVectorMoveSlot(
    std::vector<IrInstruction> &instructions,
    int32_t dataPtrLocal,
    int32_t destIndexLocal,
    int32_t srcIndexLocal,
    LocalInfo::ValueKind indexKind,
    const std::string &structPath,
    const Definition *moveHelper,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error);
void emitDisarmTemporaryStructAfterCopy(const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
                                        int32_t srcPtrLocal,
                                        const std::string &structPath);
bool shouldDisarmStructCopySourceExpr(const Expr &expr);
bool emitCompareToZero(std::vector<IrInstruction> &instructions,
                       LocalInfo::ValueKind kind,
                       bool equals,
                       std::string &error);
bool emitFloatLiteral(std::vector<IrInstruction> &instructions, const Expr &expr, std::string &error);
bool emitReturnForDefinition(std::vector<IrInstruction> &instructions,
                             const std::string &defPath,
                             const ReturnInfo &returnInfo,
                             std::string &error);
const char *resolveGpuBuiltinLocalName(const std::string &gpuBuiltin);
bool emitGpuBuiltinLoad(
    const std::string &gpuBuiltin,
    const std::function<std::optional<int32_t>(const char *)> &resolveLocalIndex,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
enum class UnaryPassthroughCallResult {
  NotMatched,
  Emitted,
  Error,
};
enum class BufferBuiltinCallEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class VectorStatementHelperEmitResult {
  NotMatched,
  Emitted,
  Error,
};
struct CountedLoopControl {
  int32_t counterLocal = -1;
  LocalInfo::ValueKind countKind = LocalInfo::ValueKind::Unknown;
  size_t checkIndex = 0;
  size_t jumpEndIndex = 0;
};
UnaryPassthroughCallResult tryEmitUnaryPassthroughCall(const Expr &expr,
                                                       const char *callName,
                                                       const std::function<bool(const Expr &)> &emitExpr,
                                                       std::string &error);
bool resolveCountedLoopKind(LocalInfo::ValueKind inferredKind,
                            bool allowBool,
                            const char *errorMessage,
                            LocalInfo::ValueKind &countKindOut,
                            std::string &error);
bool emitCountedLoopPrologue(
    LocalInfo::ValueKind countKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, int32_t)> &patchInstructionImm,
    const std::function<void()> &emitLoopCountNegative,
    CountedLoopControl &out,
    std::string &error);
void emitCountedLoopIterationStep(
    const CountedLoopControl &control,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
void patchCountedLoopEnd(
    const CountedLoopControl &control,
    const std::function<size_t()> &instructionCount,
    const std::function<void(size_t, int32_t)> &patchInstructionImm);
bool emitBodyStatements(
    const std::vector<Expr> &bodyStatements,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, LocalMap &)> &emitStatement);
bool emitBodyStatementsWithFileScope(
    const std::vector<Expr> &bodyStatements,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, LocalMap &)> &emitStatement,
    const std::function<bool()> &emitAfterBody,
    const std::function<void()> &pushFileScope,
    const std::function<void()> &emitCurrentFileScopeCleanup,
    const std::function<void()> &popFileScope);
bool declareForConditionBinding(
    const Expr &binding,
    LocalMap &locals,
    int32_t &nextLocal,
    const std::function<bool(const Expr &)> &isBindingMutable,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const Expr &)> &hasExplicitBindingTypeTransform,
    const std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)> &bindingValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<void(const Expr &, LocalInfo &)> &applyStructArrayInfo,
    const std::function<void(const Expr &, LocalInfo &)> &applyStructValueInfo,
    std::string &error);
bool emitForConditionBindingInit(
    const Expr &binding,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<const Definition *(const std::string &)> &resolveDestroyHelperForStruct,
    const std::function<const Definition *(const std::string &)> &resolveMoveHelperForStruct,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error);
VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<const Definition *(const std::string &)> &resolveDestroyHelperForStruct,
    const std::function<const Definition *(const std::string &)> &resolveMoveHelperForStruct,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error);
VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error);
VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error);
VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error);
VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error);
struct BufferInitInfo {
  int32_t count = 0;
  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  IrOpcode zeroOpcode = IrOpcode::PushI32;
};
bool resolveBufferInitInfo(const Expr &expr,
                           const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
                           BufferInitInfo &out,
                           std::string &error);
struct BufferLoadInfo {
  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
};
bool resolveBufferLoadInfo(
    const Expr &expr,
    const std::function<std::optional<LocalInfo::ValueKind>(const Expr &)> &resolveBufferElemKind,
    const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &)> &inferExprKind,
    BufferLoadInfo &out,
    std::string &error);
bool emitBufferLoadCall(const Expr &expr,
                        LocalInfo::ValueKind indexKind,
                        const std::function<bool(const Expr &)> &emitExpr,
                        const std::function<int32_t()> &allocTempLocal,
                        const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
BufferBuiltinCallEmitResult tryEmitBufferBuiltinCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t(int32_t)> &allocLocalRange,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
