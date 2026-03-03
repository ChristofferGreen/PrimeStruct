#pragma once

#include <functional>
#include <string>

#include "primec/Ir.h"

namespace primec::ir_lowerer {

using InternRuntimeErrorStringFn = std::function<int32_t(const std::string &)>;

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
