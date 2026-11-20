#include "WasmEmitterInternal.h"

#include <utility>

namespace primec {

namespace {

constexpr uint8_t WasmBlockTypeVoid = 0x40;
constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpElse = 0x05;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpBlock = 0x02;
constexpr uint8_t WasmOpLoop = 0x03;
constexpr uint8_t WasmOpBr = 0x0c;
constexpr uint8_t WasmOpBrIf = 0x0d;
constexpr uint8_t WasmOpI32Eqz = 0x45;
constexpr uint8_t WasmOpUnreachable = 0x00;

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

constexpr size_t NoExitTarget = static_cast<size_t>(-1);

// The innermost label a jump to IR index `target` leaves by, or null.
const WasmJumpLabel *findJumpLabel(const WasmLocalLayout &layout, size_t target) {
  for (size_t i = layout.jumpLabels.size(); i > 0; --i) {
    if (layout.jumpLabels[i - 1].irTarget == target) {
      return &layout.jumpLabels[i - 1];
    }
  }
  return nullptr;
}

void appendBranchToLabel(const WasmLocalLayout &layout,
                         const WasmJumpLabel &label,
                         std::vector<uint8_t> &out) {
  out.push_back(WasmOpBr);
  appendU32Leb(layout.controlDepth - label.depth, out);
}

// Whether [start, end) holds a jump to `target` that leaves the construct it is written in: the early exit of an arm
// of the `if`/`else` that ends at `target`. A jump that closes an `if`/`else` nested in an else arm (the end of the
// arm is `target` itself) is not one; the instruction before the jump's index is the target of that `if`'s
// `JumpIfZero`.
bool hasEscapingJump(const IrFunction &function, size_t start, size_t end, size_t target) {
  for (size_t p = start; p < end; ++p) {
    const IrInstruction &inst = function.instructions[p];
    if (inst.op != IrOpcode::Jump || inst.imm != target) {
      continue;
    }
    if (end < target) {
      return true;
    }
    bool closesNestedIf = false;
    for (size_t q = start; q < p && !closesNestedIf; ++q) {
      const IrInstruction &guard = function.instructions[q];
      closesNestedIf = guard.op == IrOpcode::JumpIfZero && guard.imm == p + 1;
    }
    if (!closesNestedIf) {
      return true;
    }
  }
  return false;
}

// `JumpIfZero` at `conditionIndex` whose body [conditionIndex + 1, bodyEnd) ends in a jump out of the enclosing
// construct (to `label`): wasm `if` of the body followed by `br`; the code from the jump-over target on follows it.
bool emitIfThenBranch(const IrFunction &function,
                      size_t bodyStart,
                      size_t bodyEnd,
                      const WasmJumpLabel &label,
                      WasmLocalLayout &localLayout,
                      const std::vector<WasmFunctionType> &functionTypes,
                      const WasmRuntimeContext &runtime,
                      std::vector<uint8_t> &out,
                      std::string &error) {
  if (!consumeWasmCondition(localLayout, out, error, function.name)) {
    return false;
  }
  std::vector<uint8_t> outerStack = enterWasmBlock(localLayout);
  ++localLayout.controlDepth;
  std::vector<uint8_t> body;
  if (!emitInstructionRange(
          function, bodyStart, bodyEnd, localLayout, functionTypes, runtime, body, error)) {
    return false;
  }
  if (!localLayout.typeState.stack.empty()) {
    error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
    return false;
  }
  appendBranchToLabel(localLayout, label, body);
  --localLayout.controlDepth;
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  out.insert(out.end(), body.begin(), body.end());
  out.push_back(WasmOpEnd);
  leaveWasmBlock(localLayout, std::move(outerStack));
  return true;
}

// One arm of an `if`/`else` that ends at IR index `exitTarget`. An arm with a jump to `exitTarget` inside it (an
// early exit) is wrapped in a `block` the jump leaves by `br`: the end of the block is the end of the arm, and the end
// of the arm is the end of the construct. Such an arm yields no value.
bool emitIfArm(const IrFunction &function,
               size_t start,
               size_t end,
               size_t exitTarget,
               WasmLocalLayout &localLayout,
               const std::vector<WasmFunctionType> &functionTypes,
               const WasmRuntimeContext &runtime,
               std::vector<uint8_t> &out,
               std::string &error,
               bool *outDiverges) {
  const bool wrap = exitTarget != NoExitTarget && hasEscapingJump(function, start, end, exitTarget);
  if (!wrap) {
    return emitInstructionRange(
        function, start, end, localLayout, functionTypes, runtime, out, error, outDiverges);
  }
  ++localLayout.controlDepth;
  localLayout.jumpLabels.push_back({exitTarget, localLayout.controlDepth});
  std::vector<uint8_t> body;
  if (!emitInstructionRange(
          function, start, end, localLayout, functionTypes, runtime, body, error, nullptr)) {
    return false;
  }
  localLayout.jumpLabels.pop_back();
  --localLayout.controlDepth;
  if (!localLayout.typeState.stack.empty()) {
    error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
    return false;
  }
  out.push_back(WasmOpBlock);
  out.push_back(WasmBlockTypeVoid);
  out.insert(out.end(), body.begin(), body.end());
  out.push_back(WasmOpEnd);
  if (outDiverges != nullptr) {
    *outDiverges = false;
  }
  return true;
}

bool emitIfRegion(const IrFunction &function,
                  size_t conditionIndex,
                  size_t trueStart,
                  size_t trueEnd,
                  size_t falseStart,
                  size_t falseEnd,
                  size_t exitTarget,
                  WasmLocalLayout &localLayout,
                  const std::vector<WasmFunctionType> &functionTypes,
                  const WasmRuntimeContext &runtime,
                  std::vector<uint8_t> &out,
                  std::string &error,
                  bool *outDiverges) {
  (void)conditionIndex;
  if (!consumeWasmCondition(localLayout, out, error, function.name)) {
    return false;
  }
  // Both arms are emitted apart first: a value an arm leaves on the stack gives the `if` its result type, which the
  // block type in front of the arms must name.
  std::vector<uint8_t> outerStack = enterWasmBlock(localLayout);
  ++localLayout.controlDepth;
  std::vector<uint8_t> trueCode;
  bool trueDiverges = false;
  if (!emitIfArm(function,
                 trueStart,
                 trueEnd,
                 exitTarget,
                 localLayout,
                 functionTypes,
                 runtime,
                 trueCode,
                 error,
                 &trueDiverges)) {
    return false;
  }
  const std::vector<uint8_t> trueStack = localLayout.typeState.stack;
  const bool hasElse = falseStart < falseEnd;
  std::vector<uint8_t> falseCode;
  std::vector<uint8_t> falseStack;
  bool falseDiverges = false;
  if (hasElse) {
    localLayout.typeState.stack.clear();
    if (!emitIfArm(function,
                   falseStart,
                   falseEnd,
                   exitTarget,
                   localLayout,
                   functionTypes,
                   runtime,
                   falseCode,
                   error,
                   &falseDiverges)) {
      return false;
    }
    falseStack = localLayout.typeState.stack;
  }
  --localLayout.controlDepth;

  uint8_t resultType = WasmBlockTypeVoid;
  if (hasElse) {
    const std::vector<uint8_t> *yielded[2] = {trueDiverges ? nullptr : &trueStack,
                                              falseDiverges ? nullptr : &falseStack};
    uint8_t merged = WasmTypeUnknown;
    size_t yielding = 0;
    size_t valueCount = 0;
    for (const std::vector<uint8_t> *stack : yielded) {
      if (stack == nullptr) {
        continue;
      }
      if (yielding > 0 && stack->size() != valueCount) {
        error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
        return false;
      }
      valueCount = stack->size();
      ++yielding;
      if (valueCount == 1 && !mergeWasmTypes(merged, stack->back(), merged)) {
        error = "wasm emitter value type mismatch in function: " + function.name;
        return false;
      }
    }
    if (valueCount > 1) {
      error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
      return false;
    }
    if (valueCount == 1) {
      resultType = merged == WasmTypeUnknown ? WasmTypeI32 : merged;
      // Each arm that falls through converts its value to the merged type (an i32 arm of an i64 `if`).
      for (size_t arm = 0; arm < 2; ++arm) {
        if (yielded[arm] == nullptr) {
          continue;
        }
        localLayout.typeState.stack = *yielded[arm];
        if (!coerceWasmTopValue(
                localLayout, resultType, arm == 0 ? trueCode : falseCode, error, function.name)) {
          return false;
        }
      }
    }
  }

  out.push_back(WasmOpIf);
  out.push_back(resultType == WasmBlockTypeVoid ? resultType : wasmValueTypeByte(resultType));
  out.insert(out.end(), trueCode.begin(), trueCode.end());
  if (hasElse) {
    out.push_back(WasmOpElse);
    out.insert(out.end(), falseCode.begin(), falseCode.end());
  }
  out.push_back(WasmOpEnd);
  leaveWasmBlock(localLayout, std::move(outerStack));
  if (resultType != WasmBlockTypeVoid) {
    localLayout.typeState.stack.push_back(resultType);
  }
  if (outDiverges != nullptr) {
    *outDiverges = hasElse && trueDiverges && falseDiverges;
  }
  return true;
}

} // namespace

bool emitInstructionRange(const IrFunction &function,
                          size_t start,
                          size_t end,
                          WasmLocalLayout &localLayout,
                          const std::vector<WasmFunctionType> &functionTypes,
                          const WasmRuntimeContext &runtime,
                          std::vector<uint8_t> &out,
                          std::string &error,
                          bool *outDiverges) {
  size_t index = start;
  bool diverges = false;
  while (index < end) {
    diverges = false;
    LoopRegion loopRegion;
    if (detectCanonicalLoopRegion(function, index, end, loopRegion, error)) {
      out.push_back(WasmOpBlock);
      out.push_back(WasmBlockTypeVoid);
      out.push_back(WasmOpLoop);
      out.push_back(WasmBlockTypeVoid);
      std::vector<uint8_t> outerStack = enterWasmBlock(localLayout);
      localLayout.controlDepth += 2;

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
      if (!consumeWasmCondition(localLayout, out, error, function.name)) {
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
      localLayout.controlDepth -= 2;
      leaveWasmBlock(localLayout, std::move(outerStack));

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

      // `first || (a && b)`: the chain becomes one condition value, and the then-part starts where the chain ends.
      size_t thenStart = index + 1;
      WasmShortCircuitCondition shortCircuit;
      if (target == index + 2 &&
          findWasmShortCircuitCondition(function, index, end, shortCircuit)) {
        if (!emitWasmShortCircuitCondition(
                function, shortCircuit, localLayout, functionTypes, runtime, out, error)) {
          return false;
        }
        thenStart = shortCircuit.thenStart;
        target = shortCircuit.elseStart;
      }

      if (target > 0 && target - 1 >= thenStart) {
        const IrInstruction &tail = function.instructions[target - 1];
        if (tail.op == IrOpcode::Jump) {
          const size_t jumpTarget = static_cast<size_t>(tail.imm);
          if (jumpTarget <= target - 1) {
            error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
            return false;
          }
          if (jumpTarget > end) {
            // The body ends in a jump out of the enclosing `if`/`else` (an early exit of one of its arms).
            const WasmJumpLabel *label = findJumpLabel(localLayout, jumpTarget);
            if (label == nullptr) {
              error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
              return false;
            }
            if (!emitIfThenBranch(function,
                                  thenStart,
                                  target - 1,
                                  *label,
                                  localLayout,
                                  functionTypes,
                                  runtime,
                                  out,
                                  error)) {
              return false;
            }
            diverges = false;
            index = target;
            continue;
          }

          // wasm's `if` executes its then-slot when the popped condition is
          // non-zero - exactly IrOpcode::JumpIfZero's own fall-through
          // condition (see decodeJumpTarget's caller: JumpIfZero jumps to
          // `target` when the value is zero, falls through to index+1
          // otherwise), so the already-computed condition on the wasm
          // stack is used as-is. An earlier version of this function
          // pushed `i32.eqz` here, inverting the condition without
          // swapping which IR range fills the then/else slots below -
          // that put the fall-through-when-true content in the slot wasm
          // only runs when the condition is *false*, executing the wrong
          // branch (or producing an unbalanced stack that fails wasm
          // validation) for essentially any if/else or early-return-from-
          // if pattern. See TODO-4748.
          bool ifDiverges = false;
          if (!emitIfRegion(function,
                            index,
                            thenStart,
                            target - 1,
                            target,
                            jumpTarget,
                            jumpTarget,
                            localLayout,
                            functionTypes,
                            runtime,
                            out,
                            error,
                            &ifDiverges)) {
            return false;
          }
          if (ifDiverges) {
            // Both arms end in an explicit Return* - wasm's own validator
            // does not infer that code is unreachable here just because
            // neither arm falls through normally (the void if/else's own
            // `end` still resumes the *outer* frame in normal, reachable
            // state, which then needs whatever fallthru type that outer
            // frame expects - e.g. the enclosing function's declared
            // return type, if this if/else is the function's last
            // construct). An explicit `unreachable` here tells the
            // validator so, matching this range's actual dynamic
            // behavior. See TODO-4748.
            out.push_back(WasmOpUnreachable);
          }
          diverges = ifDiverges;
          index = jumpTarget;
          continue;
        }
      }

      // Same reasoning as above - no else branch here (falseStart==falseEnd
      // below), but the condition still must not be inverted. An `if`
      // without an `else` never diverges: the implicit empty else always
      // reaches the end normally, so no `unreachable` hint is needed or
      // correct here.
      if (!emitIfRegion(function,
                        index,
                        thenStart,
                        target,
                        target,
                        target,
                        NoExitTarget,
                        localLayout,
                        functionTypes,
                        runtime,
                        out,
                        error,
                        nullptr)) {
        return false;
      }
      diverges = false;
      index = target;
      continue;
    }

    if (inst.op == IrOpcode::Jump) {
      size_t target = 0;
      if (!decodeJumpTarget(function, inst, target)) {
        error = "wasm emitter malformed jump target in function: " + function.name;
        return false;
      }
      if (target == index + 1) {
        // A jump to the next instruction (the end of an inlined body that returns a value) does nothing.
        ++index;
        continue;
      }
      if (const WasmJumpLabel *label = findJumpLabel(localLayout, target)) {
        appendBranchToLabel(localLayout, *label, out);
        diverges = true;
        ++index;
        continue;
      }
      error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
      return false;
    }

    if (!emitTypedInstruction(
            inst, localLayout, functionTypes, runtime, out, error, function.name)) {
      return false;
    }
    diverges = inst.op == IrOpcode::ReturnVoid || inst.op == IrOpcode::ReturnI32 ||
               inst.op == IrOpcode::ReturnI64 || inst.op == IrOpcode::ReturnF32 ||
               inst.op == IrOpcode::ReturnF64 || inst.op == IrOpcode::Trap;
    ++index;
  }

  if (outDiverges != nullptr) {
    *outDiverges = diverges;
  }
  return true;
}

} // namespace primec
