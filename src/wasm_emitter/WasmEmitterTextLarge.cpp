#include "WasmEmitterTextHeapParts.h"

namespace primec {
namespace wasm_text {

// The large blocks and the byte limit of the string heap (see WasmEmitterTextHeap.cpp for the layout). A request above
// `LargeThreshold` is a byte region of exactly the requested size, kept in a chunk record of kind 2 (base address and
// size); the limit counts the chunk regions and the large blocks together.

// roomFor(bytes i64) -> i32: the VM's `VmTextHeap::roomFor`: 1 when `bytes` more stay within the limit (0 for no limit),
// also after the free chunks were returned, largest first, to make room; else 0.
WasmCodeBody buildRoomFor(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Bytes = 0;
  constexpr uint32_t Limit = 1;
  WasmCodeBody body;
  declareLocals(body, 0, 1);
  WasmAsm a(body.instructions);
  a.globalGet(WasmGlobalTextLimitHigh);
  a.i64ExtendI32U();
  a.i64Const(32);
  a.i64Shl();
  a.globalGet(WasmGlobalTextLimitLow);
  a.i64ExtendI32U();
  a.i64Or();
  a.localSet(Limit);
  a.localGet(Limit);
  a.i64Eqz();
  a.ifThen();
  a.i32Const(1);
  a.ret();
  a.end();
  // held bytes (chunk regions and large blocks) plus the request, within the limit
  const auto fits = [&]() {
    a.globalGet(WasmGlobalTextChunkBytes);
    a.i64ExtendI32U();
    a.globalGet(WasmGlobalTextLargeBytes);
    a.i64ExtendI32U();
    a.i64Add();
    a.localGet(Bytes);
    a.i64Add();
    a.localGet(Limit);
    a.i64LeU();
    a.ifThen();
    a.i32Const(1);
    a.ret();
    a.end();
  };
  fits();
  a.block();
  a.loop();
  a.globalGet(WasmGlobalTextFreeChunkBytes);
  a.i32Eqz();
  a.brIf(1);
  a.call(textFn(runtime, WasmTextHelper::LargestFreeChunk));
  a.call(textFn(runtime, WasmTextHelper::ReturnFreeChunk));
  fits();
  a.br(0);
  a.end();
  a.end();
  a.i32Const(0);
  a.end();
  return body;
}

// allocLarge(size i64) -> i64: a block above `LargeThreshold`: a byte region of `size` bytes recorded as a large block;
// 0 when `__pstext_try_alloc` could not get it.
WasmCodeBody buildAllocLarge(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Size = 0;
  constexpr uint32_t Record = 1;
  constexpr uint32_t Entry = 2;
  constexpr uint32_t Bytes = 3;
  constexpr uint32_t Address = 4;
  WasmCodeBody body;
  declareLocals(body, 3, 1);
  WasmAsm a(body.instructions);
  a.localGet(Size);
  a.call(textFn(runtime, WasmTextHelper::RoomFor));
  a.i32Eqz();
  a.ifThen();
  emitLimitFailure(a, runtime);
  a.end();
  a.localGet(Size);
  a.call(byteFn(runtime, WasmByteHelper::Alloc));
  a.localSet(Address);
  a.localGet(Address);
  a.i64Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.localGet(Size);
  a.i32WrapI64();
  a.localSet(Bytes);
  a.call(textFn(runtime, WasmTextHelper::NewRecord));
  a.localSet(Record);
  recordAddress(a, Record);
  a.localSet(Entry);
  a.localGet(Entry);
  a.localGet(Address);
  a.i64Store(RecBase);
  a.localGet(Entry);
  a.localGet(Bytes);
  a.i32Store(RecBytes);
  a.localGet(Entry);
  a.i32Const(2);
  a.i32Store(RecKind);
  globalAddLocal(a, WasmGlobalTextLargeBytes, Bytes, false);
  globalAddLocal(a, WasmGlobalTextInUse, Bytes, false);
  a.globalGet(WasmGlobalTextLargeBlocks);
  a.i32Const(1);
  a.i32Add();
  a.globalSet(WasmGlobalTextLargeBlocks);
  a.localGet(Address);
  a.end();
  return body;
}

// setLimit(bytes i64): the most bytes of chunk regions and large blocks the heap holds (0 removes the limit).
WasmCodeBody buildSetLimit(const WasmRuntimeContext &) {
  constexpr uint32_t Bytes = 0;
  WasmCodeBody body;
  declareLocals(body, 0, 0);
  WasmAsm a(body.instructions);
  a.localGet(Bytes);
  a.i32WrapI64();
  a.globalSet(WasmGlobalTextLimitLow);
  a.localGet(Bytes);
  a.i64Const(32);
  a.i64ShrU();
  a.i32WrapI64();
  a.globalSet(WasmGlobalTextLimitHigh);
  a.end();
  return body;
}

} // namespace wasm_text
} // namespace primec
