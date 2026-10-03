#pragma once

#include "primec/ir/Ir.h"

#include "test_ir_optimizer_helpers.h"

#include <bit>
#include <cstdint>
#include <random>
#include <vector>

// Seeded programs of valid, balanced stack IR (assignments, the dup/store/pop
// assignment idiom, discarded expressions, if/else, counted loops, integer and
// float expressions). The generator avoids operations that fault (division uses
// a nonzero constant) so every program terminates normally and prints its
// scratch locals. Shared by the optimizer and optexe differential tests.
namespace optimizer_test {

using primec::IrInstruction;
using primec::IrOpcode;

class Generator {
public:
  explicit Generator(uint64_t seed) : rng_(seed) {
  }

  std::vector<IrInstruction> program() {
    // Locals 0..5 are scratch values, 6.. are loop counters (one per depth).
    for (uint64_t slot = 0; slot < ScratchLocals; ++slot) {
      emit(IrOpcode::PushI64, pick(5) == 0 ? rng_() : static_cast<uint64_t>(pick(7)) - 3);
      emit(IrOpcode::StoreLocal, slot);
    }
    block(0, 3 + pick(4));
    for (uint64_t slot = 0; slot < ScratchLocals; ++slot) {
      emit(IrOpcode::LoadLocal, slot);
      emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
    }
    emit(IrOpcode::PushI32, 0);
    emit(IrOpcode::ReturnI32);
    return std::move(code_);
  }

  size_t loops = 0;
  size_t branches = 0;

private:
  static constexpr uint64_t ScratchLocals = 6;
  static constexpr uint64_t MaxDepth = 3;

  uint64_t pick(uint64_t bound) {
    return rng_() % bound;
  }

  size_t emit(IrOpcode op, uint64_t imm = 0) {
    code_.push_back({op, imm, 0});
    return code_.size() - 1;
  }

  void constant() {
    switch (pick(6)) {
    case 0:
      emit(IrOpcode::PushI32,
           static_cast<uint64_t>(static_cast<int64_t>(static_cast<int>(pick(9)) - 4)));
      break;
    case 1:
      emit(IrOpcode::PushI64, 0);
      break;
    case 2:
      emit(IrOpcode::PushI64, 1);
      break;
    case 3:
      emit(IrOpcode::PushI64, rng_());
      break;
    default:
      emit(IrOpcode::PushI32, pick(100));
      break;
    }
  }

  // Pushes exactly one 64-bit integer value.
  void intExpr(int depth) {
    const uint64_t choice = depth >= 3 ? pick(2) : pick(12);
    switch (choice) {
    case 0:
      constant();
      return;
    case 1:
      emit(IrOpcode::LoadLocal, pick(ScratchLocals));
      return;
    case 2:
    case 3:
    case 4: {
      intExpr(depth + 1);
      intExpr(depth + 1);
      static const IrOpcode ops[] = {IrOpcode::AddI64, IrOpcode::SubI64, IrOpcode::MulI64};
      emit(ops[choice - 2]);
      return;
    }
    case 5: {
      intExpr(depth + 1);
      intExpr(depth + 1);
      static const IrOpcode cmps[] = {IrOpcode::CmpLtI64,
                                      IrOpcode::CmpGtI64,
                                      IrOpcode::CmpEqI64,
                                      IrOpcode::CmpNeI64,
                                      IrOpcode::CmpLeI64,
                                      IrOpcode::CmpGeU64,
                                      IrOpcode::CmpLtU64};
      emit(cmps[pick(7)]);
      return;
    }
    case 6:
      intExpr(depth + 1);
      emit(pick(2) == 0 ? IrOpcode::NegI64 : IrOpcode::NegI32);
      return;
    case 7: {
      intExpr(depth + 1);
      emit(IrOpcode::PushI64, 1 + pick(7)); // nonzero divisor
      emit(pick(2) == 0 ? IrOpcode::DivI64 : IrOpcode::DivU64);
      return;
    }
    case 8:
      // x + 0, x * 1, x / 1 and x - 0 shapes the peephole pass rewrites.
      intExpr(depth + 1);
      emit(IrOpcode::PushI64, pick(2) == 0 ? 0 : 1);
      emit(pick(2) == 0 ? IrOpcode::AddI64 : IrOpcode::MulI64);
      return;
    case 9: {
      // A float round trip: int -> f64 -> arithmetic -> int.
      intExpr(depth + 1);
      emit(IrOpcode::ConvertI64ToF64);
      emit(IrOpcode::PushF64, std::bit_cast<uint64_t>(1.5 + static_cast<double>(pick(8))));
      emit(pick(2) == 0 ? IrOpcode::AddF64 : IrOpcode::MulF64);
      emit(IrOpcode::PushF64, std::bit_cast<uint64_t>(1000000.0));
      emit(IrOpcode::CmpLtF64);
      return;
    }
    case 10:
      // A constant sub-expression for the folder.
      emit(IrOpcode::PushI64, pick(50));
      emit(IrOpcode::PushI64, pick(50));
      emit(pick(2) == 0 ? IrOpcode::AddI64 : IrOpcode::MulI64);
      return;
    default:
      intExpr(depth + 1);
      emit(IrOpcode::Dup);
      emit(IrOpcode::AddI64);
      return;
    }
  }

