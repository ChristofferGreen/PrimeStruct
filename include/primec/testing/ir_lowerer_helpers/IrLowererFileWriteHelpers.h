



using ResolveStringTableTargetForWriteFn = std::function<bool(const Expr &, int32_t &, size_t &)>;
using InferExprKindForWriteFn = std::function<LocalInfo::ValueKind(const Expr &)>;
using EmitExprForWriteFn = std::function<bool(const Expr &)>;
using EmitInstructionForWriteFn = std::function<void(IrOpcode, uint64_t)>;
using AllocTempLocalForWriteFn = std::function<int32_t()>;
using GetInstructionCountForWriteFn = std::function<size_t()>;
using PatchInstructionImmForWriteFn = std::function<void(size_t, int32_t)>;
using EmitFileWriteStepFn = std::function<bool(const Expr &, int32_t)>;
using ResolveStringTableTargetWithLocalsForWriteFn =
    std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)>;
using InferExprKindWithLocalsForWriteFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using EmitExprWithLocalsForWriteFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsEntryArgsNameWithLocalsForWriteFn = std::function<bool(const Expr &, const LocalMap &)>;
using ShouldBypassBuiltinFileMethodWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;

enum class FileHandleMethodCallEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class FileConstructorCallEmitResult {
  NotMatched,
  Emitted,
  Error,
};

bool resolveFileOpenModeOpcode(const std::string &mode, IrOpcode &opcodeOut);
bool resolveDynamicFileOpenModeOpcode(const std::string &mode, IrOpcode &opcodeOut);
bool emitFileOpenCall(const std::string &mode,
                      int32_t stringIndex,
                      const EmitInstructionForWriteFn &emitInstruction,
                      std::string &error);
FileConstructorCallEmitResult tryEmitFileConstructorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const InferExprKindWithLocalsForWriteFn &inferExprKind,
    const EmitExprWithLocalsForWriteFn &emitExpr,
    const IsEntryArgsNameWithLocalsForWriteFn &isEntryArgsName,
    const EmitInstructionForWriteFn &emitInstruction,
    std::string &error);
FileConstructorCallEmitResult tryEmitFileConstructorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const EmitInstructionForWriteFn &emitInstruction,
    std::string &error);
bool resolveFileWriteValueOpcode(LocalInfo::ValueKind kind, IrOpcode &opcodeOut);
bool emitFileWriteStep(const Expr &arg,
                       int32_t handleIndex,
                       int32_t errorLocal,
                       const ResolveStringTableTargetForWriteFn &resolveStringTableTarget,
                       const InferExprKindForWriteFn &inferExprKind,
                       const EmitExprForWriteFn &emitExpr,
                       const EmitInstructionForWriteFn &emitInstruction,
                       std::string &error);
bool emitFileWriteCall(const Expr &expr,
                       int32_t handleIndex,
                       const EmitFileWriteStepFn &emitWriteStep,
                       const AllocTempLocalForWriteFn &allocTempLocal,
                       const EmitInstructionForWriteFn &emitInstruction,
                       const GetInstructionCountForWriteFn &getInstructionCount,
                       const PatchInstructionImmForWriteFn &patchInstructionImm);
bool emitFileWriteByteCall(const Expr &expr,
                           int32_t handleIndex,
                           const EmitExprForWriteFn &emitExpr,
                           const EmitInstructionForWriteFn &emitInstruction,
                           std::string &error);
bool emitFileReadByteCall(const Expr &expr,
                          const LocalMap &localsIn,
                          int32_t handleIndex,
                          const AllocTempLocalForWriteFn &allocTempLocal,
                          const EmitInstructionForWriteFn &emitInstruction,
                          const GetInstructionCountForWriteFn &getInstructionCount,
                          const PatchInstructionImmForWriteFn &patchInstructionImm,
                          std::string &error);
bool emitFileReadByteCall(const Expr &expr,
                          const LocalMap &localsIn,
                          int32_t handleIndex,
                          const EmitInstructionForWriteFn &emitInstruction,
                          std::string &error);
bool emitFileWriteBytesCall(const Expr &expr,
                            int32_t handleIndex,
                            const EmitExprForWriteFn &emitExpr,
                            const AllocTempLocalForWriteFn &allocTempLocal,
                            const EmitInstructionForWriteFn &emitInstruction,
                            const GetInstructionCountForWriteFn &getInstructionCount,
                            const PatchInstructionImmForWriteFn &patchInstructionImm,
                            std::string &error);
bool emitFileWriteBytesLoop(const Expr &bytesExpr,
                            int32_t handleIndex,
                            const EmitExprForWriteFn &emitExpr,
                            const AllocTempLocalForWriteFn &allocTempLocal,
                            const EmitInstructionForWriteFn &emitInstruction,
                            const GetInstructionCountForWriteFn &getInstructionCount,
                            const PatchInstructionImmForWriteFn &patchInstructionImm);
FileHandleMethodCallEmitResult tryEmitFileHandleMethodCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ShouldBypassBuiltinFileMethodWithLocalsFn &shouldBypassBuiltin,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const InferExprKindWithLocalsForWriteFn &inferExprKind,
    const EmitExprWithLocalsForWriteFn &emitExpr,
    const AllocTempLocalForWriteFn &allocTempLocal,
    const EmitInstructionForWriteFn &emitInstruction,
    const GetInstructionCountForWriteFn &getInstructionCount,
    const PatchInstructionImmForWriteFn &patchInstructionImm,
    std::string &error);
void emitFileFlushCall(int32_t handleIndex, const EmitInstructionForWriteFn &emitInstruction);
void emitFileCloseCall(int32_t handleIndex,
                       const AllocTempLocalForWriteFn &allocTempLocal,
                       const EmitInstructionForWriteFn &emitInstruction);

