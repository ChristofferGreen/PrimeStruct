#include "WasmEmitterByteHeap.h"

#include "WasmEmitterHeapRegions.h"

namespace primec {

// The byte regions of a Wasm module: `HeapAllocBytes` and the byte-region case of `HeapFree`, `HeapRealloc`, `LoadU8` and
// `StoreU8`, with the VM's byte addresses and faults (include/primec/runtime/VmHeapCoreBody.inc, "byte regions").
//
// The VM's address is `Tag | ByteRegionBit | generation << 48 | offset`: one running generation counter (`(g + 1) &
// 0x7fff` per allocation), a region occupying `(count + 7) & ~7` bytes at a multiple of 8, free runs reused best fit
// (the smallest that fits, the lowest base among equals) and coalesced, the free run that reaches the end of the storage
// taken when none fits, a freed region zeroed, no growth in place (a realloc allocates, copies and releases). This port
// counts in 8-byte granules and keeps, next to the free run table, one owner entry for every granule (the first granule
// of the live region that holds it plus one) instead of the VM's sorted map of live regions: an access is one index into
// the owner table and two compares, and an allocation or free writes the owners of its own granules, never shifting a
// table. A byte is valid when its granule has an owner, the owner's generation is the address's and the offset lies
// within the owner's byte count (the footprint's padding belongs to no region). The regions live in the linear memory and
// move to its end when the granules outgrow them (`ensure`), so no helper keeps a memory address across a call that may
// allocate.

namespace {

using namespace wasm_heap;

constexpr int64_t OffsetMask47 = (int64_t{1} << 47) - 1;
constexpr uint32_t GenerationMask = 0x7fff;
constexpr int64_t AddressTag = static_cast<int64_t>(0x8000800000000000ull); // Tag | ByteRegionBit

// Fault kinds of the slot heap's `fault` helper (WasmEmitterHeap.cpp).
constexpr int32_t FaultFree = 0;
constexpr int32_t FaultRealloc = 1;
constexpr int32_t FaultOverflow = 2;
constexpr int32_t FaultByte = 3;

uint32_t byteFn(const WasmRuntimeContext &runtime, WasmByteHelper helper) {
  return runtime.byteFunction[static_cast<size_t>(helper)];
}

// Pushes the memory address of entry `indexLocal` of a table with `entryBytes` bytes per entry (a shift of 1, 2 or 3).
void entryAddress(WasmAsm &a, uint32_t indexLocal, int32_t shift, uint32_t baseGlobal) {
  emitScaled(a, indexLocal, shift);
  regionAdd(a, baseGlobal);
}

void emitFault(WasmAsm &a, const WasmRuntimeContext &runtime, int32_t kind) {
  if (runtime.hasText && kind == FaultOverflow) {
    // `__pstext_try_alloc` answers a region it cannot make with 0.
    a.globalGet(WasmGlobalTextTry);
    a.ifThen();
    a.i64Const(0);
    a.ret();
    a.end();
  }
  a.i32Const(kind);
  a.i64Const(0);
  a.call(fn(runtime, WasmHeapHelper::Fault));
}

// ensure(needed i64) -> i32: 1 when the regions have room for `needed` granules (growing them first when they do not),
// 0 when `needed` is past `WasmByteMaxGranules` or the host refuses the memory. Growing moves the five regions to the
// end of the memory (`memory.grow`) with room for twice the granules (or exactly what is needed when the host refuses
// that), copies what has been handed out and makes the byte storage the last region, so the shadow stack moves the next
// time it needs room.
WasmCodeBody buildEnsure(const WasmRuntimeContext &) {
  constexpr uint32_t Needed = 0;
  constexpr uint32_t Capacity = 1;
  constexpr uint32_t NewCapacity = 2;
  constexpr uint32_t Pages = 3;
  constexpr uint32_t Old = 4;
  constexpr uint32_t NewBase = 5;
  constexpr uint32_t NewOwner = 6;
  constexpr uint32_t NewCounts = 7;
  constexpr uint32_t NewGens = 8;
  constexpr uint32_t NewRuns = 9;
  constexpr uint32_t Needed32 = 10;
  WasmCodeBody body;
  declareLocals(body, 10, 0);
  WasmAsm a(body.instructions);
  a.localGet(Needed);
  a.i64Const(WasmByteMaxGranules);
  a.i64GtU();
  a.ifThen();
  a.i32Const(0);
  a.ret();
  a.end();
  a.globalGet(WasmGlobalByteCapacity);
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
  a.i32Const(static_cast<int32_t>(WasmByteMaxGranules));
  a.localGet(NewCapacity);
  a.i32Const(static_cast<int32_t>(WasmByteMaxGranules));
  a.i32LtU();
  a.select();
  a.localSet(NewCapacity);
  a.block();
  a.loop();
  // pages = (regionBytes(newCapacity) + 65535) / 65536; old = memory.grow(pages)
  a.localGet(NewCapacity);
  a.i64ExtendI32U();
  a.i64Const(22);
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
  // the new regions from the first byte of the new pages: bytes, owners, counts, generations, runs
  a.localGet(Old);
  a.i32Const(16);
  a.i32Shl();
  a.localSet(NewBase);
  a.localGet(NewBase);
  a.localGet(NewCapacity);
  a.i32Const(3);
  a.i32Shl();
  a.i32Add();
  a.localSet(NewOwner);
  a.localGet(NewOwner);
  a.localGet(NewCapacity);
  a.i32Const(2);
  a.i32Shl();
  a.i32Add();
  a.localSet(NewCounts);
  a.localGet(NewCounts);
  a.localGet(NewCapacity);
  a.i32Const(2);
  a.i32Shl();
  a.i32Add();
  a.localSet(NewGens);
  a.localGet(NewGens);
  a.localGet(NewCapacity);
  a.i32Const(1);
  a.i32Shl();
  a.i32Add();
  a.localSet(NewRuns);
  const auto copyRegion =
      [&](uint32_t newBase, uint32_t oldGlobal, uint32_t countGlobal, int32_t shift) {
        a.localGet(newBase);
        a.globalGet(oldGlobal);
        a.globalGet(countGlobal);
        a.i32Const(shift);
        a.i32Shl();
        a.memoryCopy();
      };
  copyRegion(NewBase, WasmGlobalByteBase, WasmGlobalByteSize, 3);
  copyRegion(NewOwner, WasmGlobalByteOwnerBase, WasmGlobalByteSize, 2);
  copyRegion(NewCounts, WasmGlobalByteCountsBase, WasmGlobalByteSize, 2);
  copyRegion(NewGens, WasmGlobalByteGensBase, WasmGlobalByteSize, 1);
  copyRegion(NewRuns, WasmGlobalByteRunsBase, WasmGlobalByteRunCount, 3);
  a.localGet(NewBase);
  a.globalSet(WasmGlobalByteBase);
  a.localGet(NewOwner);
  a.globalSet(WasmGlobalByteOwnerBase);
  a.localGet(NewCounts);
  a.globalSet(WasmGlobalByteCountsBase);
  a.localGet(NewGens);
  a.globalSet(WasmGlobalByteGensBase);
  a.localGet(NewRuns);
  a.globalSet(WasmGlobalByteRunsBase);
  a.localGet(NewCapacity);
  a.globalSet(WasmGlobalByteCapacity);
  a.i32Const(0);
  a.globalSet(WasmGlobalStackLast);
  a.i32Const(1);
  a.end();
  return body;
}

// release(start i32): frees the live region that starts at granule `start`: its bytes are zeroed, its granules lose
// their owner, and the freed run merges with the free runs on both sides.
WasmCodeBody buildRelease(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Start = 0;
  constexpr uint32_t Count = 1;
  constexpr uint32_t Footprint = 2;
  constexpr uint32_t Index = 3;
  constexpr uint32_t Stop = 4;
  constexpr uint32_t Position = 5;
  constexpr uint32_t Length = 6;
  constexpr uint32_t Entry = 7;
  constexpr uint32_t Merged = 8;
  WasmCodeBody body;
  declareLocals(body, 8, 0);
  WasmAsm a(body.instructions);
  entryAddress(a, Start, 2, WasmGlobalByteCountsBase);
  a.i32Load(0);
  a.localSet(Count);
  a.localGet(Count);
  a.i32Const(7);
  a.i32Add();
  a.i32Const(3);
  a.i32ShrU();
  a.localSet(Footprint);
  // memory.fill(bytes + start * 8, 0, count)
  entryAddress(a, Start, 3, WasmGlobalByteBase);
  a.i32Const(0);
  a.localGet(Count);
  a.memoryFill();
  a.localGet(Start);
  a.localSet(Index);
  a.localGet(Start);
  a.localGet(Footprint);
  a.i32Add();
  a.localSet(Stop);
  a.block();
  a.loop();
  a.localGet(Index);
  a.localGet(Stop);
  a.i32GeU();
  a.brIf(1);
  entryAddress(a, Index, 2, WasmGlobalByteOwnerBase);
  a.i32Const(0);
  a.i32Store(0);
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  entryAddress(a, Start, 2, WasmGlobalByteCountsBase);
  a.i32Const(0);
  a.i32Store(0);

  // position = the first run that starts after the region
  a.i32Const(0);
  a.localSet(Position);
  a.block();
  a.loop();
  a.localGet(Position);
  a.globalGet(WasmGlobalByteRunCount);
  a.i32GeU();
  a.brIf(1);
  entryAddress(a, Position, 3, WasmGlobalByteRunsBase);
  a.i32Load(0);
  a.localGet(Start);
  a.i32GtU();
  a.brIf(1);
  a.i32Increment(Position);
  a.br(0);
  a.end();
  a.end();
  a.localGet(Footprint);
  a.localSet(Length);
  // the run that starts where the region ends joins it
  a.localGet(Position);
  a.globalGet(WasmGlobalByteRunCount);
  a.i32LtU();
  a.ifThen();
  entryAddress(a, Position, 3, WasmGlobalByteRunsBase);
  a.i32Load(0);
  a.localGet(Start);
  a.localGet(Footprint);
  a.i32Add();
  a.i32Eq();
  a.ifThen();
  entryAddress(a, Position, 3, WasmGlobalByteRunsBase);
  a.i32Load(4);
  a.localGet(Length);
  a.i32Add();
  a.localSet(Length);
  a.localGet(Position);
  a.call(byteFn(runtime, WasmByteHelper::RemoveRun));
  a.end();
  a.end();
  // the run that ends where the region starts takes it in
  a.i32Const(0);
  a.localSet(Merged);
  a.localGet(Position);
  a.ifThen();
  a.localGet(Position);
  a.i32Const(1);
  a.i32Sub();
  a.i32Const(3);
  a.i32Shl();
  regionAdd(a, WasmGlobalByteRunsBase);
  a.localSet(Entry);
  a.localGet(Entry);
  a.i32Load(0);
  a.localGet(Entry);
  a.i32Load(4);
  a.i32Add();
  a.localGet(Start);
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
  a.localGet(Start);
  a.localGet(Length);
  a.call(byteFn(runtime, WasmByteHelper::InsertRun));
  a.end();
  a.end();
  return body;
}

// findStart(address i64) -> i32: the first granule of the live region that starts exactly at the byte-region address,
// or -1 when the address names none (not 8-byte aligned, beyond the storage, inside a region or free, or from an
// earlier generation).
WasmCodeBody buildFindStart(const WasmRuntimeContext &) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Index = 1;
  constexpr uint32_t Offset = 2;
  WasmCodeBody body;
  declareLocals(body, 1, 1);
  WasmAsm a(body.instructions);
  const auto returnMissing = [&]() {
    a.ifThen();
    a.i32Const(-1);
    a.ret();
    a.end();
  };
  a.localGet(Address);
  a.i64Const(OffsetMask47);
  a.i64And();
  a.localSet(Offset);
  a.localGet(Offset);
  a.i64Const(7);
  a.i64And();
  a.i64Const(0);
  a.i64Ne();
  returnMissing();
  a.localGet(Offset);
  a.i64Const(3);
  a.i64ShrU();
  a.localSet(Offset);
  a.localGet(Offset);
  a.globalGet(WasmGlobalByteSize);
  a.i64ExtendI32U();
  a.i64GeU();
  returnMissing();
  a.localGet(Offset);
  a.i32WrapI64();
  a.localSet(Index);
  entryAddress(a, Index, 2, WasmGlobalByteOwnerBase);
  a.i32Load(0);
  a.localGet(Index);
  a.i32Const(1);
  a.i32Add();
  a.i32Ne();
  returnMissing();
  entryAddress(a, Index, 1, WasmGlobalByteGensBase);
  a.i32Load16U(0);
  a.localGet(Address);
  a.i64Const(48);
  a.i64ShrU();
  a.i32WrapI64();
  a.i32Const(static_cast<int32_t>(GenerationMask));
  a.i32And();
  a.i32Ne();
  returnMissing();
  a.localGet(Index);
  a.end();
  return body;
}

// access(address i64) -> i32: the memory address of the byte a byte-region address names, or the VM's fault
// `invalid byte address in IR: <address>` when the byte is in no live region of that generation.
WasmCodeBody buildAccess(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Granule = 1;
  constexpr uint32_t Start = 2;
  constexpr uint32_t Offset = 3;
  WasmCodeBody body;
  declareLocals(body, 2, 1);
  WasmAsm a(body.instructions);
  const auto faultIf = [&]() {
    a.ifThen();
    a.i32Const(FaultByte);
    a.localGet(Address);
    a.call(fn(runtime, WasmHeapHelper::Fault));
    a.end();
  };
  a.localGet(Address);
  a.i64Const(OffsetMask47);
  a.i64And();
  a.localSet(Offset);
  a.localGet(Offset);
  a.i64Const(3);
  a.i64ShrU();
  a.globalGet(WasmGlobalByteSize);
  a.i64ExtendI32U();
  a.i64GeU();
  faultIf();
  a.localGet(Offset);
  a.i64Const(3);
  a.i64ShrU();
  a.i32WrapI64();
  a.localSet(Granule);
  entryAddress(a, Granule, 2, WasmGlobalByteOwnerBase);
  a.i32Load(0);
  a.localSet(Start);
  a.localGet(Start);
  a.i32Eqz();
  faultIf();
  a.localGet(Start);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Start);
  entryAddress(a, Start, 1, WasmGlobalByteGensBase);
  a.i32Load16U(0);
  a.localGet(Address);
  a.i64Const(48);
  a.i64ShrU();
  a.i32WrapI64();
  a.i32Const(static_cast<int32_t>(GenerationMask));
  a.i32And();
  a.i32Ne();
  faultIf();
  // offset - start * 8 must be below the region's byte count
  a.localGet(Offset);
  a.i32WrapI64();
  a.localGet(Start);
  a.i32Const(3);
  a.i32Shl();
  a.i32Sub();
  entryAddress(a, Start, 2, WasmGlobalByteCountsBase);
  a.i32Load(0);
  a.i32GeU();
  faultIf();
  a.localGet(Offset);
  a.i32WrapI64();
  a.globalGet(WasmGlobalByteBase);
  a.i32Add();
  a.end();
  return body;
}

