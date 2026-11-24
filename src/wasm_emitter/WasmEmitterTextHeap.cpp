#include "WasmEmitterTextHeap.h"

#include "WasmEmitterByteHeap.h"
#include "WasmEmitterTextHeapParts.h"

#include <functional>

// The string heap of a Wasm module (`__pstext_alloc`, `__pstext_try_alloc`, `__pstext_free`, `__pstext_capacity`,
// `__pstext_length`, `__pstext_usable` and the statistics that count its blocks): a port of the small-block half of the
// VM's `VmTextHeap` (include/primec/runtime/VmTextHeapBody.inc) over the byte regions (WasmEmitterByteHeap.cpp), with the
// same block addresses, capacities, reuse and fault texts.
//
// A text block is a slice of a chunk, and a chunk is a byte region taken with the byte `Alloc`, so the generation and the
// offset of every chunk, and with them the address of every block, equal the VM's as long as the same chunks are taken
// and returned in the same order. The bookkeeping must not take byte regions of its own (that would move every later
// address), so it lives in a metadata area of the linear memory: a table of 64-byte chunk records, a table of the 45 size
// classes, and for every chunk a `u32` state per block. A state is 0 while the block is handed out; a free block holds
// bit 31 and the number of the next free block plus one (0 ends the list), which makes the free list of a chunk the VM's
// vector of free block indexes: the block freed last is taken first, a fresh chunk hands out block 0, 1, 2, ... and a
// block freed twice is found in O(1). The metadata allocator carves blocks of 2^n bytes from the area and puts the freed
// ones on a list per size; the area grows with `memory.grow`, never inside the byte regions.
//
// Chunk record (64 bytes): the byte-region address of the chunk (i64) at 0, the chunk's bytes at 8, the block size at 12,
// the block count at 16, the free block count at 20, the first free block plus one at 24, the memory address of the state
// array at 28 and the kind at 32 (0 unused, 1 chunk). Class entry (16 bytes): the bytes its chunks hold at 0, its one
// fully free chunk plus one at 4 (0 none) and its lowest chunk with a free block plus one at 8.
//
// A block above `LargeThreshold` (64 KiB) is a byte region of its own and a record of kind 2 (WasmEmitterTextLarge.cpp, which
// also holds the byte limit). An arena is a record of kind 4 (WasmEmitterTextArena.cpp): its chunks and large blocks name
// it in the record's arena field (its record plus one), so they are found and returned together when it ends.
namespace primec {

using namespace wasm_text;

namespace wasm_text {

// alloc(size i64) -> i64: the VM's `VmTextHeap::allocate`: a zeroed block of the request's size class from
// the class's lowest chunk with a free block, else from a new chunk (when the limit leaves room); a large block for a
// request above `LargeThreshold`; address 0 for a size of 0.
WasmCodeBody buildAlloc(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Size = 0;
  constexpr uint32_t BlockBytes = 1;
  constexpr uint32_t Class = 2;
  constexpr uint32_t Record = 3;
  constexpr uint32_t Entry = 4;
  constexpr uint32_t ChunkBytes = 5;
  constexpr uint32_t Index = 6;
  constexpr uint32_t State = 7;
  constexpr uint32_t Counter = 8;
  constexpr uint32_t Count = 9;
  constexpr uint32_t Next = 10;
  constexpr uint32_t Address = 11;
  WasmCodeBody body;
  declareLocals(body, 10, 1);
  WasmAsm a(body.instructions);
  a.localGet(Size);
  a.i64Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  // one more allocation (a 64-bit count in two words)
  a.globalGet(WasmGlobalTextAllocLow);
  a.i32Const(1);
  a.i32Add();
  a.globalSet(WasmGlobalTextAllocLow);
  a.globalGet(WasmGlobalTextAllocLow);
  a.i32Eqz();
  a.ifThen();
  a.globalGet(WasmGlobalTextAllocHigh);
  a.i32Const(1);
  a.i32Add();
  a.globalSet(WasmGlobalTextAllocHigh);
  a.end();
  a.localGet(Size);
  a.i64Const(LargeThreshold);
  a.i64GtU();
  a.ifThen();
  a.localGet(Size);
  a.call(textFn(runtime, WasmTextHelper::AllocLarge));
  a.ret();
  a.end();
  // while an arena is open every small block is a bump block of the innermost one
  a.globalGet(WasmGlobalTextArena);
  a.ifThen();
  a.localGet(Size);
  a.call(textFn(runtime, WasmTextHelper::AllocInArena));
  a.ret();
  a.end();
  a.localGet(Size);
  a.call(textFn(runtime, WasmTextHelper::BlockSizeFor));
  a.i32WrapI64();
  a.localSet(BlockBytes);
  classAddress(a, runtime, BlockBytes);
  a.localSet(Class);
  a.localGet(Class);
  a.i32Load(ClassLow);
  a.localSet(Record);
  a.localGet(Record);
  a.ifThen();
  // the class's lowest chunk with a free block; when every block of it is free it stops being the class's free chunk
  a.localGet(Record);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Record);
  recordAddress(a, Record);
  a.localSet(Entry);
  a.localGet(Entry);
  a.i32Load(RecFreeCount);
  a.localGet(Entry);
  a.i32Load(RecBlockCount);
  a.i32Eq();
  a.ifThen();
  a.localGet(Class);
  a.i32Const(0);
  a.i32Store(ClassFree);
  a.globalGet(WasmGlobalTextFreeChunkBytes);
  a.localGet(Entry);
  a.i32Load(RecBytes);
  a.i32Sub();
  a.globalSet(WasmGlobalTextFreeChunkBytes);
  a.end();
  a.elseArm();
  // a new chunk: FirstChunk, one block of a larger class, or as much as the class holds, up to ChunkLimit
  a.i32Const(FirstChunk);
  a.localSet(ChunkBytes);
  a.localGet(BlockBytes);
  a.localGet(ChunkBytes);
  a.i32GtU();
  a.ifThen();
  a.localGet(BlockBytes);
  a.localSet(ChunkBytes);
  a.end();
  a.localGet(Class);
  a.i32Load(ClassHeld);
  a.localGet(ChunkBytes);
  a.i32GtU();
  a.ifThen();
  a.localGet(Class);
  a.i32Load(ClassHeld);
  a.localSet(ChunkBytes);
  a.end();
  a.localGet(ChunkBytes);
  a.i32Const(ChunkLimit);
  a.i32GtU();
  a.ifThen();
  a.i32Const(ChunkLimit);
  a.localSet(ChunkBytes);
  a.end();
  a.localGet(ChunkBytes);
  a.i64ExtendI32U();
  a.call(textFn(runtime, WasmTextHelper::RoomFor));
  a.i32Eqz();
  a.ifThen();
  emitLimitFailure(a, runtime);
  a.end();
  a.localGet(ChunkBytes);
  a.i64ExtendI32U();
  a.call(byteFn(runtime, WasmByteHelper::Alloc));
  a.localSet(Address);
  // a region `__pstext_try_alloc` could not get
  a.localGet(Address);
  a.i64Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.call(textFn(runtime, WasmTextHelper::NewRecord));
  a.localSet(Record);
  recordAddress(a, Record);
  a.localSet(Entry);
  a.localGet(Entry);
  a.localGet(Address);
  a.i64Store(RecBase);
  a.localGet(Entry);
  a.localGet(ChunkBytes);
  a.i32Store(RecBytes);
  a.localGet(Entry);
  a.localGet(BlockBytes);
  a.i32Store(RecBlockBytes);
  a.localGet(ChunkBytes);
  a.localGet(BlockBytes);
  a.i32DivU();
  a.localSet(Count);
  a.localGet(Entry);
  a.localGet(Count);
  a.i32Store(RecBlockCount);
  a.localGet(Entry);
  a.localGet(Count);
  a.i32Store(RecFreeCount);
  a.localGet(Entry);
  a.i32Const(1);
  a.i32Store(RecHead);
  a.localGet(Count);
  a.i32Const(2);
  a.i32Shl();
  a.call(textFn(runtime, WasmTextHelper::MetaAlloc));
  a.localSet(State);
  a.localGet(Entry);
  a.localGet(State);
  a.i32Store(RecState);
  // block i points at block i + 1 (the last one ends the list)
  a.i32Const(0);
  a.localSet(Index);
  a.block();
  a.loop();
  a.localGet(Index);
  a.localGet(Count);
  a.i32GeU();
  a.brIf(1);
  a.localGet(Index);
  a.i32Const(2);
  a.i32Add();
  a.localSet(Next);
  a.localGet(State);
  a.localGet(Index);
  a.i32Const(2);
  a.i32Shl();
  a.i32Add();
  a.i32Const(0);
  a.localGet(Next);
  a.localGet(Next);
  a.localGet(Count);
  a.i32GtU();
  a.select();
  a.i32Const(FreeFlag);
  a.i32Or();
  a.i32Store(0);
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  a.localGet(Entry);
  a.i32Const(1);
  a.i32Store(RecKind);
  globalAddLocal(a, WasmGlobalTextChunkBytes, ChunkBytes, false);
  fieldAddLocal(a, Class, ClassHeld, ChunkBytes, false);
  a.localGet(Class);
  a.localGet(Record);
  a.i32Const(1);
  a.i32Add();
  a.i32Store(ClassLow);
  a.globalGet(WasmGlobalTextChunkRegions);
  a.i32Const(1);
  a.i32Add();
  a.globalSet(WasmGlobalTextChunkRegions);
  a.end();
  // the first free block of the chunk
  recordAddress(a, Record);
  a.localSet(Entry);
  a.localGet(Entry);
  a.i32Load(RecHead);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Index);
  a.localGet(Entry);
  a.i32Load(RecState);
  a.localGet(Index);
  a.i32Const(2);
  a.i32Shl();
  a.i32Add();
  a.localSet(State);
  a.localGet(Entry);
  a.localGet(State);
  a.i32Load(0);
  a.i32Const(0x7fffffff);
  a.i32And();
  a.i32Store(RecHead);
  a.localGet(State);
  a.i32Const(0);
  a.i32Store(0);
  a.localGet(Entry);
  a.localGet(Entry);
  a.i32Load(RecFreeCount);
  a.i32Const(1);
  a.i32Sub();
  a.localTee(Counter);
  a.i32Store(RecFreeCount);
  a.localGet(Counter);
  a.i32Eqz();
  a.ifThen();
  a.localGet(BlockBytes);
  a.call(textFn(runtime, WasmTextHelper::RescanLow));
  a.end();
  globalAddLocal(a, WasmGlobalTextInUse, BlockBytes, false);
  a.localGet(Entry);
  a.i64Load(RecBase);
  a.localGet(Index);
  a.localGet(BlockBytes);
  a.i32Mul();
  a.i64ExtendI32U();
  a.i64Add();
  a.end();
  return body;
}

