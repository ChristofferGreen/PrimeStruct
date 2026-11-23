#include "WasmEmitterHeapRegions.h"

namespace primec {
namespace wasm_heap {

uint32_t fn(const WasmRuntimeContext &runtime, WasmHeapHelper helper) {
  return runtime.heapFunction[static_cast<size_t>(helper)];
}

void declareLocals(WasmCodeBody &body, uint32_t i32Count, uint32_t i64Count) {
  uint32_t groups = (i32Count != 0 ? 1u : 0u) + (i64Count != 0 ? 1u : 0u);
  appendU32Leb(groups, body.localDecls);
  if (i32Count != 0) {
    appendU32Leb(i32Count, body.localDecls);
    body.localDecls.push_back(WasmTypeI32);
  }
  if (i64Count != 0) {
    appendU32Leb(i64Count, body.localDecls);
    body.localDecls.push_back(WasmTypeI64);
  }
}

// local = memory address of the entry `index * stride` of a region: `index << shift`.
void emitScaled(WasmAsm &a, uint32_t indexLocal, int32_t shift) {
  a.localGet(indexLocal);
  a.i32Const(shift);
  a.i32Shl();
}

// Adds the current base address of a heap region (a global) to the address on the stack. The regions move when the
// heap grows, so no helper keeps an address across a call that may grow it.
void regionAdd(WasmAsm &a, uint32_t baseGlobal) {
  a.globalGet(baseGlobal);
  a.i32Add();
}

// removeRun(position i32): drops the run, shifting the later ones down.
WasmCodeBody buildRemoveRun(const WasmRuntimeContext &) {
  constexpr uint32_t Position = 0;
  WasmCodeBody body;
  declareLocals(body, 0, 0);
  WasmAsm a(body.instructions);
  // memory.copy(runs + position * 8, runs + (position + 1) * 8, (count - position - 1) * 8)
  emitScaled(a, Position, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.localGet(Position);
  a.i32Const(1);
  a.i32Add();
  a.i32Const(3);
  a.i32Shl();
  regionAdd(a, WasmGlobalRunsBase);
  a.globalGet(WasmGlobalRunCount);
  a.localGet(Position);
  a.i32Sub();
  a.i32Const(1);
  a.i32Sub();
  a.i32Const(3);
  a.i32Shl();
  a.memoryCopy();
  a.globalGet(WasmGlobalRunCount);
  a.i32Const(1);
  a.i32Sub();
  a.globalSet(WasmGlobalRunCount);
  a.end();
  return body;
}

// insertRun(position i32, base i32, length i32): puts the run at `position`, shifting the later ones up.
WasmCodeBody buildInsertRun(const WasmRuntimeContext &) {
  constexpr uint32_t Position = 0;
  constexpr uint32_t Base = 1;
  constexpr uint32_t Length = 2;
  WasmCodeBody body;
  declareLocals(body, 0, 0);
  WasmAsm a(body.instructions);
  // memory.copy(runs + (position + 1) * 8, runs + position * 8, (count - position) * 8)
  a.localGet(Position);
  a.i32Const(1);
  a.i32Add();
  a.i32Const(3);
  a.i32Shl();
  regionAdd(a, WasmGlobalRunsBase);
  emitScaled(a, Position, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.globalGet(WasmGlobalRunCount);
  a.localGet(Position);
  a.i32Sub();
  a.i32Const(3);
  a.i32Shl();
  a.memoryCopy();
  emitScaled(a, Position, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.localGet(Base);
  a.i32Store(0);
  emitScaled(a, Position, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.localGet(Length);
  a.i32Store(4);
  a.globalGet(WasmGlobalRunCount);
  a.i32Const(1);
  a.i32Add();
  a.globalSet(WasmGlobalRunCount);
  a.end();
  return body;
}

// takeFront(position i32, count i32): removes the first `count` slots of the run (all of it when it is that long).
WasmCodeBody buildTakeFront(const WasmRuntimeContext &runtime) {
  const uint32_t removeRun = fn(runtime, WasmHeapHelper::RemoveRun);
  constexpr uint32_t Position = 0;
  constexpr uint32_t Count = 1;
  constexpr uint32_t Entry = 2;
  constexpr uint32_t Length = 3;
  WasmCodeBody body;
  declareLocals(body, 2, 0);
  WasmAsm a(body.instructions);
  emitScaled(a, Position, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.localSet(Entry);
  a.localGet(Entry);
  a.i32Load(4);
  a.localSet(Length);
  a.localGet(Length);
  a.localGet(Count);
  a.i32GtU();
  a.ifThen();
  a.localGet(Entry);
  a.localGet(Entry);
  a.i32Load(0);
  a.localGet(Count);
  a.i32Add();
  a.i32Store(0);
  a.localGet(Entry);
  a.localGet(Length);
  a.localGet(Count);
  a.i32Sub();
  a.i32Store(4);
  a.elseArm();
  a.localGet(Position);
  a.call(removeRun);
  a.end();
  a.end();
  return body;
}

// ensureCapacity(needed i64) -> i32: 1 when the regions have room for `needed` slots, growing them first when they do
// not; 0 when `needed` is past `WasmHeapMaxSlots` or the host refuses the memory. Growing moves all four regions to
// the end of the memory (`memory.grow`) with room for twice the slots (or exactly what is needed when the host refuses
// that), copies what the heap has handed out, and makes the heap the last region, so the shadow stack moves the next
// time it needs room. Slots, states and counts are addressed by index, so nothing else changes.
WasmCodeBody buildEnsureCapacity(const WasmRuntimeContext &) {
  constexpr uint32_t Needed = 0;
  constexpr uint32_t Capacity = 1;
  constexpr uint32_t NewCapacity = 2;
  constexpr uint32_t Pages = 3;
  constexpr uint32_t Old = 4;
  constexpr uint32_t NewBase = 5;
  constexpr uint32_t NewStates = 6;
  constexpr uint32_t NewCounts = 7;
  constexpr uint32_t NewRuns = 8;
  constexpr uint32_t Needed32 = 9;
  WasmCodeBody body;
  declareLocals(body, 9, 0);
  WasmAsm a(body.instructions);
  a.localGet(Needed);
  a.i64Const(WasmHeapMaxSlots);
  a.i64GtU();
  a.ifThen();
  a.i32Const(0);
  a.ret();
  a.end();
  a.globalGet(WasmGlobalHeapCapacity);
  a.localSet(Capacity);
  a.localGet(Needed);
  a.localGet(Capacity);
  a.i64ExtendI32U();
  a.i64GtU();
  a.i32Eqz();
  a.ifThen();
  a.i32Const(1);
  a.ret();
  a.end();
  a.localGet(Needed);
  a.i32WrapI64();
  a.localSet(Needed32);
  // newCapacity = min(max(needed, 2 * capacity), max)
  a.localGet(Capacity);
  a.i32Const(1);
  a.i32Shl();
  a.localSet(NewCapacity);
  a.localGet(NewCapacity);
  a.localGet(Needed32);
  a.localGet(NewCapacity);
  a.localGet(Needed32);
  a.i32GtU();
  a.select();
  a.localSet(NewCapacity);
  a.localGet(NewCapacity);
  a.i32Const(static_cast<int32_t>(WasmHeapMaxSlots));
  a.localGet(NewCapacity);
  a.i32Const(static_cast<int32_t>(WasmHeapMaxSlots));
  a.i32LtU();
  a.select();
  a.localSet(NewCapacity);
  a.block();
  a.loop();
  // pages = (regionBytes(newCapacity) + 65535) / 65536; old = memory.grow(pages)
  a.localGet(NewCapacity);
  a.i64ExtendI32U();
  a.i64Const(18);
  a.i64Mul();
  a.i64Const(8 + 65535);
  a.i64Add();
  a.i64Const(16);
  a.i64ShrU();
  a.i32WrapI64();
  a.localSet(Pages);
  a.localGet(Pages);
  a.memoryGrow();
  a.localSet(Old);
  a.localGet(Old);
  a.i32Const(-1);
  a.i32Ne();
  a.brIf(1);
  // refused: try again for exactly what is needed, and give up when that was it
  a.localGet(NewCapacity);
  a.localGet(Needed32);
  a.i32Eq();
  a.ifThen();
  a.i32Const(0);
  a.ret();
  a.end();
  a.localGet(Needed32);
  a.localSet(NewCapacity);
  a.br(0);
  a.end();
  a.end();
  // the new regions: values, states, counts, runs, from the first byte of the new pages
  a.localGet(Old);
  a.i32Const(16);
  a.i32Shl();
  a.localSet(NewBase);
  a.localGet(NewBase);
  a.localGet(NewCapacity);
  a.i32Const(3);
  a.i32Shl();
  a.i32Add();
  a.localSet(NewStates);
  a.localGet(NewStates);
  a.localGet(NewCapacity);
  a.i32Const(1);
  a.i32Shl();
  a.i32Add();
  a.localSet(NewCounts);
  a.localGet(NewCounts);
  a.localGet(NewCapacity);
  a.i32Const(2);
  a.i32Shl();
  a.i32Add();
  a.localSet(NewRuns);
  // copy what the heap has handed out (and the free runs), then switch the bases
  a.localGet(NewBase);
  a.globalGet(WasmGlobalValuesBase);
  a.globalGet(WasmGlobalHeapSize);
  a.i32Const(3);
  a.i32Shl();
  a.memoryCopy();
  a.localGet(NewStates);
  a.globalGet(WasmGlobalStatesBase);
  a.globalGet(WasmGlobalHeapSize);
  a.i32Const(1);
  a.i32Shl();
  a.memoryCopy();
  a.localGet(NewCounts);
  a.globalGet(WasmGlobalCountsBase);
  a.globalGet(WasmGlobalHeapSize);
  a.i32Const(2);
  a.i32Shl();
  a.memoryCopy();
  a.localGet(NewRuns);
  a.globalGet(WasmGlobalRunsBase);
  a.globalGet(WasmGlobalRunCount);
  a.i32Const(3);
  a.i32Shl();
  a.memoryCopy();
  a.localGet(NewBase);
  a.globalSet(WasmGlobalValuesBase);
  a.localGet(NewStates);
  a.globalSet(WasmGlobalStatesBase);
  a.localGet(NewCounts);
  a.globalSet(WasmGlobalCountsBase);
  a.localGet(NewRuns);
  a.globalSet(WasmGlobalRunsBase);
  a.localGet(NewCapacity);
  a.globalSet(WasmGlobalHeapCapacity);
  a.i32Const(0);
  a.globalSet(WasmGlobalStackLast);
  a.i32Const(1);
  a.end();
  return body;
}

} // namespace wasm_heap
} // namespace primec
