#pragma once

#include <string>

#include "primec/Ir.h"

namespace primec {

class NativeEmitter {
 public:
  bool emitExecutable(const IrModule &module, const std::string &outputPath, std::string &error) const;
};

} // namespace primec