// tryAlloc(size i64) -> i64: `alloc`, except that a byte region the memory cannot hold gives 0 and not the fault.
WasmCodeBody buildTryAlloc(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Size = 0;
  constexpr uint32_t Result = 1;
  WasmCodeBody body;
  declareLocals(body, 0, 1);
  WasmAsm a(body.instructions);
  a.i32Const(1);
  a.globalSet(WasmGlobalTextTry);
  a.localGet(Size);
  a.call(textFn(runtime, WasmTextHelper::Alloc));
  a.localSet(Result);
  a.i32Const(0);
  a.globalSet(WasmGlobalTextTry);
  a.localGet(Result);
  a.end();
  return body;
}

// free(address i64): the VM's `VmTextHeap::release` of a block of a chunk.
WasmCodeBody buildFree(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Record = 1;
  constexpr uint32_t Entry = 2;
  constexpr uint32_t Within = 3;
  constexpr uint32_t BlockBytes = 4;
  constexpr uint32_t BlockIndex = 5;
  constexpr uint32_t State = 6;
  constexpr uint32_t Class = 7;
  constexpr uint32_t Offset = 8;
  constexpr uint32_t FreeCount = 9;
  constexpr uint32_t Other = 10;
  constexpr uint32_t OtherEntry = 11;
  WasmCodeBody body;
  declareLocals(body, 11, 0);
  WasmAsm a(body.instructions);
  const auto badAddress = [&]() {
    a.ifThen();
    faultWith(a, runtime, FaultTextFree, Address);
    a.end();
  };
  a.localGet(Address);
  a.i64Eqz();
  a.ifThen();
  a.ret();
  a.end();
  emitWasmIsByteAddress(a, Address);
  a.i32Eqz();
  badAddress();
  a.localGet(Address);
  a.call(textFn(runtime, WasmTextHelper::Find));
  a.localTee(Record);
  a.i32Const(-1);
  a.i32Eq();
  badAddress();
  recordAddress(a, Record);
  a.localSet(Entry);
  // a large block: its byte region goes back (the byte regions fault on a stale generation) and its record is gone
  // a block of an arena is freed with the arena
  a.localGet(Entry);
  a.i32Load(RecArena);
  a.ifThen();
  a.ret();
  a.end();
  a.localGet(Entry);
  a.i32Load(RecKind);
  a.i32Const(2);
  a.i32Eq();
  a.ifThen();
  a.localGet(Address);
  a.call(byteFn(runtime, WasmByteHelper::Free));
  a.localGet(Entry);
  a.i32Load(RecBytes);
  a.localSet(BlockBytes);
  globalAddLocal(a, WasmGlobalTextLargeBytes, BlockBytes, true);
  globalAddLocal(a, WasmGlobalTextInUse, BlockBytes, true);
  a.globalGet(WasmGlobalTextLargeBlocks);
  a.i32Const(1);
  a.i32Sub();
  a.globalSet(WasmGlobalTextLargeBlocks);
  a.localGet(Entry);
  a.i32Const(0);
  a.i32Const(WasmTextRecordBytes);
  a.memoryFill();
  a.ret();
  a.end();
  a.localGet(Entry);
  a.i32Load(RecBlockBytes);
  a.localSet(BlockBytes);
  a.localGet(Address);
  a.i64Const(OffsetMask47);
  a.i64And();
  a.i32WrapI64();
  a.localSet(Offset);
  a.localGet(Offset);
  recordOffset(a, Entry);
  a.i32Sub();
  a.localSet(Within);
  // inside a block, or an address of another generation of the chunk's region
  a.localGet(Within);
  a.localGet(BlockBytes);
  a.i32RemU();
  a.i32Const(0);
  a.i32Ne();
  a.localGet(Address);
  a.localGet(Entry);
  a.i64Load(RecBase);
  a.i64Xor();
  a.i64Const(48);
  a.i64ShrU();
  a.i64Eqz();
  a.i32Eqz();
  a.i32Or();
  badAddress();
  a.localGet(Within);
  a.localGet(BlockBytes);
  a.i32DivU();
  a.localSet(BlockIndex);
  a.localGet(BlockIndex);
  a.localGet(Entry);
  a.i32Load(RecBlockCount);
  a.i32GeU();
  badAddress();
  a.localGet(Entry);
  a.i32Load(RecState);
  a.localGet(BlockIndex);
  a.i32Const(2);
  a.i32Shl();
  a.i32Add();
  a.localSet(State);
  a.localGet(State);
  a.i32Load(0);
  a.ifThen();
  faultWith(a, runtime, FaultTextTwice, Address);
  a.end();
  // freed text is zeroed so a reused block starts empty
  a.localGet(Address);
  a.call(byteFn(runtime, WasmByteHelper::Access));
  a.i32Const(0);
  a.localGet(BlockBytes);
  a.memoryFill();
  classAddress(a, runtime, BlockBytes);
  a.localSet(Class);
  a.localGet(Entry);
  a.i32Load(RecFreeCount);
  a.localSet(FreeCount);
  // a chunk that had no free block joins the class's chunks with one, and becomes the lowest when it is
  a.localGet(FreeCount);
  a.i32Eqz();
  a.ifThen();
  a.localGet(Class);
  a.i32Load(ClassLow);
  a.localSet(Other);
  a.localGet(Other);
  a.i32Eqz();
  a.ifThen();
  a.localGet(Class);
  a.localGet(Record);
  a.i32Const(1);
  a.i32Add();
  a.i32Store(ClassLow);
  a.elseArm();
  a.localGet(Other);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Other);
  recordAddress(a, Other);
  a.localSet(OtherEntry);
  recordOffset(a, Entry);
  recordOffset(a, OtherEntry);
  a.i32LtU();
  a.ifThen();
  a.localGet(Class);
  a.localGet(Record);
  a.i32Const(1);
  a.i32Add();
  a.i32Store(ClassLow);
  a.end();
  a.end();
  a.end();
  a.localGet(State);
  a.localGet(Entry);
  a.i32Load(RecHead);
  a.i32Const(FreeFlag);
  a.i32Or();
  a.i32Store(0);
  a.localGet(Entry);
  a.localGet(BlockIndex);
  a.i32Const(1);
  a.i32Add();
  a.i32Store(RecHead);
  a.localGet(FreeCount);
  a.i32Const(1);
  a.i32Add();
  a.localSet(FreeCount);
  a.localGet(Entry);
  a.localGet(FreeCount);
  a.i32Store(RecFreeCount);
  globalAddLocal(a, WasmGlobalTextInUse, BlockBytes, true);
  a.localGet(FreeCount);
  a.localGet(Entry);
  a.i32Load(RecBlockCount);
  a.i32Ne();
  a.ifThen();
  a.ret();
  a.end();
  // Every block of the chunk is free. A class keeps one such chunk, the smaller of two (the lower on a tie), and the
  // free chunks of all classes together stay within FreeChunkLimit: the largest goes first.
  a.localGet(Class);
  a.i32Load(ClassFree);
  a.localSet(Other);
  a.localGet(Other);
  a.ifThen();
  a.localGet(Other);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Other);
  recordAddress(a, Other);
  a.localSet(OtherEntry);
  // (other bytes, other offset) < (bytes, offset)
  a.localGet(OtherEntry);
  a.i32Load(RecBytes);
  a.localGet(Entry);
  a.i32Load(RecBytes);
  a.i32LtU();
  a.localGet(OtherEntry);
  a.i32Load(RecBytes);
  a.localGet(Entry);
  a.i32Load(RecBytes);
  a.i32Eq();
  recordOffset(a, OtherEntry);
  recordOffset(a, Entry);
  a.i32LtU();
  a.i32And();
  a.i32Or();
  a.ifThen();
  a.localGet(Record);
  a.call(textFn(runtime, WasmTextHelper::ReturnChunk));
  a.ret();
  a.end();
  a.localGet(Other);
  a.call(textFn(runtime, WasmTextHelper::ReturnFreeChunk));
  a.end();
  a.localGet(Class);
  a.localGet(Record);
  a.i32Const(1);
  a.i32Add();
  a.i32Store(ClassFree);
  a.globalGet(WasmGlobalTextFreeChunkBytes);
  a.localGet(Entry);
  a.i32Load(RecBytes);
  a.i32Add();
  a.globalSet(WasmGlobalTextFreeChunkBytes);
  a.block();
  a.loop();
  a.globalGet(WasmGlobalTextFreeChunkBytes);
  a.i32Const(FreeChunkLimit);
  a.i32GtU();
  a.i32Eqz();
  a.brIf(1);
  a.call(textFn(runtime, WasmTextHelper::LargestFreeChunk));
  a.call(textFn(runtime, WasmTextHelper::ReturnFreeChunk));
  a.br(0);
  a.end();
  a.end();
  a.end();
  return body;
}

