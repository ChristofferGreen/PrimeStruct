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
  // The C++ emitters (cpp, exe, optcpp, optexe): like Any, but host calls are allowed
  // and become direct calls of `extern "C"` functions the link step supplies.
  Cpp,
  // The optimized C++ emitters (optcpp, optexe): like Cpp, and they also run the u8 byte opcodes and
  // pass text across the host boundary.
  OptCpp,
};

// Whether the back end behind `target` runs FunctionAddress, CallIndirect and CallIndirectVoid itself. The others
// take the linked module through expandIndirectCalls. The VM, the optimized C++ emitters, serialized bytecode and
// the C++ emitters, Wasm and arm64 native (a build for an arm64 Mac) run them.
bool irTargetRunsIndirectCalls(IrValidationTarget target);

// A module that still has an import (primec/ir/IrLinker.h) is not runnable, and is refused unless `allowImports`: a
// library archive keeps the imports of the libraries it was compiled against.
bool validateIrModule(const IrModule &module,
                      IrValidationTarget target,
                      std::string &error,
                      bool allowImports = false);

inline bool validateIrModule(const IrModule &module, std::string &error) {
  return validateIrModule(module, IrValidationTarget::Any, error);
}

} // namespace primec
