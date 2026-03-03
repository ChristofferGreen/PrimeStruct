#include "IrLowererRuntimeErrorHelpers.h"

namespace primec::ir_lowerer {

namespace {
void emitRuntimeError(IrFunction &function, const std::string &message, const InternRuntimeErrorStringFn &internString) {
  uint64_t flags = encodePrintFlags(true, true);
  int32_t msgIndex = internString(message);
  function.instructions.push_back({IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
  function.instructions.push_back({IrOpcode::PushI32, 3});
  function.instructions.push_back({IrOpcode::ReturnI32, 0});
}
} // namespace

void emitArrayIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "array index out of bounds", internString);
}

void emitStringIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "string index out of bounds", internString);
}

void emitMapKeyNotFound(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "map key not found", internString);
}

void emitVectorIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "vector index out of bounds", internString);
}

void emitVectorPopOnEmpty(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "vector pop on empty", internString);
}

void emitVectorCapacityExceeded(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "vector capacity exceeded", internString);
}

void emitVectorReserveNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "vector reserve expects non-negative capacity", internString);
}

void emitVectorReserveExceeded(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "vector reserve exceeds capacity", internString);
}

void emitLoopCountNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "loop count must be non-negative", internString);
}

void emitPowNegativeExponent(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "pow exponent must be non-negative", internString);
}

void emitFloatToIntNonFinite(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "float to int conversion requires finite value", internString);
}

} // namespace primec::ir_lowerer