// capacity(address i64) -> i64: the size of the block at `address` (a block start of a chunk); 0 for address 0; the
// VM's `invalid string heap address` fault for any other address.
WasmCodeBody buildCapacity(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Record = 1;
  constexpr uint32_t Entry = 2;
  constexpr uint32_t Within = 3;
  WasmCodeBody body;
  declareLocals(body, 3, 0);
  WasmAsm a(body.instructions);
  const auto badAddress = [&]() {
    a.ifThen();
    faultWith(a, runtime, FaultTextAddress, Address);
    a.end();
  };
  a.localGet(Address);
  a.i64Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.localGet(Address);
  a.call(textFn(runtime, WasmTextHelper::Find));
  a.localTee(Record);
  a.i32Const(-1);
  a.i32Eq();
  badAddress();
  recordAddress(a, Record);
  a.localSet(Entry);
  a.localGet(Entry);
  a.i32Load(RecKind);
  a.i32Const(2);
  a.i32Eq();
  a.ifThen();
  a.localGet(Entry);
  a.i32Load(RecBytes);
  a.i64ExtendI32U();
  a.ret();
  a.end();
  a.localGet(Address);
  a.i64Const(OffsetMask47);
  a.i64And();
  a.i32WrapI64();
  recordOffset(a, Entry);
  a.i32Sub();
  a.localSet(Within);
  a.localGet(Within);
  a.localGet(Entry);
  a.i32Load(RecBlockBytes);
  a.i32RemU();
  a.i32Const(0);
  a.i32Ne();
  badAddress();
  a.localGet(Entry);
  a.i32Load(RecBlockBytes);
  a.i64ExtendI32U();
  a.end();
  return body;
}

