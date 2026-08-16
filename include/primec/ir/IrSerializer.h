#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

bool serializeIr(const IrModule &module, std::vector<uint8_t> &out, std::string &error);
bool deserializeIr(const std::vector<uint8_t> &data, IrModule &out, std::string &error);

} // namespace primec
