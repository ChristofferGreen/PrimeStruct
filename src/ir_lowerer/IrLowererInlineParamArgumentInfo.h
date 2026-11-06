#pragma once

#include "primec/ir_lowerer/IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

// How an inlined call parameter takes facts from its argument's local info (collection handles, pointer targets).
bool isDirectSequentialHandle(const LocalInfo &info);
bool sequentialHandleKindsCompatible(const LocalInfo &paramInfo, const LocalInfo &argInfo);
void preserveSequentialHandleArgumentInfo(LocalInfo &paramInfo, const LocalInfo &argInfo);
void preservePointerTargetArgumentInfo(LocalInfo &paramInfo, const LocalInfo &argInfo);

} // namespace primec::ir_lowerer
