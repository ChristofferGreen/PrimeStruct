#pragma once

#include <string>

#include "primec/Ir.h"

namespace primec {

// Applies a conservative IR inlining pass that replaces eligible call sites
// with callee bodies. The pass is intentionally narrow to preserve runtime
// behavior and determinism.
bool inlineIrModuleCalls(IrModule &module, std::string &error);

} // namespace primec