// length(address i64) -> i64: the bytes of text before the NUL within the block (0 for address 0, and for an address the
// byte regions no longer hold).
WasmCodeBody buildLength(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Size = 1;
  constexpr uint32_t Length = 2;
  constexpr uint32_t Pointer = 3;
  constexpr uint32_t Record = 4;
  WasmCodeBody body;
  declareLocals(body, 4, 0);
  WasmAsm a(body.instructions);
  a.localGet(Address);
  a.call(textFn(runtime, WasmTextHelper::Capacity));
  a.i32WrapI64();
  a.localSet(Size);
  a.localGet(Size);
  a.i32Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  emitWasmIsByteAddress(a, Address);
  a.i32Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.localGet(Address);
  a.call(textFn(runtime, WasmTextHelper::Find));
  a.i32Const(6);
  a.i32Shl();
  a.globalGet(WasmGlobalTextRecords);
  a.i32Add();
  a.localSet(Record);
  a.localGet(Address);
  a.localGet(Record);
  a.i64Load(RecBase);
  a.i64Xor();
  a.i64Const(48);
  a.i64ShrU();
  a.i64Eqz();
  a.i32Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.localGet(Address);
  a.call(byteFn(runtime, WasmByteHelper::Access));
  a.localSet(Pointer);
  a.block();
  a.loop();
  a.localGet(Length);
  a.localGet(Size);
  a.i32GeU();
  a.brIf(1);
  a.localGet(Pointer);
  a.localGet(Length);
  a.i32Add();
  a.i32Load8U(0);
  a.i32Eqz();
  a.brIf(1);
  a.i32Increment(Length);
  a.br(0);
  a.end();
  a.end();
  a.localGet(Length);
  a.i64ExtendI32U();
  a.end();
  return body;
}

