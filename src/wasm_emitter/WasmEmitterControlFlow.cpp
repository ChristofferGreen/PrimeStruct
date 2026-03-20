#include "WasmEmitterInternal.h"

namespace primec {

namespace {

constexpr uint8_t WasmBlockTypeVoid = 0x40;
constexpr uint8_t WasmOpElse = 0x05;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpBlock = 0x02;
constexpr uint8_t WasmOpLoop = 0x03;
constexpr uint8_t WasmOpBr = 0x0c;
constexpr uint8_t WasmOpBrIf = 0x0d;
constexpr uint8_t WasmOpI32Eqz = 0x45;

struct LoopRegion {
  size_t guardIndex = 0;
  size_t tailJumpIndex = 0;
  size_t endIndex = 0;
};

bool decodeJumpTarget(const IrFunction &function, const IrInstruction &inst, size_t &target) {
  const uint64_t instructionCount = static_cast<uint64_t>(function.instructions.size());
  if (inst.imm > instructionCount) {
    return false;
  }
  target = static_cast<size_t>(inst.imm);
  return true;
}

bool detectCanonicalLoopRegion(
    const IrFunction &function, size_t loopHead, size_t end, LoopRegion &outRegion, std::string &error) {
  bool found = false;
  LoopRegion candidate;

  for (size_t i = loopHead + 1; i < end; ++i) {
    const IrInstruction &inst = function.instructions[i];
    if (inst.op != IrOpcode::Jump) {
      continue;
    }

    size_t jumpTarget = 0;
    if (!decodeJumpTarget(function, inst, jumpTarget)) {
      error = "wasm emitter malformed jump target in function: " + function.name;
      return false;
    }
    if (jumpTarget != loopHead) {
      continue;
    }
    if (i + 1 > end) {
      continue;
    }

    size_t guardIndex = static_cast<size_t>(-1);
    const size_t loopEnd = i + 1;
    for (size_t j = loopHead; j < i; ++j) {
      const IrInstruction &guard = function.instructions[j];
      if (guard.op != IrOpcode::JumpIfZero) {
        continue;
      }
      size_t guardTarget = 0;
      if (!decodeJumpTarget(function, guard, guardTarget)) {
        error = "wasm emitter malformed jump target in function: " + function.name;
        return false;
      }
      if (guardTarget == loopEnd) {
        guardIndex = j;
        break;
      }
    }
    if (guardIndex == static_cast<size_t>(-1)) {
      continue;
    }

    candidate.guardIndex = guardIndex;
    candidate.tailJumpIndex = i;
    candidate.endIndex = loopEnd;
    found = true;
  }

  if (!found) {
    return false;
  }
  outRegion = candidate;
  return true;
}

bool emitIfRegion(const IrFunction &function,
                  size_t conditionIndex,
                  size_t trueStart,
                  size_t trueEnd,
                  size_t falseStart,
                  size_t falseEnd,
                  const WasmLocalLayout &localLayout,
                  const std::vector<WasmFunctionType> &functionTypes,
                  const WasmRuntimeContext &runtime,
                  std::vector<uint8_t> &out,
                  std::string &error) {
  (void)conditionIndex;
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  if (!emitInstructionRange(function, trueStart, trueEnd, localLayout, functionTypes, runtime, out, error)) {
    return false;
  }
  if (falseStart < falseEnd) {
    out.push_back(WasmOpElse);
    if (!emitInstructionRange(
            function, falseStart, falseEnd, localLayout, functionTypes, runtime, out, error)) {
      return false;
    }
  }
  out.push_back(WasmOpEnd);
  return true;
}

} // namespace

bool emitInstructionRange(const IrFunction &function,
                          size_t start,
                          size_t end,
                          const WasmLocalLayout &localLayout,
                          const std::vector<WasmFunctionType> &functionTypes,
                          const WasmRuntimeContext &runtime,
                          std::vector<uint8_t> &out,
                          std::string &error) {
  size_t index = start;
  while (index < end) {
    LoopRegion loopRegion;
    if (detectCanonicalLoopRegion(function, index, end, loopRegion, error)) {
      out.push_back(WasmOpBlock);
      out.push_back(WasmBlockTypeVoid);
      out.push_back(WasmOpLoop);
      out.push_back(WasmBlockTypeVoid);

      if (!emitInstructionRange(
              function,
              index,
              loopRegion.guardIndex,
              localLayout,
              functionTypes,
              runtime,
              out,
              error)) {
        return false;
      }
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpBrIf);
      appendU32Leb(1, out);

      if (!emitInstructionRange(
              function,
              loopRegion.guardIndex + 1,
              loopRegion.tailJumpIndex,
              localLayout,
              functionTypes,
              runtime,
              out,
              error)) {
        return false;
      }
      out.push_back(WasmOpBr);
      appendU32Leb(0, out);
      out.push_back(WasmOpEnd);
      out.push_back(WasmOpEnd);

      index = loopRegion.endIndex;
      continue;
    }

    const IrInstruction &inst = function.instructions[index];

    if (inst.op == IrOpcode::JumpIfZero) {
      size_t target = 0;
      if (!decodeJumpTarget(function, inst, target)) {
        error = "wasm emitter malformed jump target in function: " + function.name;
        return false;
      }
      if (target <= index || target > end) {
        error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
        return false;
      }

      if (target > 0 && target - 1 > index) {
        const IrInstruction &tail = function.instructions[target - 1];
        if (tail.op == IrOpcode::Jump) {
          const size_t jumpTarget = static_cast<size_t>(tail.imm);
          if (jumpTarget <= target - 1) {
            error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
            return false;
          }
          if (jumpTarget > end) {
            error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
            return false;
          }

          out.push_back(WasmOpI32Eqz);
          if (!emitIfRegion(function,
                            index,
                            index + 1,
                            target - 1,
                            target,
                            jumpTarget,
                            localLayout,
                            functionTypes,
                            runtime,
                            out,
                            error)) {
            return false;
          }
          index = jumpTarget;
          continue;
        }
      }

      out.push_back(WasmOpI32Eqz);
      if (!emitIfRegion(function,
                        index,
                        index + 1,
                        target,
                        target,
                        target,
                        localLayout,
                        functionTypes,
                        runtime,
                        out,
                        error)) {
        return false;
      }
      index = target;
      continue;
    }

    if (inst.op == IrOpcode::Jump) {
      size_t target = 0;
      if (!decodeJumpTarget(function, inst, target)) {
        error = "wasm emitter malformed jump target in function: " + function.name;
        return false;
      }
      (void)target;
      error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
      return false;
    }

    if (!emitSimpleInstruction(inst, localLayout, functionTypes, runtime, out, error, function.name)) {
      return false;
    }
    ++index;
  }

  return true;
}

} // namespace primec
