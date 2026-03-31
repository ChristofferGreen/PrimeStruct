



using InternRuntimeErrorStringFn = std::function<int32_t(const std::string &)>;
using EmitRuntimeErrorFn = std::function<void()>;

struct RuntimeErrorEmitters {
  EmitRuntimeErrorFn emitArrayIndexOutOfBounds{};
  EmitRuntimeErrorFn emitPointerIndexOutOfBounds{};
  EmitRuntimeErrorFn emitStringIndexOutOfBounds{};
  EmitRuntimeErrorFn emitMapKeyNotFound{};
  EmitRuntimeErrorFn emitBuiltinCanonicalMapInsertPending{};
  EmitRuntimeErrorFn emitSoaArbitraryWidthPending{};
  EmitRuntimeErrorFn emitVectorIndexOutOfBounds{};
  EmitRuntimeErrorFn emitVectorPopOnEmpty{};
  EmitRuntimeErrorFn emitVectorCapacityExceeded{};
  EmitRuntimeErrorFn emitVectorReserveNegative{};
  EmitRuntimeErrorFn emitVectorReserveExceeded{};
  EmitRuntimeErrorFn emitLoopCountNegative{};
  EmitRuntimeErrorFn emitPowNegativeExponent{};
  EmitRuntimeErrorFn emitFloatToIntNonFinite{};
};

struct RuntimeErrorAndStringLiteralSetup {
  StringLiteralHelperContext stringLiteralHelpers{};
  RuntimeErrorEmitters runtimeErrorEmitters{};
};
enum class FileErrorWhyCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};

RuntimeErrorAndStringLiteralSetup makeRuntimeErrorAndStringLiteralSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    std::string &error);
RuntimeErrorEmitters makeRuntimeErrorEmitters(IrFunction &function, const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitArrayIndexOutOfBounds(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitPointerIndexOutOfBounds(IrFunction &function,
                                                   const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitStringIndexOutOfBounds(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitMapKeyNotFound(IrFunction &function, const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitBuiltinCanonicalMapInsertPending(IrFunction &function,
                                                            const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitSoaArbitraryWidthPending(IrFunction &function,
                                                    const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitVectorIndexOutOfBounds(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitVectorPopOnEmpty(IrFunction &function, const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitVectorCapacityExceeded(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitVectorReserveNegative(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitVectorReserveExceeded(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitLoopCountNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitPowNegativeExponent(IrFunction &function, const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitFloatToIntNonFinite(IrFunction &function,
                                                const InternRuntimeErrorStringFn &internString);

void emitArrayIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitPointerIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitStringIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitMapKeyNotFound(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitBuiltinCanonicalMapInsertPending(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitSoaArbitraryWidthPending(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitVectorIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitVectorPopOnEmpty(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitVectorCapacityExceeded(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitVectorReserveNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitVectorReserveExceeded(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitLoopCountNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitPowNegativeExponent(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitFloatToIntNonFinite(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitFileErrorWhy(IrFunction &function, int32_t errorLocal, const InternRuntimeErrorStringFn &internString);
FileErrorWhyCallEmitResult tryEmitFileErrorWhyCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(int32_t)> &emitFileErrorWhy,
    std::string &error);
