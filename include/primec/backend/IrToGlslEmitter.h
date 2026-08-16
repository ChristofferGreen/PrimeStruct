#pragma once

#include "primec/ir/Ir.h"

#include <string>

namespace primec {

class IrToGlslEmitter {
public:
  bool emitSource(const IrModule &module, std::string &out, std::string &error) const;
};

} // namespace primec
