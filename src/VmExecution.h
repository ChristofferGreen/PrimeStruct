#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "primec/Ir.h"

namespace primec::vm_detail {

bool executeVmModule(const IrModule &module,
                     uint64_t &result,
                     std::string &error,
                     uint64_t argCount,
                     const std::vector<std::string_view> *args);

} // namespace primec::vm_detail
