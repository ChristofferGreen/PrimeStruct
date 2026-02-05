#pragma once

#include <cstdint>
#include <string>

#include "primec/Ir.h"

namespace primec {

class Vm {
public:
  bool execute(const IrModule &module, uint64_t &result, std::string &error, uint64_t argCount = 0) const;
};

} // namespace primec
