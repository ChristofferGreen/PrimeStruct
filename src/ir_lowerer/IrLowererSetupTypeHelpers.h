#pragma once

#include <string>

#include "IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name);

} // namespace primec::ir_lowerer
