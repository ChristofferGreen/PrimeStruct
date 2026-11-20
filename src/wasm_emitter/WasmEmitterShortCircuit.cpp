#include "WasmEmitterInternal.h"

#include <utility>

namespace primec {

namespace {

constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpElse = 0x05;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpI32Const = 0x41;

// The value (0 or 1) of the conjunction of `operands[k..]`: each operand is code that leaves one condition value, and
// a false operand stops the chain with 0.
bool emitConjunction(const IrFunction &function,
                     const WasmShortCircuitCondition &condition,
                     size_t k,
                     WasmLocalLayout &localLayout,
                     const std::vector<WasmFunctionType> &functionTypes,
                     const WasmRuntimeContext &runtime,
                     std::vector<uint8_t> &out,
                     std::string &error) {
  std::vector<uint8_t> outerStack = enterWasmBlock(localLayout);
  const WasmShortCircuitOperand &operand = condition.operands[k];
  if (!emitInstructionRange(
          function, operand.start, operand.end, localLayout, functionTypes, runtime, out, error)) {
    return false;
  }
  if (localLayout.typeState.stack.size() != 1) {
    error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
    return false;
  }
  if (!consumeWasmCondition(localLayout, out, error, function.name)) {
    return false;
  }
  if (k + 1 < condition.operands.size()) {
    ++localLayout.controlDepth;
    std::vector<uint8_t> rest;
    const bool ok = emitConjunction(function, condition, k + 1, localLayout, functionTypes, runtime, rest, error);
    --localLayout.controlDepth;
    if (!ok) {
      return false;
    }
    out.push_back(WasmOpIf);
    out.push_back(WasmTypeI32);
    out.insert(out.end(), rest.begin(), rest.end());
    out.push_back(WasmOpElse);
    out.push_back(WasmOpI32Const);
    appendS32Leb(0, out);
    out.push_back(WasmOpEnd);
  }
  leaveWasmBlock(localLayout, std::move(outerStack));
  return true;
}

} // namespace

bool findWasmShortCircuitCondition(const IrFunction &function,
                                   size_t conditionIndex,
                                   size_t end,
                                   WasmShortCircuitCondition &condition) {
  const std::vector<IrInstruction> &code = function.instructions;
  const size_t secondStart = conditionIndex + 2;
  if (secondStart >= code.size() || code[conditionIndex + 1].op != IrOpcode::Jump) {
    return false;
  }
  const uint64_t thenStart = code[conditionIndex + 1].imm;
  if (thenStart <= secondStart || thenStart > end) {
    return false;
  }
  // The second operand of the `||` is a chain of conditions that jump to the else-part (`elseStart`), which lies
  // behind the then-part; falling out of the last one reaches the then-part.
  WasmShortCircuitCondition found;
  found.thenStart = static_cast<size_t>(thenStart);
  size_t operandStart = secondStart;
  for (size_t p = secondStart; p < found.thenStart; ++p) {
    if (code[p].op != IrOpcode::JumpIfZero || code[p].imm <= thenStart) {
      continue;
    }
    const size_t target = static_cast<size_t>(code[p].imm);
    if (target > end || (found.elseStart != 0 && target != found.elseStart)) {
      return false;
    }
    found.elseStart = target;
    found.operands.push_back({operandStart, p});
    operandStart = p + 1;
  }
  if (found.operands.empty() || operandStart != found.thenStart) {
    return false;
  }
  condition = std::move(found);
  return true;
}

bool emitWasmShortCircuitCondition(const IrFunction &function,
                                   const WasmShortCircuitCondition &condition,
                                   WasmLocalLayout &localLayout,
                                   const std::vector<WasmFunctionType> &functionTypes,
                                   const WasmRuntimeContext &runtime,
                                   std::vector<uint8_t> &out,
                                   std::string &error) {
  // `first || rest`: the first operand is the value on the stack; when it is false the chain decides.
  if (!consumeWasmCondition(localLayout, out, error, function.name)) {
    return false;
  }
  ++localLayout.controlDepth;
  std::vector<uint8_t> rest;
  const bool ok = emitConjunction(function, condition, 0, localLayout, functionTypes, runtime, rest, error);
  --localLayout.controlDepth;
  if (!ok) {
    return false;
  }
  out.push_back(WasmOpIf);
  out.push_back(WasmTypeI32);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpElse);
  out.insert(out.end(), rest.begin(), rest.end());
  out.push_back(WasmOpEnd);
  localLayout.typeState.stack.push_back(WasmTypeI32);
  return true;
}

} // namespace primec
