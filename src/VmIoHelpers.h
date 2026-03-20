#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "primec/Vm.h"

namespace primec::vm_detail {

bool handlePrintOpcode(const IrModule &module,
                       const IrInstruction &inst,
                       std::vector<uint64_t> &stack,
                       const std::vector<std::string_view> *args,
                       std::string &error);

bool handleFileOpcode(const IrModule &module,
                      const IrInstruction &inst,
                      std::vector<uint64_t> &stack,
                      std::vector<uint64_t> &locals,
                      std::string &error);

} // namespace primec::vm_detail
