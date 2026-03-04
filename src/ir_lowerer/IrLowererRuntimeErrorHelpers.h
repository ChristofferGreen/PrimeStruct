#pragma once

#include <functional>
#include <string>

#include "primec/Ir.h"

namespace primec::ir_lowerer {

using InternRuntimeErrorStringFn = std::function<int32_t(const std::string &)>;
using EmitRuntimeErrorFn = std::function<void()>;

EmitRuntimeErrorFn makeEmitArrayIndexOutOfBounds(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitStringIndexOutOfBounds(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitMapKeyNotFound(IrFunction &function, const InternRuntimeErrorStringFn &internString);
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

} // namespace primec::ir_lowerer
