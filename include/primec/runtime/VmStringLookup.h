#pragma once

#include <cstdint>
#include <string>

#include "primec/ir/Ir.h"

namespace primec::vm_detail {

// The one place VM code turns a string index into text: an index into the module's string table.
// Faults with a diagnostic when the index names no string. (Run-time text is the stdlib `String`,
// which lives in the string heap of VmTextHeap.h and is not addressed by a string index.)
inline bool resolveVmString(const IrModule &module,
                            uint64_t index,
                            const std::string *&textOut,
                            std::string &error) {
  if (index >= module.stringTable.size()) {
    error = "invalid string index in IR";
    return false;
  }
  textOut = &module.stringTable[static_cast<size_t>(index)];
  return true;
}

} // namespace primec::vm_detail
