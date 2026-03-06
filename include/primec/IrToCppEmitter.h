#pragma once

#include "primec/Ir.h"

#include <string>

namespace primec {

class IrToCppEmitter {
public:
  bool emitSource(const IrModule &module, std::string &out, std::string &error) const;
};

} // namespace primec
