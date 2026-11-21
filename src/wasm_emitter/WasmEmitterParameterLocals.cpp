#include "WasmEmitterInternal.h"

#include <set>

namespace primec {

namespace {

bool isLocalOperand(IrOpcode op) {
  return op == IrOpcode::LoadLocal || op == IrOpcode::StoreLocal ||
         op == IrOpcode::AddressOfLocal || op == IrOpcode::FileReadByte;
}

// The IR locals the leading `StoreLocal`s of `function` store its parameters to, in parameter order, or false when
// the function does not start with `parameterCount` stores.
bool readParameterTargets(const IrFunction &function, std::vector<uint64_t> &targets) {
  const size_t count = function.parameterCount;
  if (function.instructions.size() < count) {
    return false;
  }
  targets.assign(count, 0);
  for (size_t i = 0; i < count; ++i) {
    const IrInstruction &inst = function.instructions[i];
    if (inst.op != IrOpcode::StoreLocal) {
      return false;
    }
    // The first store takes the last argument.
    targets[count - 1 - i] = inst.imm;
  }
  return true;
}

} // namespace

bool wasmFunctionNeedsParameterShift(const IrFunction &function) {
  std::vector<uint64_t> targets;
  if (function.parameterCount == 0 || !readParameterTargets(function, targets)) {
    return false;
  }
  for (size_t i = 0; i < targets.size(); ++i) {
    if (targets[i] != i) {
      return true;
    }
  }
  return false;
}

bool shiftWasmParameterLocals(IrFunction &function, std::string &error) {
  std::vector<uint64_t> targets;
  if (!readParameterTargets(function, targets)) {
    return true;
  }
  if (std::set<uint64_t>(targets.begin(), targets.end()).size() != targets.size()) {
    error = "wasm emitter: function " + function.name + " stores two parameters to one local";
    return false;
  }
  const uint64_t count = function.parameterCount;
  std::vector<IrInstruction> shifted;
  shifted.reserve(function.instructions.size() + 2 * count);
  // Wasm takes its parameters as locals 0..count-1. The function keeps those for the arguments and moves every local
  // of its body up by `count` (the order of the slots, which an address into the frame can walk, stays the same); the
  // prologue then copies each argument to the slot the body reads it from.
  for (uint64_t i = 0; i < count; ++i) {
    IrInstruction store = function.instructions[0];
    store.imm = count - 1 - i;
    shifted.push_back(store);
  }
  for (uint64_t i = 0; i < count; ++i) {
    IrInstruction load = function.instructions[0];
    load.op = IrOpcode::LoadLocal;
    load.imm = i;
    shifted.push_back(load);
    IrInstruction store = function.instructions[0];
    store.imm = targets[i] + count;
    shifted.push_back(store);
  }
  for (size_t index = count; index < function.instructions.size(); ++index) {
    IrInstruction inst = function.instructions[index];
    if (isLocalOperand(inst.op)) {
      inst.imm += count;
    } else if (inst.op == IrOpcode::Jump || inst.op == IrOpcode::JumpIfZero) {
      // Every jump lands after the prologue, which grew by `count` loads and `count` stores.
      inst.imm += 2 * count;
    }
    shifted.push_back(inst);
  }
  function.instructions = std::move(shifted);
  return true;
}

} // namespace primec
