#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

class WasmEmitter {
public:
  // The browser profile imports nothing from WASI: a runtime error ends the program with a wasm trap (a
  // `WebAssembly.RuntimeError` in the host); the WASI profile calls `proc_exit(3)` first.
  bool browserProfile = false;

  bool emitModule(const IrModule &module, std::vector<uint8_t> &out, std::string &error) const;
};

} // namespace primec
