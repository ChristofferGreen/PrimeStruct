#pragma once

#include "WasmEmitterAsm.h"
#include "WasmEmitterInternal.h"

namespace primec {

// The signature and the body of a string heap helper (WasmEmitterTextHeap.cpp).
WasmFunctionType wasmTextHelperType(WasmTextHelper helper);
WasmCodeBody buildWasmTextHelperBody(WasmTextHelper helper, const WasmRuntimeContext &runtime);

// The values of the string heap globals (`WasmGlobalTextMetaBump` on) a module starts with, in the order of their
// indexes.
std::vector<uint32_t> wasmTextGlobalInitials(const WasmRuntimeContext &runtime);

} // namespace primec
