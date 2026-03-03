#pragma once

#include "IrLowererSharedTypes.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

LocalInfo::ValueKind normalizeIndexKind(LocalInfo::ValueKind kind);
bool isSupportedIndexKind(LocalInfo::ValueKind kind);

IrOpcode pushZeroForIndex(LocalInfo::ValueKind kind);
IrOpcode cmpLtForIndex(LocalInfo::ValueKind kind);
IrOpcode cmpGeForIndex(LocalInfo::ValueKind kind);
IrOpcode pushOneForIndex(LocalInfo::ValueKind kind);
IrOpcode addForIndex(LocalInfo::ValueKind kind);
IrOpcode mulForIndex(LocalInfo::ValueKind kind);

} // namespace primec::ir_lowerer
