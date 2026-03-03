#pragma once

#include <string>

#include "IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name);
LocalInfo::ValueKind combineNumericKinds(LocalInfo::ValueKind left, LocalInfo::ValueKind right);

} // namespace primec::ir_lowerer