// alloc(count i64) -> i64: `count` zeroed bytes from the smallest free run that fits, else the free run at the end of
// the storage extended, else new granules; address 0 for a count of 0.
WasmCodeBody buildAlloc(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Count = 0;
  constexpr uint32_t Best = 1;
  constexpr uint32_t BestLength = 2;
  constexpr uint32_t Index = 3;
  constexpr uint32_t Length = 4;
  constexpr uint32_t Base = 5;
  constexpr uint32_t Footprint = 6;
  constexpr uint32_t TailRun = 7;
  constexpr uint32_t Generation = 8;
  constexpr uint32_t Count32 = 9;
  constexpr uint32_t Stop = 10;
  WasmCodeBody body;
  declareLocals(body, 10, 0);
  WasmAsm a(body.instructions);
  a.localGet(Count);
  a.i64Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.localGet(Count);
  a.i64Const(static_cast<int64_t>(WasmByteMaxGranules) * 8);
  a.i64GtU();
  a.ifThen();
  emitFault(a, runtime, FaultOverflow);
  a.end();
  a.localGet(Count);
  a.i32WrapI64();
  a.localSet(Count32);
  a.localGet(Count32);
  a.i32Const(7);
  a.i32Add();
  a.i32Const(3);
  a.i32ShrU();
  a.localSet(Footprint);
  a.i32Const(-1);
  a.localSet(Best);
  // the smallest run that fits; the first of equal lengths is the lowest base
  a.block();
  a.loop();
  a.localGet(Index);
  a.globalGet(WasmGlobalByteRunCount);
  a.i32GeU();
  a.brIf(1);
  entryAddress(a, Index, 3, WasmGlobalByteRunsBase);
  a.i32Load(4);
  a.localSet(Length);
  a.localGet(Length);
  a.localGet(Footprint);
  a.i32GeU();
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
  entryAddress(a, Best, 3, WasmGlobalByteRunsBase);
  a.i32Load(0);
  a.localSet(Base);
  a.localGet(Best);
  a.localGet(Footprint);
  a.call(byteFn(runtime, WasmByteHelper::TakeFront));
  a.elseArm();
  // the end of the storage, taking the free run that reaches it
  a.globalGet(WasmGlobalByteSize);
  a.localSet(Base);
  a.i32Const(-1);
  a.localSet(TailRun);
  a.globalGet(WasmGlobalByteRunCount);
  a.ifThen();
  a.globalGet(WasmGlobalByteRunCount);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Index);
  entryAddress(a, Index, 3, WasmGlobalByteRunsBase);
  a.i32Load(0);
  entryAddress(a, Index, 3, WasmGlobalByteRunsBase);
  a.i32Load(4);
  a.i32Add();
  a.globalGet(WasmGlobalByteSize);
  a.i32Eq();
  a.ifThen();
  entryAddress(a, Index, 3, WasmGlobalByteRunsBase);
  a.i32Load(0);
  a.localSet(Base);
  a.localGet(Index);
  a.localSet(TailRun);
  a.end();
  a.end();
  // past the storage's limit, or a host that refuses the memory, is the VM's overflow fault
  a.localGet(Base);
  a.i64ExtendI32U();
  a.localGet(Footprint);
  a.i64ExtendI32U();
  a.i64Add();
  a.i64Const(WasmByteMaxGranules);
  a.i64GtU();
  a.ifThen();
  emitFault(a, runtime, FaultOverflow);
  a.end();
  a.localGet(Base);
  a.i64ExtendI32U();
  a.localGet(Footprint);
  a.i64ExtendI32U();
  a.i64Add();
  a.call(byteFn(runtime, WasmByteHelper::Ensure));
  a.i32Eqz();
  a.ifThen();
  emitFault(a, runtime, FaultOverflow);
  a.end();
  a.localGet(TailRun);
  a.i32Const(-1);
  a.i32Ne();
  a.ifThen();
  a.localGet(TailRun);
  a.call(byteFn(runtime, WasmByteHelper::RemoveRun));
  a.end();
  a.localGet(Base);
  a.localGet(Footprint);
  a.i32Add();
  a.globalSet(WasmGlobalByteSize);
  a.end();
  // the next generation, then the owners, the count and the generation of the region
  a.globalGet(WasmGlobalByteGeneration);
  a.i32Const(1);
  a.i32Add();
  a.i32Const(static_cast<int32_t>(GenerationMask));
  a.i32And();
  a.localSet(Generation);
  a.localGet(Generation);
  a.globalSet(WasmGlobalByteGeneration);
  a.localGet(Base);
  a.localSet(Index);
  a.localGet(Base);
  a.localGet(Footprint);
  a.i32Add();
  a.localSet(Stop);
  a.block();
  a.loop();
  a.localGet(Index);
  a.localGet(Stop);
  a.i32GeU();
  a.brIf(1);
  entryAddress(a, Index, 2, WasmGlobalByteOwnerBase);
  a.localGet(Base);
  a.i32Const(1);
  a.i32Add();
  a.i32Store(0);
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  entryAddress(a, Base, 2, WasmGlobalByteCountsBase);
  a.localGet(Count32);
  a.i32Store(0);
  entryAddress(a, Base, 1, WasmGlobalByteGensBase);
  a.localGet(Generation);
  a.i32Store16(0);
  // Tag | ByteRegionBit | generation << 48 | base * 8
  a.i64Const(AddressTag);
  a.localGet(Generation);
  a.i64ExtendI32U();
  a.i64Const(48);
  a.i64Shl();
  a.i64Or();
  a.localGet(Base);
  a.i64ExtendI32U();
  a.i64Const(3);
  a.i64Shl();
  a.i64Or();
  a.end();
  return body;
}