// usable(address i64) -> i64: the bytes of text a block holds, its size less the NUL byte (0 for address 0).
WasmCodeBody buildUsable(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Size = 1;
  WasmCodeBody body;
  declareLocals(body, 0, 1);
  WasmAsm a(body.instructions);
  a.localGet(Address);
  a.call(textFn(runtime, WasmTextHelper::Capacity));
  a.localSet(Size);
  a.localGet(Size);
  a.i64Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.localGet(Size);
  a.i64Const(1);
  a.i64Sub();
  a.end();
  return body;
}

// stat(which i64) -> i64: statistics 0 (chunks), 1 (bytes in use), 2 (bytes free), 3 (large blocks), 4 (their bytes) and 5
// (allocations), 6 (open arenas) and 7 (bytes the open arenas handed out).
WasmCodeBody buildStat(const WasmRuntimeContext &) {
  constexpr uint32_t Which = 0;
  constexpr uint32_t Index = 1;
  constexpr uint32_t High = 2;
  constexpr uint32_t Record = 3;
  constexpr uint32_t Total = 4;
  WasmCodeBody body;
  declareLocals(body, 4, 0);
  WasmAsm a(body.instructions);
  const auto answer = [&](int64_t number, const std::function<void()> &value) {
    a.localGet(Which);
    a.i64Const(number);
    a.i64Eq();
    a.ifThen();
    value();
    a.ret();
    a.end();
  };
  answer(1, [&]() {
    a.globalGet(WasmGlobalTextInUse);
    a.i64ExtendI32U();
  });
  answer(3, [&]() {
    a.globalGet(WasmGlobalTextLargeBlocks);
    a.i64ExtendI32U();
  });
  answer(4, [&]() {
    a.globalGet(WasmGlobalTextLargeBytes);
    a.i64ExtendI32U();
  });
  answer(6, [&]() {
    a.globalGet(WasmGlobalTextArenaDepth);
    a.i64ExtendI32U();
  });
  answer(7, [&]() {
    a.globalGet(WasmGlobalTextArenaBytes);
    a.i64ExtendI32U();
  });
  answer(5, [&]() {
    a.globalGet(WasmGlobalTextAllocHigh);
    a.i64ExtendI32U();
    a.i64Const(32);
    a.i64Shl();
    a.globalGet(WasmGlobalTextAllocLow);
    a.i64ExtendI32U();
    a.i64Or();
  });
  // the chunks and the bytes of their free blocks, from the records
  a.localGet(Which);
  a.i64Const(2);
  a.i64LeU();
  a.ifThen();
  a.globalGet(WasmGlobalTextRecordHigh);
  a.localSet(High);
  a.block();
  a.loop();
  a.localGet(Index);
  a.localGet(High);
  a.i32GeU();
  a.brIf(1);
  recordAddress(a, Index);
  a.localSet(Record);
  a.localGet(Record);
  a.i32Load(RecKind);
  a.i32Const(1);
  a.i32And();
  a.ifThen();
  a.localGet(Which);
  a.i64Eqz();
  a.ifThen();
  a.i32Const(1);
  a.localGet(Total);
  a.i32Add();
  a.localSet(Total);
  a.elseArm();
  a.localGet(Total);
  a.localGet(Record);
  a.i32Load(RecFreeCount);
  a.localGet(Record);
  a.i32Load(RecBlockBytes);
  a.i32Mul();
  a.i32Add();
  a.localSet(Total);
  a.end();
  a.end();
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  a.localGet(Total);
  a.i64ExtendI32U();
  a.ret();
  a.end();
  a.i64Const(0);
  a.end();
  return body;
}

} // namespace wasm_text

