#pragma once

#include <string>

#include "primec/ir/Ir.h"

namespace primec {

enum class IrValidationTarget {
  Any,
  // Serialized PSIR (`--emit=ir`): backend-neutral like Any, but host calls are
  // allowed because the bytecode is meant to be loaded by a VM embedder.
  Serialized,
  Vm,
  Native,
  Glsl,
  Wasm,
  WasmBrowser,
};

bool validateIrModule(const IrModule &module, IrValidationTarget target, std::string &error);

inline bool validateIrModule(const IrModule &module, std::string &error) {
  return validateIrModule(module, IrValidationTarget::Any, error);
}

} // namespace primec