  void statement(uint64_t depth) {
    const uint64_t kind = depth >= MaxDepth ? pick(5) : pick(8);
    switch (kind) {
    case 0:
      intExpr(0);
      emit(IrOpcode::StoreLocal, pick(ScratchLocals));
      return;
    case 1: // assignment statement idiom
      intExpr(0);
      emit(IrOpcode::Dup);
      emit(IrOpcode::StoreLocal, pick(ScratchLocals));
      emit(IrOpcode::Pop);
      return;
    case 2: // evaluated and discarded
      intExpr(0);
      emit(IrOpcode::Pop);
      return;
    case 3:
      intExpr(0);
      emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
      return;
    case 4: // store then use
      intExpr(0);
      emit(IrOpcode::StoreLocal, pick(ScratchLocals));
      emit(IrOpcode::LoadLocal, pick(ScratchLocals));
      emit(IrOpcode::Pop);
      return;
    case 5:
    case 6: { // if / else
      ++branches;
      intExpr(0);
      const size_t toElse = emit(IrOpcode::JumpIfZero);
      block(depth + 1, 1 + pick(3));
      const size_t toEnd = emit(IrOpcode::Jump);
      code_[toElse].imm = code_.size();
      if (pick(3) != 0) {
        block(depth + 1, 1 + pick(3));
      }
      code_[toEnd].imm = code_.size();
      return;
    }
    default: { // counted loop with its own counter
      ++loops;
      const uint64_t counter = ScratchLocals + depth;
      emit(IrOpcode::PushI32, 1 + pick(4));
      emit(IrOpcode::StoreLocal, counter);
      const size_t head = code_.size();
      emit(IrOpcode::LoadLocal, counter);
      const size_t exit = emit(IrOpcode::JumpIfZero);
      block(depth + 1, 1 + pick(3));
      emit(IrOpcode::LoadLocal, counter);
      emit(IrOpcode::PushI32, 1);
      emit(IrOpcode::SubI32);
      emit(IrOpcode::StoreLocal, counter);
      emit(IrOpcode::Jump, head);
      code_[exit].imm = code_.size();
      return;
    }
    }
  }

  void block(uint64_t depth, uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
      statement(depth);
    }
  }

  std::mt19937_64 rng_;
  std::vector<IrInstruction> code_;
};

inline primec::IrModule
makeModule(uint64_t seed, size_t *loops = nullptr, size_t *branches = nullptr) {
  Generator generator(seed);
  primec::IrModule module = optimizer_test::moduleOf(generator.program());
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  module.functions[0].metadata.capabilityMask = primec::EffectIoOut;
  if (loops != nullptr) {
    *loops = generator.loops;
  }
  if (branches != nullptr) {
    *branches = generator.branches;
  }
  return module;
}

} // namespace optimizer_test
