#include "WasmEmitterHeap.h"

#include "WasmEmitterAsm.h"
#include "WasmEmitterByteHeap.h"
#include "WasmEmitterHeapRegions.h"

namespace primec {

// The slot heap of a Wasm module: `HeapAlloc`, `HeapFree` and `HeapRealloc` with the VM's heap addresses and faults
// (docs/spec/vm-design.md, Heap). It is a port of `VmHeapCore` (include/primec/runtime/VmHeapCoreBody.inc), the
// single-arena heap the VM and the C++ runtimes share, so a program sees the same addresses, the same reuse of freed
// memory and the same fault texts on every backend.
//
// The state lives in four regions of the linear memory (WasmEmitterRuntimeContext.cpp), whose base addresses are
// mutable globals because the heap relocates them when it grows (`ensureCapacity`):
//   values  8 bytes per slot, the raw 64 bits of a VM slot;
//   states  2 bytes per slot, `LiveBit | generation` while an allocation holds the slot and the generation it was
//           last allocated with once freed (zero if it never was): an address is live when `states[slot]` equals
//           `address >> 48`, exactly as in the VM;
//   counts  4 bytes per slot, the slot count of the live allocation that starts there (0 elsewhere);
//   runs    the free runs, sorted by first slot and coalesced, as (first slot, length) pairs of two u32.
// Two mutable globals hold the number of slots the heap has handed out so far (`slots.size()` in the VM; slots
// never leave it) and the number of free runs. Every slot below the first is either live or inside a free run, so
// the runs are the whole free list: the best fit scans them in address order and keeps the first of the shortest
// that fits, which is the VM's "smallest run, lowest base among equals". The heap has a fixed capacity of
// `WasmHeapMaxSlots` (about 4 GiB of memory); an allocation past it, or one the host's memory refuses, ends with the
// VM's `VM heap allocation overflow` fault.

namespace {

using namespace wasm_heap;

constexpr int64_t OffsetMask48 = (int64_t{1} << 48) - 1;
constexpr uint32_t LiveBit = 0x8000;
constexpr uint32_t GenerationMask = 0x7fff;
constexpr uint32_t StderrFd = 2;
constexpr int32_t RuntimeErrorExitCode = 3;

// Fault kinds of the `fault` helper.
constexpr int32_t FaultFree = 0;
constexpr int32_t FaultRealloc = 1;
constexpr int32_t FaultOverflow = 2;
constexpr int32_t FaultByte = 3;

// The address a slot range starts at: Tag | generation << 48 | slot * 16 (the VM's encoding, arena 0).
void emitAddress(WasmAsm &a, uint32_t generationLocal, uint32_t baseLocal) {
  a.localGet(generationLocal);
  a.i32Const(static_cast<int32_t>(LiveBit));
  a.i32Or();
  a.i64ExtendI32U();
  a.i64Const(48);
  a.i64Shl();
  a.localGet(baseLocal);
  a.i64ExtendI32U();
  a.i64Const(4);
  a.i64Shl();
  a.i64Or();
}

// The fault: the VM's message (and the address for the two address faults) on stderr, `proc_exit(3)` on the WASI
// profile, a trap on the browser profile. (kind i32, value i64) -> ().
WasmCodeBody buildFault(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Kind = 0;
  constexpr uint32_t Value = 1;
  constexpr uint32_t Fd = 2;
  constexpr uint32_t Digits = 3;
  constexpr uint32_t Rem = 4;
  constexpr uint32_t Ptr = 5;
  constexpr uint32_t Len = 6;
  constexpr uint32_t Neg = 7;
  WasmCodeBody body;
  // fd (i32); digits, rem (i64); ptr, len, neg (i32): the shapes `emitWasiWriteDecimalFromI64Local` wants.
  appendU32Leb(3, body.localDecls);
  appendU32Leb(1, body.localDecls);
  body.localDecls.push_back(WasmTypeI32);
  appendU32Leb(2, body.localDecls);
  body.localDecls.push_back(WasmTypeI64);
  appendU32Leb(3, body.localDecls);
  body.localDecls.push_back(WasmTypeI32);
  std::vector<uint8_t> &out = body.instructions;
  WasmAsm a(out);
  if (!runtime.hasProcExit) {
    a.unreachable();
    a.end();
    return body;
  }
  a.i32Const(static_cast<int32_t>(StderrFd));
  a.localSet(Fd);
  const auto literalIf = [&](int32_t kind, uint32_t address, uint32_t length) {
    a.localGet(Kind);
    a.i32Const(kind);
    a.i32Eq();
    a.ifThen();
    emitWasiWriteLiteralFromFdLocal(Fd, address, length, runtime, out);
    a.drop();
    a.end();
  };
  literalIf(FaultFree, runtime.heapFreeMessageAddr, runtime.heapFreeMessageLength);
  literalIf(FaultRealloc, runtime.heapReallocMessageAddr, runtime.heapReallocMessageLength);
  literalIf(FaultOverflow, runtime.heapOverflowMessageAddr, runtime.heapOverflowMessageLength);
  if (runtime.hasBytes) {
    literalIf(FaultByte, runtime.byteMessageAddr, runtime.byteMessageLength);
  }
  a.localGet(Kind);
  a.i32Const(FaultOverflow);
  a.i32Ne();
  a.ifThen();
  a.localGet(Value);
  a.localSet(Digits);
  emitWasiWriteDecimalFromI64Local(Fd, Digits, Rem, Ptr, Len, Neg, false, runtime, out);
  a.drop();
  emitWasiWriteLiteralFromFdLocal(Fd, runtime.newlineAddr, 1, runtime, out);
  a.drop();
  a.end();
  a.i32Const(RuntimeErrorExitCode);
  a.call(runtime.procExitImportIndex);
  a.unreachable();
  a.end();
  return body;
}

// find(address i64) -> i32: the first slot of the live allocation `address` names, or -1 when it names none (not a
// heap address, not 16-byte aligned, beyond the heap, not the start of an allocation, or from an earlier generation
// of the slot).
WasmCodeBody buildFind(const WasmRuntimeContext &) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Index = 1;
  WasmCodeBody body;
  declareLocals(body, 0, 1);
  WasmAsm a(body.instructions);
  const auto returnMissing = [&]() {
    a.ifThen();
    a.i32Const(-1);
    a.ret();
    a.end();
  };
  a.localGet(Address);
  a.i64Const(63);
  a.i64ShrU();
  a.i64Eqz();
  returnMissing();
  a.localGet(Address);
  a.i64Const(15);
  a.i64And();
  a.i64Const(0);
  a.i64Ne();
  returnMissing();
  a.localGet(Address);
  a.i64Const(OffsetMask48);
  a.i64And();
  a.i64Const(4);
  a.i64ShrU();
  a.localSet(Index);
  a.localGet(Index);
  a.globalGet(WasmGlobalHeapSize);
  a.i64ExtendI32U();
  a.i64GeU();
  returnMissing();
  a.localGet(Index);
  a.i32WrapI64();
  a.i32Const(2);
  a.i32Shl();
  regionAdd(a, WasmGlobalCountsBase);
  a.i32Load(0);
  a.i32Eqz();
  returnMissing();
  a.localGet(Index);
  a.i32WrapI64();
  a.i32Const(1);
  a.i32Shl();
  regionAdd(a, WasmGlobalStatesBase);
  a.i32Load16U(0);
  a.localGet(Address);
  a.i64Const(48);
  a.i64ShrU();
  a.i32WrapI64();
  a.i32Const(0xffff);
  a.i32And();
  a.i32Ne();
  returnMissing();
  a.localGet(Index);
  a.i32WrapI64();
  a.end();
  return body;
}

// newGeneration(base i32, count i32) -> i32: a generation no earlier allocation of these slots had (until it wraps).
WasmCodeBody buildNewGeneration(const WasmRuntimeContext &) {
  constexpr uint32_t Base = 0;
  constexpr uint32_t Count = 1;
  constexpr uint32_t Index = 2;
  constexpr uint32_t Max = 3;
  constexpr uint32_t Stop = 4;
  constexpr uint32_t Current = 5;
  WasmCodeBody body;
  declareLocals(body, 4, 0);
  WasmAsm a(body.instructions);
  a.localGet(Base);
  a.localSet(Index);
  a.localGet(Base);
  a.localGet(Count);
  a.i32Add();
  a.localSet(Stop);
  a.block();
  a.loop();
  a.localGet(Index);
  a.localGet(Stop);
  a.i32GeU();
  a.brIf(1);
  emitScaled(a, Index, 1);
  regionAdd(a, WasmGlobalStatesBase);
  a.i32Load16U(0);
  a.i32Const(static_cast<int32_t>(GenerationMask));
  a.i32And();
  a.localSet(Current);
  // max = current > max ? current : max
  a.localGet(Current);
  a.localGet(Max);
  a.localGet(Current);
  a.localGet(Max);
  a.i32GtU();
  a.select();
  a.localSet(Max);
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  a.localGet(Max);
  a.i32Const(1);
  a.i32Add();
  a.i32Const(static_cast<int32_t>(GenerationMask));
  a.i32And();
  a.end();
  return body;
}

// fillStates(base i32, count i32, state i32): states[base, base + count) = state.
WasmCodeBody buildFillStates(const WasmRuntimeContext &) {
  constexpr uint32_t Base = 0;
  constexpr uint32_t Count = 1;
  constexpr uint32_t State = 2;
  constexpr uint32_t Index = 3;
  constexpr uint32_t Stop = 4;
  WasmCodeBody body;
  declareLocals(body, 2, 0);
  WasmAsm a(body.instructions);
  a.localGet(Base);
  a.localSet(Index);
  a.localGet(Base);
  a.localGet(Count);
  a.i32Add();
  a.localSet(Stop);
  a.block();
  a.loop();
  a.localGet(Index);
  a.localGet(Stop);
  a.i32GeU();
  a.brIf(1);
  emitScaled(a, Index, 1);
  regionAdd(a, WasmGlobalStatesBase);
  a.localGet(State);
  a.i32Store16(0);
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  a.end();
  return body;
}

// release(base i32): frees the live allocation that starts at `base`: its values are zeroed, its slots keep the
// generation, and the freed run merges with the free runs on both sides.
WasmCodeBody buildRelease(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Base = 0;
  constexpr uint32_t Count = 1;
  constexpr uint32_t Generation = 2;
  constexpr uint32_t Index = 3;
  constexpr uint32_t Stop = 4;
  constexpr uint32_t Position = 5;
  constexpr uint32_t Length = 6;
  constexpr uint32_t Entry = 7;
  constexpr uint32_t Merged = 8;
  WasmCodeBody body;
  declareLocals(body, 8, 0);
  WasmAsm a(body.instructions);
  emitScaled(a, Base, 2);
  regionAdd(a, WasmGlobalCountsBase);
  a.i32Load(0);
  a.localSet(Count);
  emitScaled(a, Base, 1);
  regionAdd(a, WasmGlobalStatesBase);
  a.i32Load16U(0);
  a.i32Const(static_cast<int32_t>(GenerationMask));
  a.i32And();
  a.localSet(Generation);
  a.localGet(Base);
  a.localSet(Index);
  a.localGet(Base);
  a.localGet(Count);
  a.i32Add();
  a.localSet(Stop);
  a.block();
  a.loop();
  a.localGet(Index);
  a.localGet(Stop);
  a.i32GeU();
  a.brIf(1);
  emitScaled(a, Index, 3);
  regionAdd(a, WasmGlobalValuesBase);
  a.i64Const(0);
  a.i64Store(0);
  emitScaled(a, Index, 1);
  regionAdd(a, WasmGlobalStatesBase);
  a.localGet(Generation);
  a.i32Store16(0);
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  emitScaled(a, Base, 2);
  regionAdd(a, WasmGlobalCountsBase);
  a.i32Const(0);
  a.i32Store(0);
  a.globalGet(WasmGlobalLiveBlocks);
  a.i32Const(1);
  a.i32Sub();
  a.globalSet(WasmGlobalLiveBlocks);

  // position = the first run that starts after the allocation
  a.i32Const(0);
  a.localSet(Position);
  a.block();
  a.loop();
  a.localGet(Position);
  a.globalGet(WasmGlobalRunCount);
  a.i32GeU();
  a.brIf(1);
  emitScaled(a, Position, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.i32Load(0);
  a.localGet(Base);
  a.i32GtU();
  a.brIf(1);
  a.i32Increment(Position);
  a.br(0);
  a.end();
  a.end();
  a.localGet(Count);
  a.localSet(Length);
  // the run that starts where the allocation ends joins it
  a.localGet(Position);
  a.globalGet(WasmGlobalRunCount);
  a.i32LtU();
  a.ifThen();
  emitScaled(a, Position, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.i32Load(0);
  a.localGet(Base);
  a.localGet(Count);
  a.i32Add();
  a.i32Eq();
  a.ifThen();
  emitScaled(a, Position, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.i32Load(4);
  a.localGet(Length);
  a.i32Add();
  a.localSet(Length);
  a.localGet(Position);
  a.call(fn(runtime, WasmHeapHelper::RemoveRun));
  a.end();
  a.end();
  // the run that ends where the allocation starts takes it in
  a.i32Const(0);
  a.localSet(Merged);
  a.localGet(Position);
  a.ifThen();
  a.localGet(Position);
  a.i32Const(1);
  a.i32Sub();
  a.i32Const(3);
  a.i32Shl();
  regionAdd(a, WasmGlobalRunsBase);
  a.localSet(Entry);
  a.localGet(Entry);
  a.i32Load(0);
  a.localGet(Entry);
  a.i32Load(4);
  a.i32Add();
  a.localGet(Base);
  a.i32Eq();
  a.ifThen();
  a.localGet(Entry);
  a.localGet(Entry);
  a.i32Load(4);
  a.localGet(Length);
  a.i32Add();
  a.i32Store(4);
  a.i32Const(1);
  a.localSet(Merged);
  a.end();
  a.end();
  a.localGet(Merged);
  a.i32Eqz();
  a.ifThen();
  a.localGet(Position);
  a.localGet(Base);
  a.localGet(Length);
  a.call(fn(runtime, WasmHeapHelper::InsertRun));
  a.end();
  a.end();
  return body;
}

// alloc(count i64) -> i64: `count` zeroed slots from the smallest free run that fits, else the free run at the end of
// the heap extended, else new slots; address 0 for a count of 0.
WasmCodeBody buildAlloc(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Count = 0;
  constexpr uint32_t Best = 1;
  constexpr uint32_t BestLength = 2;
  constexpr uint32_t Index = 3;
  constexpr uint32_t Length = 4;
  constexpr uint32_t Base = 5;
  constexpr uint32_t Reused = 6;
  constexpr uint32_t Generation = 7;
  constexpr uint32_t TailRun = 8;
  constexpr uint32_t Count32 = 9;
  WasmCodeBody body;
  declareLocals(body, 9, 0);
  WasmAsm a(body.instructions);
  a.localGet(Count);
  a.i64Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.i32Const(-1);
  a.localSet(Best);
  // the smallest run that fits; the first of equal lengths is the lowest base
  a.block();
  a.loop();
  a.localGet(Index);
  a.globalGet(WasmGlobalRunCount);
  a.i32GeU();
  a.brIf(1);
  emitScaled(a, Index, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.i32Load(4);
  a.localSet(Length);
  a.localGet(Length);
  a.i64ExtendI32U();
  a.localGet(Count);
  a.i64GeU();
  a.ifThen();
  a.localGet(Best);
  a.i32Const(-1);
  a.i32Eq();
  a.localGet(Length);
  a.localGet(BestLength);
  a.i32LtU();
  a.i32Or();
  a.ifThen();
  a.localGet(Index);
  a.localSet(Best);
  a.localGet(Length);
  a.localSet(BestLength);
  a.end();
  a.end();
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  a.localGet(Best);
  a.i32Const(-1);
  a.i32Ne();
  a.ifThen();
  // a free run: its front
  a.localGet(Count);
  a.i32WrapI64();
  a.localSet(Count32);
  emitScaled(a, Best, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.i32Load(0);
  a.localSet(Base);
  a.localGet(Best);
  a.localGet(Count32);
  a.call(fn(runtime, WasmHeapHelper::TakeFront));
  a.i32Const(1);
  a.localSet(Reused);
  a.elseArm();
  // the end of the heap, taking the free run that reaches it
  a.globalGet(WasmGlobalHeapSize);
  a.localSet(Base);
  a.i32Const(-1);
  a.localSet(TailRun);
  a.globalGet(WasmGlobalRunCount);
  a.ifThen();
  a.globalGet(WasmGlobalRunCount);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Index);
  emitScaled(a, Index, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.i32Load(0);
  emitScaled(a, Index, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.i32Load(4);
  a.i32Add();
  a.globalGet(WasmGlobalHeapSize);
  a.i32Eq();
  a.ifThen();
  emitScaled(a, Index, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.i32Load(0);
  a.localSet(Base);
  a.localGet(Index);
  a.localSet(TailRun);
  a.end();
  a.end();
  // more than the heap can hold, or a host that refuses the memory, is the VM's overflow fault
  a.localGet(Count);
  a.i64Const(WasmHeapMaxSlots);
  a.i64GtU();
  a.ifThen();
  a.i32Const(FaultOverflow);
  a.i64Const(0);
  a.call(fn(runtime, WasmHeapHelper::Fault));
  a.end();
  a.localGet(Count);
  a.i32WrapI64();
  a.localSet(Count32);
  a.localGet(Base);
  a.i64ExtendI32U();
  a.localGet(Count);
  a.i64Add();
  a.call(fn(runtime, WasmHeapHelper::EnsureCapacity));
  a.i32Eqz();
  a.ifThen();
  a.i32Const(FaultOverflow);
  a.i64Const(0);
  a.call(fn(runtime, WasmHeapHelper::Fault));
  a.end();
  a.localGet(TailRun);
  a.i32Const(-1);
  a.i32Ne();
  a.ifThen();
  a.localGet(TailRun);
  a.call(fn(runtime, WasmHeapHelper::RemoveRun));
  a.i32Const(1);
  a.localSet(Reused);
  a.end();
  a.localGet(Base);
  a.localGet(Count32);
  a.i32Add();
  a.globalSet(WasmGlobalHeapSize);
  a.end();
  a.localGet(Reused);
  a.ifThen();
  a.localGet(Base);
  a.localGet(Count32);
  a.call(fn(runtime, WasmHeapHelper::NewGeneration));
  a.localSet(Generation);
  a.end();
  a.localGet(Base);
  a.localGet(Count32);
  a.localGet(Generation);
  a.i32Const(static_cast<int32_t>(LiveBit));
  a.i32Or();
  a.call(fn(runtime, WasmHeapHelper::FillStates));
  emitScaled(a, Base, 2);
  regionAdd(a, WasmGlobalCountsBase);
  a.localGet(Count32);
  a.i32Store(0);
  a.globalGet(WasmGlobalLiveBlocks);
  a.i32Const(1);
  a.i32Add();
  a.globalSet(WasmGlobalLiveBlocks);
  emitAddress(a, Generation, Base);
  a.end();
  return body;
}

// growInPlace(base i32, oldCount i32, count i64) -> i64: grows the live allocation to `count` slots where it is when
// the slots after it are free (a free run long enough, or one that reaches the end of the heap, which then grows),
// and returns its new address (every slot takes a generation none of them had, so the old address faults); 0 when it
// cannot.
WasmCodeBody buildGrowInPlace(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Base = 0;
  constexpr uint32_t Old = 1;
  constexpr uint32_t Count = 2;
  constexpr uint32_t End = 3;
  constexpr uint32_t Extra = 4;
  constexpr uint32_t RunLength = 5;
  constexpr uint32_t Run = 6;
  constexpr uint32_t Index = 7;
  constexpr uint32_t Count32 = 8;
  constexpr uint32_t Generation = 9;
  WasmCodeBody body;
  declareLocals(body, 7, 0);
  WasmAsm a(body.instructions);
  const auto returnZero = [&]() {
    a.ifThen();
    a.i64Const(0);
    a.ret();
    a.end();
  };
  a.localGet(Count);
  a.i64Const(WasmHeapMaxSlots);
  a.i64GtU();
  returnZero();
  a.localGet(Count);
  a.i32WrapI64();
  a.localSet(Count32);
  a.localGet(Base);
  a.i64ExtendI32U();
  a.localGet(Count);
  a.i64Add();
  a.call(fn(runtime, WasmHeapHelper::EnsureCapacity));
  a.i32Eqz();
  returnZero();
  a.localGet(Base);
  a.localGet(Old);
  a.i32Add();
  a.localSet(End);
  a.localGet(Count32);
  a.localGet(Old);
  a.i32Sub();
  a.localSet(Extra);
  a.i32Const(-1);
  a.localSet(Run);
  a.block();
  a.loop();
  a.localGet(Index);
  a.globalGet(WasmGlobalRunCount);
  a.i32GeU();
  a.brIf(1);
  emitScaled(a, Index, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.i32Load(0);
  a.localGet(End);
  a.i32Eq();
  a.ifThen();
  a.localGet(Index);
  a.localSet(Run);
  emitScaled(a, Index, 3);
  regionAdd(a, WasmGlobalRunsBase);
  a.i32Load(4);
  a.localSet(RunLength);
  a.br(2);
  a.end();
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  // too short a run, and not one that reaches the end of the heap: the allocation has to move
  a.localGet(RunLength);
  a.localGet(Extra);
  a.i32LtU();
  a.localGet(End);
  a.localGet(RunLength);
  a.i32Add();
  a.globalGet(WasmGlobalHeapSize);
  a.i32Ne();
  a.i32And();
  returnZero();
  a.localGet(RunLength);
  a.ifThen();
  a.localGet(Run);
  // min(runLength, extra)
  a.localGet(RunLength);
  a.localGet(Extra);
  a.localGet(RunLength);
  a.localGet(Extra);
  a.i32LtU();
  a.select();
  a.call(fn(runtime, WasmHeapHelper::TakeFront));
  a.end();
  a.localGet(Base);
  a.localGet(Count32);
  a.i32Add();
  a.globalGet(WasmGlobalHeapSize);
  a.i32GtU();
  a.ifThen();
  a.localGet(Base);
  a.localGet(Count32);
  a.i32Add();
  a.globalSet(WasmGlobalHeapSize);
  a.end();
  a.localGet(Base);
  a.localGet(Count32);
  a.call(fn(runtime, WasmHeapHelper::NewGeneration));
  a.localSet(Generation);
  a.localGet(Base);
  a.localGet(Count32);
  a.localGet(Generation);
  a.i32Const(static_cast<int32_t>(LiveBit));
  a.i32Or();
  a.call(fn(runtime, WasmHeapHelper::FillStates));
  emitScaled(a, Base, 2);
  regionAdd(a, WasmGlobalCountsBase);
  a.localGet(Count32);
  a.i32Store(0);
  emitAddress(a, Generation, Base);
  a.end();
  return body;
}

// free(address i64): the VM's `HeapFree`; address 0 frees nothing.
WasmCodeBody buildFree(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Base = 1;
  WasmCodeBody body;
  declareLocals(body, 1, 0);
  WasmAsm a(body.instructions);
  a.localGet(Address);
  a.i64Eqz();
  a.ifThen();
  a.ret();
  a.end();
  if (runtime.hasBytes) {
    emitWasmIsByteAddress(a, Address);
    a.ifThen();
    a.localGet(Address);
    a.call(runtime.byteFunction[static_cast<size_t>(WasmByteHelper::Free)]);
    a.ret();
    a.end();
  }
  a.localGet(Address);
  a.call(fn(runtime, WasmHeapHelper::Find));
  a.localSet(Base);
  a.localGet(Base);
  a.i32Const(-1);
  a.i32Eq();
  a.ifThen();
  a.i32Const(FaultFree);
  a.localGet(Address);
  a.call(fn(runtime, WasmHeapHelper::Fault));
  a.end();
  a.localGet(Base);
  a.call(fn(runtime, WasmHeapHelper::Release));
  a.end();
  return body;
}

// realloc(address i64, count i64) -> i64: the VM's `HeapRealloc`: address 0 allocates, a count of 0 frees, a growth
// that fits where the allocation is keeps its values in place, anything else moves them to a new allocation and
// frees the old one.
WasmCodeBody buildRealloc(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Count = 1;
  constexpr uint32_t Base = 2;
  constexpr uint32_t Old = 3;
  constexpr uint32_t NewBase = 4;
  constexpr uint32_t Copied = 5;
  constexpr uint32_t NewAddress = 6;
  WasmCodeBody body;
  declareLocals(body, 4, 1);
  WasmAsm a(body.instructions);
  a.localGet(Address);
  a.i64Eqz();
  a.ifThen();
  a.localGet(Count);
  a.call(fn(runtime, WasmHeapHelper::Alloc));
  a.ret();
  a.end();
  a.localGet(Count);
  a.i64Eqz();
  a.ifThen();
  a.localGet(Address);
  a.call(fn(runtime, WasmHeapHelper::Free));
  a.i64Const(0);
  a.ret();
  a.end();
  if (runtime.hasBytes) {
    emitWasmIsByteAddress(a, Address);
    a.ifThen();
    a.localGet(Address);
    a.localGet(Count);
    a.call(runtime.byteFunction[static_cast<size_t>(WasmByteHelper::Realloc)]);
    a.ret();
    a.end();
  }
  a.localGet(Address);
  a.call(fn(runtime, WasmHeapHelper::Find));
  a.localSet(Base);
  a.localGet(Base);
  a.i32Const(-1);
  a.i32Eq();
  a.ifThen();
  a.i32Const(FaultRealloc);
  a.localGet(Address);
  a.call(fn(runtime, WasmHeapHelper::Fault));
  a.end();
  emitScaled(a, Base, 2);
  regionAdd(a, WasmGlobalCountsBase);
  a.i32Load(0);
  a.localSet(Old);
  a.localGet(Count);
  a.localGet(Old);
  a.i64ExtendI32U();
  a.i64GtU();
  a.ifThen();
  a.localGet(Base);
  a.localGet(Old);
  a.localGet(Count);
  a.call(fn(runtime, WasmHeapHelper::GrowInPlace));
  a.localTee(NewAddress);
  a.i64Const(0);
  a.i64Ne();
  a.ifThen();
  a.localGet(NewAddress);
  a.ret();
  a.end();
  a.end();
  a.localGet(Count);
  a.call(fn(runtime, WasmHeapHelper::Alloc));
  a.localSet(NewAddress);
  a.localGet(NewAddress);
  a.i64Const(OffsetMask48);
  a.i64And();
  a.i64Const(4);
  a.i64ShrU();
  a.i32WrapI64();
  a.localSet(NewBase);
  // copied = min(old, count)
  a.localGet(Old);
  a.localGet(Count);
  a.i32WrapI64();
  a.localGet(Old);
  a.localGet(Count);
  a.i32WrapI64();
  a.i32LtU();
  a.select();
  a.localSet(Copied);
  emitScaled(a, NewBase, 3);
  regionAdd(a, WasmGlobalValuesBase);
  emitScaled(a, Base, 3);
  regionAdd(a, WasmGlobalValuesBase);
  a.localGet(Copied);
  a.i32Const(3);
  a.i32Shl();
  a.memoryCopy();
  a.localGet(Base);
  a.call(fn(runtime, WasmHeapHelper::Release));
  a.localGet(NewAddress);
  a.end();
  return body;
}

} // namespace

WasmFunctionType wasmHeapHelperType(WasmHeapHelper helper) {
  WasmFunctionType type;
  switch (helper) {
  case WasmHeapHelper::Fault:
    type.params = {WasmTypeI32, WasmTypeI64};
    break;
  case WasmHeapHelper::Find:
    type.params = {WasmTypeI64};
    type.results = {WasmTypeI32};
    break;
  case WasmHeapHelper::NewGeneration:
    type.params = {WasmTypeI32, WasmTypeI32};
    type.results = {WasmTypeI32};
    break;
  case WasmHeapHelper::FillStates:
    type.params = {WasmTypeI32, WasmTypeI32, WasmTypeI32};
    break;
  case WasmHeapHelper::RemoveRun:
  case WasmHeapHelper::Release:
    type.params = {WasmTypeI32};
    break;
  case WasmHeapHelper::InsertRun:
    type.params = {WasmTypeI32, WasmTypeI32, WasmTypeI32};
    break;
  case WasmHeapHelper::TakeFront:
    type.params = {WasmTypeI32, WasmTypeI32};
    break;
  case WasmHeapHelper::EnsureCapacity:
    type.params = {WasmTypeI64};
    type.results = {WasmTypeI32};
    break;
  case WasmHeapHelper::Alloc:
    type.params = {WasmTypeI64};
    type.results = {WasmTypeI64};
    break;
  case WasmHeapHelper::GrowInPlace:
    type.params = {WasmTypeI32, WasmTypeI32, WasmTypeI64};
    type.results = {WasmTypeI64};
    break;
  case WasmHeapHelper::Realloc:
    type.params = {WasmTypeI64, WasmTypeI64};
    type.results = {WasmTypeI64};
    break;
  case WasmHeapHelper::Free:
    type.params = {WasmTypeI64};
    break;
  }
  return type;
}

WasmCodeBody buildWasmHeapHelperBody(WasmHeapHelper helper, const WasmRuntimeContext &runtime) {
  switch (helper) {
  case WasmHeapHelper::Fault:
    return buildFault(runtime);
  case WasmHeapHelper::Find:
    return buildFind(runtime);
  case WasmHeapHelper::NewGeneration:
    return buildNewGeneration(runtime);
  case WasmHeapHelper::FillStates:
    return buildFillStates(runtime);
  case WasmHeapHelper::RemoveRun:
    return buildRemoveRun(runtime, SlotRuns);
  case WasmHeapHelper::InsertRun:
    return buildInsertRun(runtime, SlotRuns);
  case WasmHeapHelper::TakeFront:
    return buildTakeFront(runtime, SlotRuns, fn(runtime, WasmHeapHelper::RemoveRun));
  case WasmHeapHelper::Release:
    return buildRelease(runtime);
  case WasmHeapHelper::EnsureCapacity:
    return buildEnsureCapacity(runtime);
  case WasmHeapHelper::Alloc:
    return buildAlloc(runtime);
  case WasmHeapHelper::GrowInPlace:
    return buildGrowInPlace(runtime);
  case WasmHeapHelper::Realloc:
    return buildRealloc(runtime);
  case WasmHeapHelper::Free:
    return buildFree(runtime);
  }
  return WasmCodeBody{};
}

} // namespace primec
