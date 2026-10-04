#include "IrPassUtil.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace primec::ir_opt {
namespace {

// The integer comparison that is true exactly when `op` is false.
std::optional<IrOpcode> invertedComparison(IrOpcode op) {
  switch (op) {
  case IrOpcode::CmpEqI32:
    return IrOpcode::CmpNeI32;
  case IrOpcode::CmpNeI32:
    return IrOpcode::CmpEqI32;
  case IrOpcode::CmpLtI32:
    return IrOpcode::CmpGeI32;
  case IrOpcode::CmpGeI32:
    return IrOpcode::CmpLtI32;
  case IrOpcode::CmpLeI32:
    return IrOpcode::CmpGtI32;
  case IrOpcode::CmpGtI32:
    return IrOpcode::CmpLeI32;
  case IrOpcode::CmpEqI64:
    return IrOpcode::CmpNeI64;
  case IrOpcode::CmpNeI64:
    return IrOpcode::CmpEqI64;
  case IrOpcode::CmpLtI64:
    return IrOpcode::CmpGeI64;
  case IrOpcode::CmpGeI64:
    return IrOpcode::CmpLtI64;
  case IrOpcode::CmpLeI64:
    return IrOpcode::CmpGtI64;
  case IrOpcode::CmpGtI64:
    return IrOpcode::CmpLeI64;
  case IrOpcode::CmpLtU64:
    return IrOpcode::CmpGeU64;
  case IrOpcode::CmpGeU64:
    return IrOpcode::CmpLtU64;
  case IrOpcode::CmpLeU64:
    return IrOpcode::CmpGtU64;
  case IrOpcode::CmpGtU64:
    return IrOpcode::CmpLeU64;
  default:
    return std::nullopt;
  }
}

bool isPurePush(IrOpcode op) {
  return op == IrOpcode::LoadLocal || op == IrOpcode::PushI32 || op == IrOpcode::PushI64;
}

// A loop whose test sits in its header:
//
//   H:  push a; push b; Cmp; JumpIfZero Exit      (Exit = J + 1)
//       body
//   J:  Jump H
//
// runs a taken jump per iteration on top of the test. Rotated, the back edge is the
// test itself, with the comparison inverted so that JumpIfZero loops while the original
// condition holds:
//
//   J:  push a; push b; InvertedCmp; JumpIfZero Body
//
// The header stays for the first entry. Only a header of two pure pushes and one
// integer comparison qualifies, so the copy is as cheap as the jump it replaces.
struct Rotation {
  size_t jump = 0;      // J
  size_t body = 0;      // first instruction after the header test
  size_t header = 0;    // H
  IrOpcode inverted = IrOpcode::CmpEqI32;
};

std::vector<Rotation> findRotations(const IrFunction &function) {
  std::vector<Rotation> rotations;
  const size_t count = function.instructions.size();
  for (size_t jump = 0; jump < count; ++jump) {
    const IrInstruction &back = function.instructions[jump];
    if (back.op != IrOpcode::Jump || back.imm >= jump) {
      continue;
    }
    const size_t header = static_cast<size_t>(back.imm);
    // push; push; Cmp; JumpIfZero Exit, then at least one body instruction before J.
    if (header + 4 >= jump) {
      continue;
    }
    const IrInstruction &first = function.instructions[header];
    const IrInstruction &second = function.instructions[header + 1];
    const IrInstruction &compare = function.instructions[header + 2];
    const IrInstruction &test = function.instructions[header + 3];
    const std::optional<IrOpcode> inverted = invertedComparison(compare.op);
    if (!isPurePush(first.op) || !isPurePush(second.op) || !inverted.has_value() ||
        test.op != IrOpcode::JumpIfZero || test.imm != jump + 1) {
      continue;
    }
    rotations.push_back({jump, header + 4, header, *inverted});
  }
  return rotations;
}

bool rotateLoops(IrFunction &function) {
  const std::vector<Rotation> rotations = findRotations(function);
  if (rotations.empty()) {
    return false;
  }
  const size_t count = function.instructions.size();
  std::vector<const Rotation *> rotationAt(count, nullptr);
  for (const Rotation &rotation : rotations) {
    rotationAt[rotation.jump] = &rotation;
  }
  // Each rotated back edge grows from one instruction to four.
  std::vector<size_t> newIndex(count + 1, 0);
  size_t next = 0;
  for (size_t i = 0; i < count; ++i) {
    newIndex[i] = next;
    next += rotationAt[i] != nullptr ? 4 : 1;
  }
  newIndex[count] = next;

  std::vector<IrInstruction> out;
  out.reserve(next);
  for (size_t i = 0; i < count; ++i) {
    if (const Rotation *rotation = rotationAt[i]; rotation != nullptr) {
      const uint32_t debugId = function.instructions[i].debugId;
      IrInstruction first = function.instructions[rotation->header];
      IrInstruction second = function.instructions[rotation->header + 1];
      IrInstruction compare = function.instructions[rotation->header + 2];
      compare.op = rotation->inverted;
      IrInstruction loop;
      loop.op = IrOpcode::JumpIfZero;
      loop.imm = newIndex[rotation->body];
      for (IrInstruction *instruction : {&first, &second, &compare, &loop}) {
        instruction->debugId = debugId;
        out.push_back(*instruction);
      }
      continue;
    }
    IrInstruction instruction = function.instructions[i];
    if (isJump(instruction.op) && instruction.imm <= count) {
      instruction.imm = newIndex[static_cast<size_t>(instruction.imm)];
    }
    out.push_back(instruction);
  }
  function.instructions = std::move(out);
  return true;
}

} // namespace

bool runLoopRotatePass(IrModule &module, const IrPassContext &, bool &changed, std::string &) {
  changed = false;
  for (IrFunction &function : module.functions) {
    changed |= rotateLoops(function);
  }
  return true;
}

} // namespace primec::ir_opt