// free(address i64): the VM's `HeapFree` of a byte-region address.
WasmCodeBody buildFree(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Start = 1;
  WasmCodeBody body;
  declareLocals(body, 1, 0);
  WasmAsm a(body.instructions);
  a.localGet(Address);
  a.call(byteFn(runtime, WasmByteHelper::FindStart));
  a.localSet(Start);
  a.localGet(Start);
  a.i32Const(-1);
  a.i32Eq();
  a.ifThen();
  a.i32Const(FaultFree);
  a.localGet(Address);
  a.call(fn(runtime, WasmHeapHelper::Fault));
  a.end();
  a.localGet(Start);
  a.call(byteFn(runtime, WasmByteHelper::Release));
  a.end();
  return body;
}

// realloc(address i64, count i64) -> i64: the VM's `HeapRealloc` of a byte-region address (the count is not 0): a new
// region holding the old one's leading bytes, the old one freed. The regions may move during the allocation, so the
// copy reads their base afresh.
WasmCodeBody buildRealloc(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Count = 1;
  constexpr uint32_t Start = 2;
  constexpr uint32_t Old = 3;
  constexpr uint32_t NewStart = 4;
  constexpr uint32_t Copied = 5;
  constexpr uint32_t NewAddress = 6;
  WasmCodeBody body;
  declareLocals(body, 4, 1);
  WasmAsm a(body.instructions);
  a.localGet(Address);
  a.call(byteFn(runtime, WasmByteHelper::FindStart));
  a.localSet(Start);
  a.localGet(Start);
  a.i32Const(-1);
  a.i32Eq();
  a.ifThen();
  a.i32Const(FaultRealloc);
  a.localGet(Address);
  a.call(fn(runtime, WasmHeapHelper::Fault));
  a.end();
  entryAddress(a, Start, 2, WasmGlobalByteCountsBase);
  a.i32Load(0);
  a.localSet(Old);
  a.localGet(Count);
  a.call(byteFn(runtime, WasmByteHelper::Alloc));
  a.localSet(NewAddress);
  a.localGet(NewAddress);
  a.i64Const(OffsetMask47);
  a.i64And();
  a.i64Const(3);
  a.i64ShrU();
  a.i32WrapI64();
  a.localSet(NewStart);
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
  entryAddress(a, NewStart, 3, WasmGlobalByteBase);
  entryAddress(a, Start, 3, WasmGlobalByteBase);
  a.localGet(Copied);
  a.memoryCopy();
  a.localGet(Start);
  a.call(byteFn(runtime, WasmByteHelper::Release));
  a.localGet(NewAddress);
  a.end();
  return body;
}

