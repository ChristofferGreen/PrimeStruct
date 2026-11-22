#pragma once

#include "WasmEmitterInternal.h"

namespace primec {

// The function type of a helper.
WasmFunctionType wasmHeapHelperType(WasmHeapHelper helper);

// The code of a helper. The runtime context must have its function indexes and region addresses set.
WasmCodeBody buildWasmHeapHelperBody(WasmHeapHelper helper, const WasmRuntimeContext &runtime);

} // namespace primec
