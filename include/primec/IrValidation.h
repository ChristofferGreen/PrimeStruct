#pragma once

#include <string>

#include "primec/Ir.h"

namespace primec {

enum class IrValidationTarget {
  Any,
  Vm,
  Native,
};

bool validateIrModule(const IrModule &module, IrValidationTarget target, std::string &error);

inline bool validateIrModule(const IrModule &module, std::string &error) {
  return validateIrModule(module, IrValidationTarget::Any, error);
}

} // namespace primec
