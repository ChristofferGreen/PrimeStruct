#pragma once

#include <cstdint>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec::testing {

// The arm64 machine words the native emitter writes for one IR instruction's template, from an
// empty buffer. Lets x86_64 hosts pin encodings they cannot execute. Supported opcodes:
// SextI32 and the F32/F64 comparisons; any other opcode yields no words.
std::vector<uint32_t> arm64TemplateWords(IrOpcode op);

} // namespace primec::testing
