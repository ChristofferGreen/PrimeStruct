#include "WasmEmitterTextHeapParts.h"

namespace primec {
namespace wasm_text {

// The string arenas (the VM's `VmTextHeap::beginArena`, `allocateInArena` and `endArena`; see WasmEmitterTextHeap.cpp for
// the layout). An arena is a chunk record of kind 4: the bytes of the small blocks it handed out (at 8), the arena it
// is nested in plus one (at 24) and a table of 45 entries of 8 bytes, one per size class, in the metadata area (at 28): the
// chunk the arena is filling plus one at 0 and the size of its next chunk for the class at 4. A chunk of an arena is a
// record of kind 1 with the arena plus one in its arena field and its bump offset in the head field; blocks are never
// reused or freed one by one. A large block made in an arena names it in the same field.

inline constexpr uint32_t ArenaTableBytes = static_cast<uint32_t>(WasmTextClassCount) * 8u;

// allocInArena(size i64) -> i64: a small block (at most `LargeThreshold` bytes, not empty) of the innermost arena: the
// next block of the chunk it is filling for the size class, else of a new chunk (4096 bytes or four blocks, doubling to
// `ChunkLimit`) when the limit leaves room; 0 when `__pstext_try_alloc` could not get the chunk.
WasmCodeBody buildAllocInArena(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Size = 0;
  constexpr uint32_t BlockBytes = 1;
  constexpr uint32_t Arena = 2;
  constexpr uint32_t Slot = 3;
  constexpr uint32_t Chunk = 4;
  constexpr uint32_t Entry = 5;
  constexpr uint32_t ChunkBytes = 6;
  constexpr uint32_t Record = 7;
  constexpr uint32_t Bump = 8;
  constexpr uint32_t Address = 9;
  WasmCodeBody body;
  declareLocals(body, 8, 1);
  WasmAsm a(body.instructions);
  a.localGet(Size);
  a.call(textFn(runtime, WasmTextHelper::BlockSizeFor));
  a.i32WrapI64();
  a.localSet(BlockBytes);
  a.globalGet(WasmGlobalTextArena);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Arena);
  // the arena's entry for the size class
  recordAddress(a, Arena);
  a.i32Load(RecState);
  a.localGet(BlockBytes);
  a.call(textFn(runtime, WasmTextHelper::ClassIndex));
  a.i32Const(3);
  a.i32Shl();
  a.i32Add();
  a.localSet(Slot);
  // the chunk it is filling, when the next block fits
  a.localGet(Slot);
  a.i32Load(0);
  a.localSet(Chunk);
  a.localGet(Chunk);
  a.ifThen();
  a.localGet(Chunk);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Record);
  recordAddress(a, Record);
  a.localSet(Entry);
  a.localGet(Entry);
  a.i32Load(RecHead);
  a.localGet(BlockBytes);
  a.i32Add();
  a.localGet(Entry);
  a.i32Load(RecBlockCount);
  a.localGet(BlockBytes);
  a.i32Mul();
  a.i32GtU();
  a.ifThen();
  a.i32Const(0);
  a.localSet(Chunk);
  a.end();
  a.end();
  a.localGet(Chunk);
  a.i32Eqz();
  a.ifThen();
  // a new chunk: the size recorded for the class, else four blocks (at least 4096 bytes), at most `ChunkLimit`; the next
  // one is twice as large
  a.localGet(Slot);
  a.i32Load(4);
  a.localSet(ChunkBytes);
  a.localGet(ChunkBytes);
  a.i32Eqz();
  a.ifThen();
  a.localGet(BlockBytes);
  a.i32Const(2);
  a.i32Shl();
  a.localSet(ChunkBytes);
  a.localGet(ChunkBytes);
  a.i32Const(FirstChunk);
  a.i32LtU();
  a.ifThen();
  a.i32Const(FirstChunk);
  a.localSet(ChunkBytes);
  a.end();
  a.end();
  a.localGet(ChunkBytes);
  a.i32Const(ChunkLimit);
  a.i32GtU();
  a.ifThen();
  a.i32Const(ChunkLimit);
  a.localSet(ChunkBytes);
  a.end();
  a.localGet(Slot);
  a.localGet(ChunkBytes);
  a.i32Const(1);
  a.i32Shl();
  a.i32Const(ChunkLimit);
  a.localGet(ChunkBytes);
  a.i32Const(1);
  a.i32Shl();
  a.i32Const(ChunkLimit);
  a.i32LtU();
  a.select();
  a.i32Store(4);
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
  a.localGet(Entry);
  a.localGet(ChunkBytes);
  a.localGet(BlockBytes);
  a.i32DivU();
  a.i32Store(RecBlockCount);
  a.localGet(Entry);
  a.globalGet(WasmGlobalTextArena);
  a.i32Store(RecArena);
  a.localGet(Entry);
  a.i32Const(1);
  a.i32Store(RecKind);
  globalAddLocal(a, WasmGlobalTextChunkBytes, ChunkBytes, false);
  a.localGet(Slot);
  a.localGet(Record);
  a.i32Const(1);
  a.i32Add();
  a.localTee(Chunk);
  a.i32Store(0);
  a.end();
  // the next block of the chunk
  a.localGet(Chunk);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Record);
  recordAddress(a, Record);
  a.localSet(Entry);
  a.localGet(Entry);
  a.i32Load(RecHead);
  a.localSet(Bump);
  a.localGet(Entry);
  a.localGet(Bump);
  a.localGet(BlockBytes);
  a.i32Add();
  a.i32Store(RecHead);
  a.localGet(Entry);
  a.i64Load(RecBase);
  a.localGet(Bump);
  a.i64ExtendI32U();
  a.i64Add();
  a.localSet(Address);
  globalAddLocal(a, WasmGlobalTextInUse, BlockBytes, false);
  globalAddLocal(a, WasmGlobalTextArenaBytes, BlockBytes, false);
  recordAddress(a, Arena);
  a.localSet(Entry);
  fieldAddLocal(a, Entry, RecBytes, BlockBytes, false);
  a.localGet(Address);
  a.end();
  return body;
}

