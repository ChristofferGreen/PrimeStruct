#pragma once

#include <string>

#include "primec/Ir.h"

namespace primec {

class Vm {
public:
  bool execute(const IrModule &module, uint64_t &result, std::string &error) const;
};

} // namespace primec
