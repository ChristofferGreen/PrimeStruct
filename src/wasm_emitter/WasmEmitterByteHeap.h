#pragma once

#include "WasmEmitterAsm.h"
#include "WasmEmitterInternal.h"

namespace primec {

// The signature and the body of a byte region helper (WasmEmitterByteHeap.cpp).
WasmFunctionType wasmByteHelperType(WasmByteHelper helper);
WasmCodeBody buildWasmByteHelperBody(WasmByteHelper helper, const WasmRuntimeContext &runtime);

// Pushes 1 when the i64 local holds a byte-region address (bit 63, the tag, and bit 47, `ByteRegionBit`), else 0: the
// VM's `isByteAddress`.
void emitWasmIsByteAddress(WasmAsm &a, uint32_t addressLocal);

} // namespace primec