// arenaBegin(): a new innermost arena.
WasmCodeBody buildArenaBegin(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Record = 0;
  constexpr uint32_t Entry = 1;
  constexpr uint32_t Table = 2;
  WasmCodeBody body;
  declareLocals(body, 3, 0);
  WasmAsm a(body.instructions);
  a.call(textFn(runtime, WasmTextHelper::NewRecord));
  a.localSet(Record);
  a.i32Const(static_cast<int32_t>(ArenaTableBytes));
  a.call(textFn(runtime, WasmTextHelper::MetaAlloc));
  a.localSet(Table);
  recordAddress(a, Record);
  a.localSet(Entry);
  a.localGet(Entry);
  a.globalGet(WasmGlobalTextArena);
  a.i32Store(RecHead);
  a.localGet(Entry);
  a.localGet(Table);
  a.i32Store(RecState);
  a.localGet(Entry);
  a.i32Const(4);
  a.i32Store(RecKind);
  a.localGet(Record);
  a.i32Const(1);
  a.i32Add();
  a.globalSet(WasmGlobalTextArena);
  a.globalGet(WasmGlobalTextArenaDepth);
  a.i32Const(1);
  a.i32Add();
  a.globalSet(WasmGlobalTextArenaDepth);
  a.end();
  return body;
}

// arenaEnd(): returns every chunk and large block of the innermost arena at once; the VM's fault when none is open.
WasmCodeBody buildArenaEnd(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Tag = 0;
  constexpr uint32_t Entry = 1;
  constexpr uint32_t Index = 2;
  constexpr uint32_t High = 3;
  constexpr uint32_t Record = 4;
  constexpr uint32_t Kind = 5;
  constexpr uint32_t Bytes = 6;
  constexpr uint32_t Table = 7;
  constexpr uint32_t Parent = 8;
  WasmCodeBody body;
  declareLocals(body, 9, 0);
  WasmAsm a(body.instructions);
  a.globalGet(WasmGlobalTextArena);
  a.localSet(Tag);
  a.localGet(Tag);
  a.i32Eqz();
  a.ifThen();
  a.i32Const(FaultTextArena);
  a.i64Const(0);
  a.call(fn(runtime, WasmHeapHelper::Fault));
  a.end();
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
  a.i32Load(RecArena);
  a.localGet(Tag);
  a.i32Eq();
  a.ifThen();
  a.localGet(Record);
  a.i32Load(RecKind);
  a.localSet(Kind);
  a.localGet(Record);
  a.i32Load(RecBytes);
  a.localSet(Bytes);
  a.localGet(Kind);
  a.i32Const(1);
  a.i32Eq();
  a.ifThen();
  globalAddLocal(a, WasmGlobalTextChunkBytes, Bytes, true);
  a.elseArm();
  globalAddLocal(a, WasmGlobalTextLargeBytes, Bytes, true);
  globalAddLocal(a, WasmGlobalTextInUse, Bytes, true);
  a.globalGet(WasmGlobalTextLargeBlocks);
  a.i32Const(1);
  a.i32Sub();
  a.globalSet(WasmGlobalTextLargeBlocks);
  a.end();
  a.localGet(Record);
  a.i64Load(RecBase);
  a.call(byteFn(runtime, WasmByteHelper::Free));
  a.localGet(Record);
  a.i32Const(0);
  a.i32Const(WasmTextRecordBytes);
  a.memoryFill();
  a.end();
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  // the arena itself: its small blocks are no longer in use, and the arena it was nested in is the innermost again
  a.localGet(Tag);
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Index);
  recordAddress(a, Index);
  a.localSet(Entry);
  a.localGet(Entry);
  a.i32Load(RecBytes);
  a.localSet(Bytes);
  a.localGet(Entry);
  a.i32Load(RecState);
  a.localSet(Table);
  a.localGet(Entry);
  a.i32Load(RecHead);
  a.localSet(Parent);
  globalAddLocal(a, WasmGlobalTextInUse, Bytes, true);
  globalAddLocal(a, WasmGlobalTextArenaBytes, Bytes, true);
  a.localGet(Entry);
  a.i32Const(0);
  a.i32Const(WasmTextRecordBytes);
  a.memoryFill();
  a.localGet(Table);
  a.i32Const(static_cast<int32_t>(ArenaTableBytes));
  a.call(textFn(runtime, WasmTextHelper::MetaFree));
  a.localGet(Parent);
  a.globalSet(WasmGlobalTextArena);
  a.globalGet(WasmGlobalTextArenaDepth);
  a.i32Const(1);
  a.i32Sub();
  a.globalSet(WasmGlobalTextArenaDepth);
  a.end();
  return body;
}

} // namespace wasm_text
} // namespace primec
