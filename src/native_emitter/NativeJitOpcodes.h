#pragma once

#include "primec/ir/Ir.h"

namespace primec::native_emitter {

// How the native JIT (primec/backend/NativeJit.h) runs each opcode: inline machine code with the
// VM's exact semantics, or a call into the runtime (VmNativeJitHost), which runs it with the
// VM's own handlers on the operands the code passes it.
inline bool nativeJitRunsOpcodeInline(IrOpcode op) {
  switch (op) {
  case IrOpcode::PushI32:
  case IrOpcode::PushI64:
  case IrOpcode::PushF32:
  case IrOpcode::PushF64:
  case IrOpcode::LoadLocal:
  case IrOpcode::StoreLocal:
  case IrOpcode::AddressOfLocal:
  case IrOpcode::LoadIndirect:
  case IrOpcode::StoreIndirect:
  case IrOpcode::Dup:
  case IrOpcode::Pop:
  case IrOpcode::AddI32:
  case IrOpcode::SubI32:
  case IrOpcode::MulI32:
  case IrOpcode::DivI32:
  case IrOpcode::NegI32:
  case IrOpcode::AddI64:
  case IrOpcode::SubI64:
  case IrOpcode::MulI64:
  case IrOpcode::DivI64:
  case IrOpcode::DivU64:
  case IrOpcode::NegI64:
  case IrOpcode::SextI32:
  case IrOpcode::CmpEqI32:
  case IrOpcode::CmpNeI32:
  case IrOpcode::CmpLtI32:
  case IrOpcode::CmpLeI32:
  case IrOpcode::CmpGtI32:
  case IrOpcode::CmpGeI32:
  case IrOpcode::CmpEqI64:
  case IrOpcode::CmpNeI64:
  case IrOpcode::CmpLtI64:
  case IrOpcode::CmpLeI64:
  case IrOpcode::CmpGtI64:
  case IrOpcode::CmpGeI64:
  case IrOpcode::CmpLtU64:
  case IrOpcode::CmpLeU64:
  case IrOpcode::CmpGtU64:
  case IrOpcode::CmpGeU64:
  case IrOpcode::AddF64:
  case IrOpcode::SubF64:
  case IrOpcode::MulF64:
  case IrOpcode::DivF64:
  case IrOpcode::NegF64:
  case IrOpcode::CmpEqF64:
  case IrOpcode::CmpNeF64:
  case IrOpcode::CmpLtF64:
  case IrOpcode::CmpLeF64:
  case IrOpcode::CmpGtF64:
  case IrOpcode::CmpGeF64:
  case IrOpcode::ConvertI32ToF64:
  case IrOpcode::ConvertI64ToF64:
  case IrOpcode::ConvertU64ToF64:
  case IrOpcode::ConvertF64ToI64:
  case IrOpcode::JumpIfZero:
  case IrOpcode::Jump:
  case IrOpcode::Call:
  case IrOpcode::CallVoid:
  case IrOpcode::ReturnVoid:
  case IrOpcode::ReturnI32:
  case IrOpcode::ReturnI64:
  case IrOpcode::ReturnF32:
  case IrOpcode::ReturnF64:
  case IrOpcode::PrintI32:
  case IrOpcode::PrintI64:
  case IrOpcode::PrintU64:
  case IrOpcode::PrintString:
  case IrOpcode::PushArgc:
  case IrOpcode::LoadStringByte:
  case IrOpcode::LoadStringLength:
    return true;
  default:
    return false;
  }
}

// The opcodes the runtime runs for the JIT code. Host calls are not among them: the JIT takes no
// host bindings, so a module importing host functions stays on the interpreter.
inline bool nativeJitBridgesOpcode(IrOpcode op) {
  switch (op) {
  case IrOpcode::HeapAlloc:
  case IrOpcode::HeapFree:
  case IrOpcode::HeapRealloc:
  case IrOpcode::PrintStringDynamic:
  case IrOpcode::PrintArgv:
  case IrOpcode::PrintArgvUnsafe:
  case IrOpcode::LoadStringByteDynamic:
  case IrOpcode::FileOpenRead:
  case IrOpcode::FileOpenWrite:
  case IrOpcode::FileOpenAppend:
  case IrOpcode::FileOpenReadDynamic:
  case IrOpcode::FileOpenWriteDynamic:
  case IrOpcode::FileOpenAppendDynamic:
  case IrOpcode::FileReadByte:
  case IrOpcode::FileClose:
  case IrOpcode::FileFlush:
  case IrOpcode::FileWriteI32:
  case IrOpcode::FileWriteI64:
  case IrOpcode::FileWriteU64:
  case IrOpcode::FileWriteString:
  case IrOpcode::FileWriteStringDynamic:
  case IrOpcode::FileWriteByte:
  case IrOpcode::FileWriteNewline:
  case IrOpcode::AddF32:
  case IrOpcode::SubF32:
  case IrOpcode::MulF32:
  case IrOpcode::DivF32:
  case IrOpcode::NegF32:
  case IrOpcode::CmpEqF32:
  case IrOpcode::CmpNeF32:
  case IrOpcode::CmpLtF32:
  case IrOpcode::CmpLeF32:
  case IrOpcode::CmpGtF32:
  case IrOpcode::CmpGeF32:
  case IrOpcode::ConvertI32ToF32:
  case IrOpcode::ConvertI64ToF32:
  case IrOpcode::ConvertU64ToF32:
  case IrOpcode::ConvertF32ToI32:
  case IrOpcode::ConvertF32ToI64:
  case IrOpcode::ConvertF32ToU64:
  case IrOpcode::ConvertF64ToI32:
  case IrOpcode::ConvertF64ToU64:
  case IrOpcode::ConvertF32ToF64:
  case IrOpcode::ConvertF64ToF32:
    return true;
  default:
    return false;
  }
}

} // namespace primec::native_emitter
