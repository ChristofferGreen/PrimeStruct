#pragma once

#include <functional>
#include <string>
#include <vector>

#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ir_lowerer/IrLowererStringLiteralHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"

namespace primec::ir_lowerer {

using InternRuntimeErrorStringFn = std::function<int32_t(const std::string &)>;

struct RuntimeErrorEmitters {
  ActionFn emitArrayIndexOutOfBounds{};
  ActionFn emitPointerIndexOutOfBounds{};
  ActionFn emitStringIndexOutOfBounds{};
  ActionFn emitMapKeyNotFound{};
  ActionFn emitVectorIndexOutOfBounds{};
  ActionFn emitVectorPopOnEmpty{};
  ActionFn emitVectorCapacityExceeded{};
  ActionFn emitVectorReserveNegative{};
  ActionFn emitVectorReserveExceeded{};
  ActionFn emitLoopCountNegative{};
  ActionFn emitPowNegativeExponent{};
  ActionFn emitFloatToIntNonFinite{};
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
    std::string &error,
    const SemanticProgram *semanticProgram = nullptr);
RuntimeErrorEmitters makeRuntimeErrorEmitters(IrFunction &function, const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitArrayIndexOutOfBounds(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitPointerIndexOutOfBounds(IrFunction &function,
                                                   const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitStringIndexOutOfBounds(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitMapKeyNotFound(IrFunction &function, const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitVectorIndexOutOfBounds(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitVectorPopOnEmpty(IrFunction &function, const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitVectorCapacityExceeded(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitVectorReserveNegative(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitVectorReserveExceeded(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitLoopCountNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitPowNegativeExponent(IrFunction &function, const InternRuntimeErrorStringFn &internString);
ActionFn makeEmitFloatToIntNonFinite(IrFunction &function,
                                                const InternRuntimeErrorStringFn &internString);

void emitArrayIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitPointerIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitStringIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitMapKeyNotFound(IrFunction &function, const InternRuntimeErrorStringFn &internString);
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
    const ExprLocalsPredicateFn &emitExpr,
    const Int32ProviderFn &allocTempLocal,
    const EmitInstructionFn &emitInstruction,
    const std::function<void(int32_t)> &emitFileErrorWhy,
    std::string &error);

} // namespace primec::ir_lowerer
