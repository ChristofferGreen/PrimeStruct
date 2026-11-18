#pragma once

#include <string>

#include "primec/ir/Ir.h"

namespace primec::ir_lowerer {

// `--emit=lib` lowers a stub entry together with the public functions of the library. Removes the entry
// function from `module`, renumbers the function operands of the rest and sets `entryIndex` to -1. Fails when a
// function still calls the entry.
bool removeLibraryEntryStub(IrModule &module, std::string &error);

} // namespace primec::ir_lowerer