// loadU8(address i64) -> i64: the byte a byte-region address names, or the low byte of the slot any other address names
// (a pointer to a `u8` local or slot field).
WasmCodeBody buildLoadU8(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  WasmCodeBody body;
  declareLocals(body, 0, 0);
  WasmAsm a(body.instructions);
  emitWasmIsByteAddress(a, Address);
  a.ifThen();
  a.localGet(Address);
  a.call(byteFn(runtime, WasmByteHelper::Access));
  a.i32Load8U(0);
  a.i64ExtendI32U();
  a.ret();
  a.end();
  a.localGet(Address);
  a.call(runtime.slotAddressFunction);
  a.i64Load(0);
  a.i64Const(0xff);
  a.i64And();
  a.end();
  return body;
}

// storeU8(address i64, value i64) -> i64: writes the low byte of the value to the byte a byte-region address names, or
// as the whole slot any other address names; returns the byte.
WasmCodeBody buildStoreU8(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Value = 1;
  WasmCodeBody body;
  declareLocals(body, 0, 0);
  WasmAsm a(body.instructions);
  a.localGet(Value);
  a.i64Const(0xff);
  a.i64And();
  a.localSet(Value);
  emitWasmIsByteAddress(a, Address);
  a.ifThen();
  a.localGet(Address);
  a.call(byteFn(runtime, WasmByteHelper::Access));
  a.localGet(Value);
  a.i32WrapI64();
  a.i32Store8(0);
  a.elseArm();
  a.localGet(Address);
  a.call(runtime.slotAddressFunction);
  a.localGet(Value);
  a.i64Store(0);
  a.end();
  a.localGet(Value);
  a.end();
  return body;
}

} // namespace

