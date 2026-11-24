#include "WasmEmitterTextHeapParts.h"

namespace primec {
namespace wasm_text {

// The string literals and the entry arguments of `String` (`__pstext_literal` and `__pstext_argument`).

// literal(index i64) -> i64: the address of a module string literal's bytes (the VM's `VmTextHeap::literal`): the
// literal is copied, NUL terminated, into a byte region of its own the first time it is asked for, and the region lives
// as long as the module; an empty literal has address 0. The literals sit in the data segment (the table of their
// addresses and lengths is `textLiteralInfo`), the addresses already made in the table `textLiterals`.
WasmCodeBody buildLiteral(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Index = 0;
  constexpr uint32_t Info = 1;
  constexpr uint32_t Length = 2;
  constexpr uint32_t Destination = 3;
  constexpr uint32_t Region = 4;
  WasmCodeBody body;
  declareLocals(body, 3, 1);
  WasmAsm a(body.instructions);
  a.localGet(Index);
  a.i64Const(runtime.textLiteralCount);
  a.i64GeU();
  a.ifThen();
  a.unreachable();
  a.end();
  a.localGet(Index);
  a.i32WrapI64();
  a.i32Const(3);
  a.i32Shl();
  a.i32Const(static_cast<int32_t>(runtime.textLiteralInfo));
  a.i32Add();
  a.localSet(Info);
  a.localGet(Info);
  a.i32Load(4);
  a.localSet(Length);
  a.localGet(Length);
  a.i32Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.localGet(Index);
  a.i32WrapI64();
  a.i32Const(3);
  a.i32Shl();
  a.i32Const(static_cast<int32_t>(runtime.textLiterals));
  a.i32Add();
  a.i64Load(0);
  a.localSet(Region);
  a.localGet(Region);
  a.i64Eqz();
  a.i32Eqz();
  a.ifThen();
  a.localGet(Region);
  a.ret();
  a.end();
  a.localGet(Length);
  a.i64ExtendI32U();
  a.i64Const(1);
  a.i64Add();
  a.call(byteFn(runtime, WasmByteHelper::Alloc));
  a.localSet(Region);
  a.localGet(Region);
  a.call(byteFn(runtime, WasmByteHelper::Access));
  a.localSet(Destination);
  a.localGet(Destination);
  a.localGet(Info);
  a.i32Load(0);
  a.localGet(Length);
  a.memoryCopy();
  a.localGet(Index);
  a.i32WrapI64();
  a.i32Const(3);
  a.i32Shl();
  a.i32Const(static_cast<int32_t>(runtime.textLiterals));
  a.i32Add();
  a.localGet(Region);
  a.i64Store(0);
  a.localGet(Region);
  a.end();
  return body;
}

// argument(index i64) -> i64: the entry argument at the index copied into a fresh text block (0 for none or an empty
// one), read with `args_sizes_get` and `args_get` into the static argument buffers.
WasmCodeBody buildArgument(const WasmRuntimeContext &runtime) {
  constexpr uint32_t Index = 0;
  constexpr uint32_t Position = 1;
  constexpr uint32_t Count = 2;
  constexpr uint32_t BufferBytes = 3;
  constexpr uint32_t Pointer = 4;
  constexpr uint32_t End = 5;
  constexpr uint32_t Length = 6;
  constexpr uint32_t Address = 7;
  WasmCodeBody body;
  declareLocals(body, 6, 1);
  WasmAsm a(body.instructions);
  if (!runtime.hasArgvOps) {
    // no `args_get` import: the module never asks for an argument
    a.i64Const(0);
    a.end();
    return body;
  }
  a.localGet(Index);
  a.i32WrapI64();
  a.localSet(Position);
  a.i32Const(static_cast<int32_t>(runtime.argcAddr));
  a.i32Const(static_cast<int32_t>(runtime.argvBufSizeAddr));
  a.call(runtime.argsSizesGetImportIndex);
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.i32Const(static_cast<int32_t>(runtime.argcAddr));
  a.i32Load(0);
  a.localSet(Count);
  a.i32Const(static_cast<int32_t>(runtime.argvBufSizeAddr));
  a.i32Load(0);
  a.localSet(BufferBytes);
  // a negative index is a large unsigned one
  a.localGet(Position);
  a.localGet(Count);
  a.i32GeU();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  // more arguments than the static buffers hold
  a.localGet(Count);
  a.i32Const(static_cast<int32_t>(runtime.argvPtrsCapacityBytes / 4));
  a.i32GtU();
  a.localGet(BufferBytes);
  a.i32Const(static_cast<int32_t>(runtime.argvBufferCapacityBytes));
  a.i32GtU();
  a.i32Or();
  a.ifThen();
  a.unreachable();
  a.end();
  a.i32Const(static_cast<int32_t>(runtime.argvPtrsBase));
  a.i32Const(static_cast<int32_t>(runtime.argvBufferBase));
  a.call(runtime.argsGetImportIndex);
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.localGet(Position);
  a.i32Const(2);
  a.i32Shl();
  a.i32Const(static_cast<int32_t>(runtime.argvPtrsBase));
  a.i32Add();
  a.i32Load(0);
  a.localSet(Pointer);
  // the argument ends where the next one starts, or at the end of the buffer
  a.localGet(Position);
  a.i32Const(1);
  a.i32Add();
  a.localGet(Count);
  a.i32Eq();
  a.ifThen();
  a.i32Const(static_cast<int32_t>(runtime.argvBufferBase));
  a.localGet(BufferBytes);
  a.i32Add();
  a.localSet(End);
  a.elseArm();
  a.localGet(Position);
  a.i32Const(1);
  a.i32Add();
  a.i32Const(2);
  a.i32Shl();
  a.i32Const(static_cast<int32_t>(runtime.argvPtrsBase));
  a.i32Add();
  a.i32Load(0);
  a.localSet(End);
  a.end();
  // the text of the argument is what comes before its NUL
  a.localGet(End);
  a.localGet(Pointer);
  a.i32Sub();
  a.i32Const(1);
  a.i32Sub();
  a.localSet(Length);
  a.localGet(Length);
  a.i32Eqz();
  a.ifThen();
  a.i64Const(0);
  a.ret();
  a.end();
  a.localGet(Length);
  a.i64ExtendI32U();
  a.i64Const(1);
  a.i64Add();
  a.call(textFn(runtime, WasmTextHelper::Alloc));
  a.localSet(Address);
  a.localGet(Address);
  a.call(byteFn(runtime, WasmByteHelper::Access));
  a.localGet(Pointer);
  a.localGet(Length);
  a.memoryCopy();
  a.localGet(Address);
  a.end();
  return body;
}

} // namespace wasm_text
} // namespace primec
