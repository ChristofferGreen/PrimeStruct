#pragma once

#include "WasmEmitterAsm.h"
#include "WasmEmitterInternal.h"

namespace primec {

// The building blocks of the slot heap helpers (WasmEmitterHeap.cpp) and the helpers over the heap's regions: the free
// run table and the growth of the regions (WasmEmitterHeapRegions.cpp).
namespace wasm_heap {

// The function index of a heap helper.
uint32_t fn(const WasmRuntimeContext &runtime, WasmHeapHelper helper);

// Declares the locals after the parameters: `i32Count` i32 locals, then `i64Count` i64 locals.
void declareLocals(WasmCodeBody &body, uint32_t i32Count, uint32_t i64Count);

// local = memory address of the entry `index * stride` of a region: `index << shift`.
void emitScaled(WasmAsm &a, uint32_t indexLocal, int32_t shift);

// Adds the current base address of a heap region (a global) to the address on the stack. The regions move when the
// heap grows, so no helper keeps an address across a call that may grow it.
void regionAdd(WasmAsm &a, uint32_t baseGlobal);

// A table of free runs: (first unit, length) pairs of two u32, sorted by first unit, between a base address and a
// count, both mutable globals. The slot heap counts slots, the byte regions granules.
struct RunTable {
  uint32_t baseGlobal;
  uint32_t countGlobal;
};
inline constexpr RunTable SlotRuns{WasmGlobalRunsBase, WasmGlobalRunCount};
inline constexpr RunTable ByteRuns{WasmGlobalByteRunsBase, WasmGlobalByteRunCount};

// removeRun(position i32): drops the free run, shifting the later ones down.
WasmCodeBody buildRemoveRun(const WasmRuntimeContext &runtime, const RunTable &table);
// insertRun(position i32, base i32, length i32): puts the free run at `position`, shifting the later ones up.
WasmCodeBody buildInsertRun(const WasmRuntimeContext &runtime, const RunTable &table);
// takeFront(position i32, count i32): removes the first `count` units of the free run (all of it when it is that long);
// `removeRun` is the function index of the table's removeRun.
WasmCodeBody
buildTakeFront(const WasmRuntimeContext &runtime, const RunTable &table, uint32_t removeRun);
// ensureCapacity(needed i64) -> i32: 1 when the regions have room for `needed` slots (growing them first when they do
// not), 0 when `needed` is past `WasmHeapMaxSlots` or the host refuses the memory.
WasmCodeBody buildEnsureCapacity(const WasmRuntimeContext &runtime);

} // namespace wasm_heap

} // namespace primec
