#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "primec/Ir.h"

namespace primec {

class Vm {
public:
  bool execute(const IrModule &module, uint64_t &result, std::string &error, uint64_t argCount = 0) const;
  bool execute(const IrModule &module,
               uint64_t &result,
               std::string &error,
               const std::vector<std::string_view> &args) const;
};

} // namespace primec
