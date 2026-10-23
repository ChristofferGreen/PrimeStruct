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

} // namespace primec::testing