void emitWasmIsByteAddress(WasmAsm &a, uint32_t addressLocal) {
  a.localGet(addressLocal);
  a.i64Const(63);
  a.i64ShrU();
  a.i64Eqz();
  a.i32Eqz();
  a.localGet(addressLocal);
  a.i64Const(47);
  a.i64ShrU();
  a.i64Const(1);
  a.i64And();
  a.i64Eqz();
  a.i32Eqz();
  a.i32And();
}

WasmFunctionType wasmByteHelperType(WasmByteHelper helper) {
  WasmFunctionType type;
  switch (helper) {
  case WasmByteHelper::RemoveRun:
  case WasmByteHelper::Release:
    type.params = {WasmTypeI32};
    break;
  case WasmByteHelper::InsertRun:
    type.params = {WasmTypeI32, WasmTypeI32, WasmTypeI32};
    break;
  case WasmByteHelper::TakeFront:
    type.params = {WasmTypeI32, WasmTypeI32};
    break;
  case WasmByteHelper::Ensure:
  case WasmByteHelper::FindStart:
  case WasmByteHelper::Access:
    type.params = {WasmTypeI64};
    type.results = {WasmTypeI32};
    break;
  case WasmByteHelper::Alloc:
  case WasmByteHelper::LoadU8:
    type.params = {WasmTypeI64};
    type.results = {WasmTypeI64};
    break;
  case WasmByteHelper::Free:
    type.params = {WasmTypeI64};
    break;
  case WasmByteHelper::Realloc:
  case WasmByteHelper::StoreU8:
    type.params = {WasmTypeI64, WasmTypeI64};
    type.results = {WasmTypeI64};
    break;
  }
  return type;
}

WasmCodeBody buildWasmByteHelperBody(WasmByteHelper helper, const WasmRuntimeContext &runtime) {
  switch (helper) {
  case WasmByteHelper::RemoveRun:
    return buildRemoveRun(runtime, ByteRuns);
  case WasmByteHelper::InsertRun:
    return buildInsertRun(runtime, ByteRuns);
  case WasmByteHelper::TakeFront:
    return buildTakeFront(runtime, ByteRuns, byteFn(runtime, WasmByteHelper::RemoveRun));
  case WasmByteHelper::Ensure:
    return buildEnsure(runtime);
  case WasmByteHelper::Release:
    return buildRelease(runtime);
  case WasmByteHelper::FindStart:
    return buildFindStart(runtime);
  case WasmByteHelper::Access:
    return buildAccess(runtime);
  case WasmByteHelper::Alloc:
    return buildAlloc(runtime);
  case WasmByteHelper::Free:
    return buildFree(runtime);
  case WasmByteHelper::Realloc:
    return buildRealloc(runtime);
  case WasmByteHelper::LoadU8:
    return buildLoadU8(runtime);
  case WasmByteHelper::StoreU8:
    return buildStoreU8(runtime);
  }
  return WasmCodeBody{};
}

} // namespace primec