WasmFunctionType wasmTextHelperType(WasmTextHelper helper) {
  WasmFunctionType type;
  switch (helper) {
  case WasmTextHelper::MetaAlloc:
  case WasmTextHelper::ClassIndex:
    type.params = {WasmTypeI32};
    type.results = {WasmTypeI32};
    break;
  case WasmTextHelper::MetaFree:
    type.params = {WasmTypeI32, WasmTypeI32};
    break;
  case WasmTextHelper::BlockSizeFor:
  case WasmTextHelper::Alloc:
  case WasmTextHelper::TryAlloc:
  case WasmTextHelper::Capacity:
  case WasmTextHelper::Length:
  case WasmTextHelper::Usable:
  case WasmTextHelper::Stat:
    type.params = {WasmTypeI64};
    type.results = {WasmTypeI64};
    break;
  case WasmTextHelper::Find:
    type.params = {WasmTypeI64};
    type.results = {WasmTypeI32};
    break;
  case WasmTextHelper::NewRecord:
  case WasmTextHelper::LargestFreeChunk:
    type.results = {WasmTypeI32};
    break;
  case WasmTextHelper::RescanLow:
  case WasmTextHelper::ReturnChunk:
  case WasmTextHelper::ReturnFreeChunk:
    type.params = {WasmTypeI32};
    break;
  case WasmTextHelper::AllocLarge:
  case WasmTextHelper::AllocInArena:
    type.params = {WasmTypeI64};
    type.results = {WasmTypeI64};
    break;
  case WasmTextHelper::ArenaBegin:
  case WasmTextHelper::ArenaEnd:
    break;
  case WasmTextHelper::MoveBytes:
  case WasmTextHelper::FillBytes:
    type.params = {WasmTypeI64, WasmTypeI64, WasmTypeI64};
    break;
  case WasmTextHelper::CompareBytes:
  case WasmTextHelper::FindByte:
    type.params = {WasmTypeI64, WasmTypeI64, WasmTypeI64};
    type.results = {WasmTypeI64};
    break;
  case WasmTextHelper::Literal:
  case WasmTextHelper::Argument:
    type.params = {WasmTypeI64};
    type.results = {WasmTypeI64};
    break;
  case WasmTextHelper::RoomFor:
    type.params = {WasmTypeI64};
    type.results = {WasmTypeI32};
    break;
  case WasmTextHelper::Free:
  case WasmTextHelper::SetLimit:
    type.params = {WasmTypeI64};
    break;
  }
  return type;
}

