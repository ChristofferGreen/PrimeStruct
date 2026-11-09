#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec::testing {

// A tiny arm64 Mach-O executable (macOS only) that proves the image can import and keep state: it calls
// the libSystem function `symbol` (`int f(int)`) with -5 through a pointer slot dyld binds at load,
// stores the result in the image's writable data, reads it back and returns it as the exit status
// (`abs` gives 5). False, with `error`, off macOS/arm64 or when the image cannot be built.
bool buildMachOImportProbe(const std::string &symbol, std::vector<uint8_t> &image, std::string &error);

// The arm64 machine words the native emitter writes for one IR instruction's template, from an
// empty buffer. Lets x86_64 hosts pin encodings they cannot execute. Supported opcodes:
// SextI32 and the F32/F64 comparisons; any other opcode yields no words.
std::vector<uint32_t> arm64TemplateWords(IrOpcode op);

// The arm64 words that build the 64-bit constant `value` in x`rd` (MOVZ, MOVN, MOVK, ORR).
std::vector<uint32_t> arm64ConstantWords(uint8_t rd, uint64_t value);

// The arm64 words that move the stack pointer by `amount` bytes, up (`add`) or down.
std::vector<uint32_t> arm64StackAdjustWords(uint64_t amount, bool add);

// The arm64 words of `op` taking the pushed `constant` as its immediate operand, from an empty
// buffer with the value-stack cache off. False when the constant stays a register operand.
bool arm64ImmediateOperandWords(IrOpcode op, uint64_t constant, std::vector<uint32_t> &words);

// One step of an arm64 value-stack sequence: push or pop a register, or take a label (the word
// index a branch could target) at that point.
struct Arm64ValueStackStep {
  enum class Kind { Push, Pop, Label };
  Kind kind = Kind::Push;
  uint8_t reg = 0;
};

// The arm64 words the native emitter writes for `steps` with the value-stack cache off (the form
// register-allocated functions run templates in): a push followed by its pop becomes a move unless
// a label or a write of the pushed register comes between them.
std::vector<uint32_t> arm64ValueStackWords(const std::vector<Arm64ValueStackStep> &steps);

} // namespace primec::testing
