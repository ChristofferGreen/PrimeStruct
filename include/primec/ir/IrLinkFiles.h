#pragma once

#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

// Reads and deserializes one serialized IR module (`.psir`).
bool loadIrModuleFile(const std::string &path, IrModule &out, std::string &error);

// The `primevm <program> --link <module>...` link: merges each module of `paths`, in order, into `module` with
// linkIrModule, then requires every import to be defined and validates the result for the VM. `module` is unchanged
// when it fails. Errors name the file that failed to load or link.
bool linkIrModuleFiles(IrModule &module, const std::vector<std::string> &paths, std::string &error);

} // namespace primec