WasmCodeBody buildWasmTextHelperBody(WasmTextHelper helper, const WasmRuntimeContext &runtime) {
  switch (helper) {
  case WasmTextHelper::MetaAlloc:
    return buildMetaAlloc(runtime);
  case WasmTextHelper::MetaFree:
    return buildMetaFree(runtime);
  case WasmTextHelper::ClassIndex:
    return buildClassIndex(runtime);
  case WasmTextHelper::BlockSizeFor:
    return buildBlockSizeFor(runtime);
  case WasmTextHelper::Find:
    return buildFind(runtime);
  case WasmTextHelper::NewRecord:
    return buildNewRecord(runtime);
  case WasmTextHelper::RescanLow:
    return buildRescanLow(runtime);
  case WasmTextHelper::ReturnChunk:
    return buildReturnChunk(runtime);
  case WasmTextHelper::ReturnFreeChunk:
    return buildReturnFreeChunk(runtime);
  case WasmTextHelper::LargestFreeChunk:
    return buildLargestFreeChunk(runtime);
  case WasmTextHelper::Alloc:
    return buildAlloc(runtime);
  case WasmTextHelper::TryAlloc:
    return buildTryAlloc(runtime);
  case WasmTextHelper::Free:
    return buildFree(runtime);
  case WasmTextHelper::Capacity:
    return buildCapacity(runtime);
  case WasmTextHelper::Length:
    return buildLength(runtime);
  case WasmTextHelper::Usable:
    return buildUsable(runtime);
  case WasmTextHelper::Stat:
    return buildStat(runtime);
  case WasmTextHelper::RoomFor:
    return buildRoomFor(runtime);
  case WasmTextHelper::AllocLarge:
    return buildAllocLarge(runtime);
  case WasmTextHelper::SetLimit:
    return buildSetLimit(runtime);
  case WasmTextHelper::AllocInArena:
    return buildAllocInArena(runtime);
  case WasmTextHelper::ArenaBegin:
    return buildArenaBegin(runtime);
  case WasmTextHelper::ArenaEnd:
    return buildArenaEnd(runtime);
  case WasmTextHelper::MoveBytes:
    return buildMoveBytes(runtime);
  case WasmTextHelper::FillBytes:
    return buildFillBytes(runtime);
  case WasmTextHelper::CompareBytes:
    return buildCompareBytes(runtime);
  case WasmTextHelper::FindByte:
    return buildFindByte(runtime);
  case WasmTextHelper::Literal:
    return buildLiteral(runtime);
  case WasmTextHelper::Argument:
    return buildArgument(runtime);
  }
  return WasmCodeBody{};
}

std::vector<uint32_t> wasmTextGlobalInitials(const WasmRuntimeContext &runtime) {
  // In the order of WasmGlobalTextMetaBump on: the metadata area, the record table and its room, no records, and zero
  // for every counter.
  return {runtime.textMeta,
          runtime.textMeta + WasmTextMetaBytes,
          runtime.textRecords,
          WasmTextInitialRecords,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u,
          0u};
}

} // namespace primec
