#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

class WasmEmitter {
public:
  bool emitModule(const IrModule &module, std::vector<uint8_t> &out, std::string &error) const;
};

} // namespace primec
