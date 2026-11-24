#include "WasmEmitterTextHeapParts.h"

namespace primec {
namespace wasm_text {

// The bulk byte operations of `String` (`__pstext_move`, `_fill`, `_compare` and `_find`): the VM's one range check per
// call (`VmHeapCore::byteSpan`: every byte of the range lies in the one live region of the address) and then a library
// loop. A count of 0 touches nothing, whatever the addresses; a range that leaves its region is the fault
// `invalid text range: <count> bytes at <first address>`.

namespace {

// pointerLocal = the memory address of the range of `countLocal` bytes at `addressLocal`, or the range fault naming
// `firstAddressLocal`.
void emitSpanOrFault(WasmAsm &a,
                     const WasmRuntimeContext &runtime,
                     uint32_t addressLocal,
                     uint32_t countLocal,
                     uint32_t pointerLocal,
                     uint32_t firstAddressLocal) {
  a.localGet(addressLocal);
  a.localGet(countLocal);
  a.call(byteFn(runtime, WasmByteHelper::Span));
  a.localTee(pointerLocal);
  a.i32Eqz();
  a.ifThen();
  emitRangeFailure(a, runtime, countLocal, firstAddressLocal);
  a.end();
}

} // namespace

// moveBytes(destination, source, count): overlap allowed.
WasmCodeBody buildMoveBytes(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Destination = 0;
  constexpr uint32_t Source = 1;
  constexpr uint32_t Count = 2;
  constexpr uint32_t To = 3;
  constexpr uint32_t From = 4;
  WasmCodeBody body;
  declareLocals(body, 2, 0);
  WasmAsm a(body.instructions);
  a.localGet(Count);
  a.i64Eqz();
  a.ifThen();
  a.ret();
  a.end();
  emitSpanOrFault(a, runtime, Destination, Count, To, Destination);
  emitSpanOrFault(a, runtime, Source, Count, From, Destination);
  a.localGet(To);
  a.localGet(From);
  a.localGet(Count);
  a.i32WrapI64();
  a.memoryCopy();
  a.end();
  return body;
}

// fillBytes(destination, value, count): the low byte of the value.
WasmCodeBody buildFillBytes(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Destination = 0;
  constexpr uint32_t Value = 1;
  constexpr uint32_t Count = 2;
  constexpr uint32_t To = 3;
  WasmCodeBody body;
  declareLocals(body, 1, 0);
  WasmAsm a(body.instructions);
  a.localGet(Count);
  a.i64Eqz();
  a.ifThen();
  a.ret();
  a.end();
  emitSpanOrFault(a, runtime, Destination, Count, To, Destination);
  a.localGet(To);
  a.localGet(Value);
  a.i32WrapI64();
  a.localGet(Count);
  a.i32WrapI64();
  a.memoryFill();
  a.end();
  return body;
}

// compareBytes(a, b, count) -> -1, 0 or 1: the first differing byte, as unsigned bytes.
WasmCodeBody buildCompareBytes(const WasmRuntimeContext &runtime) {
  constexpr uint32_t First = 0;
  constexpr uint32_t Second = 1;
  constexpr uint32_t Count = 2;
  constexpr uint32_t PointerA = 3;
  constexpr uint32_t PointerB = 4;
  constexpr uint32_t Index = 5;
  constexpr uint32_t Count32 = 6;
  constexpr uint32_t ByteA = 7;
  constexpr uint32_t ByteB = 8;
  WasmCodeBody body;
  declareLocals(body, 6, 0);
  WasmAsm a(body.instructions);
  a.localGet(Count);
  a.i64Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  emitSpanOrFault(a, runtime, First, Count, PointerA, First);
  emitSpanOrFault(a, runtime, Second, Count, PointerB, First);
  a.localGet(Count);
  a.i32WrapI64();
  a.localSet(Count32);
  a.block();
  a.loop();
  a.localGet(Index);
  a.localGet(Count32);
  a.i32GeU();
  a.brIf(1);
  a.localGet(PointerA);
  a.localGet(Index);
  a.i32Add();
  a.i32Load8U(0);
  a.localSet(ByteA);
  a.localGet(PointerB);
  a.localGet(Index);
  a.i32Add();
  a.i32Load8U(0);
  a.localSet(ByteB);
  a.localGet(ByteA);
  a.localGet(ByteB);
  a.i32Ne();
  a.ifThen();
  a.i64Const(-1);
  a.i64Const(1);
  a.localGet(ByteA);
  a.localGet(ByteB);
  a.i32LtU();
  a.select();
  a.ret();
  a.end();
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  a.i64Const(0);
  a.end();
  return body;
}

// findByte(address, count, value) -> the index of the first byte equal to the low byte of the value, or -1.
WasmCodeBody buildFindByte(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Address = 0;
  constexpr uint32_t Count = 1;
  constexpr uint32_t Value = 2;
  constexpr uint32_t Pointer = 3;
  constexpr uint32_t Index = 4;
  constexpr uint32_t Count32 = 5;
  WasmCodeBody body;
  declareLocals(body, 3, 0);
  WasmAsm a(body.instructions);
  a.localGet(Count);
  a.i64Eqz();
  a.ifThen();
  a.i64Const(-1);
  a.ret();
  a.end();
  emitSpanOrFault(a, runtime, Address, Count, Pointer, Address);
  a.localGet(Count);
  a.i32WrapI64();
  a.localSet(Count32);
  a.block();
  a.loop();
  a.localGet(Index);
  a.localGet(Count32);
  a.i32GeU();
  a.brIf(1);
  a.localGet(Pointer);
  a.localGet(Index);
  a.i32Add();
  a.i32Load8U(0);
  a.localGet(Value);
  a.i32WrapI64();
  a.i32Const(255);
  a.i32And();
  a.i32Eq();
  a.ifThen();
  a.localGet(Index);
  a.i64ExtendI32U();
  a.ret();
  a.end();
  a.i32Increment(Index);
  a.br(0);
  a.end();
  a.end();
  a.i64Const(-1);
  a.end();
  return body;
}

} // namespace wasm_text
} // namespace primec
