#include "WasmEmitterInternal.h"

namespace primec {

// Local addresses and indirect loads and stores on the module's linear memory (docs/spec/vm-design.md, Frames & stack).
//
// The VM numbers the locals of all live frames in one sequence: a local address is `(base + index + 1) * 16` bytes,
// where `base` is the number of slots the frames below it hold, so address 0 names nothing and a callee reads and
// writes a caller's local through the address the caller passed. Wasm keeps that numbering over a shadow stack in its
// linear memory: the mutable global `top` is the number of live slots, a function that takes an address (every slot
// of its frame is then reachable) reserves `irLocalCount` slots at entry, keeps its locals there instead of in wasm
// locals, and gives them back at every return. A slot is 8 bytes of memory (the raw 64 bits of the VM's slot); the
// address stride stays 16. `slotAddress` checks an address the way the VM does and turns it into a memory address.

namespace {

constexpr uint8_t WasmOpUnreachable = 0x00;
constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpLocalSet = 0x21;
constexpr uint8_t WasmOpGlobalGet = 0x23;
constexpr uint8_t WasmOpGlobalSet = 0x24;
constexpr uint8_t WasmOpI64Load = 0x29;
constexpr uint8_t WasmOpI64Store = 0x37;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint8_t WasmOpI64Const = 0x42;
constexpr uint8_t WasmOpI64Eqz = 0x50;
constexpr uint8_t WasmOpI64GeU = 0x5a;
constexpr uint8_t WasmOpI32GtU = 0x4b;
constexpr uint8_t WasmOpI32Add = 0x6a;
constexpr uint8_t WasmOpI32Shl = 0x74;
constexpr uint8_t WasmOpI64Sub = 0x7d;
constexpr uint8_t WasmOpI64And = 0x83;
constexpr uint8_t WasmOpI64ShrU = 0x88;
constexpr uint8_t WasmOpI32WrapI64 = 0xa7;
constexpr uint8_t WasmOpI64ExtendI32S = 0xac;
constexpr uint8_t WasmOpI64ExtendI32U = 0xad;
constexpr uint8_t WasmOpI32ReinterpretF32 = 0xbc;
constexpr uint8_t WasmOpI64ReinterpretF64 = 0xbd;
constexpr uint8_t WasmOpF32ReinterpretI32 = 0xbe;
constexpr uint8_t WasmOpF64ReinterpretI64 = 0xbf;
constexpr uint8_t WasmOpI32Eqz = 0x45;
constexpr uint8_t WasmBlockTypeVoid = 0x40;
constexpr uint32_t StderrFd = 2;
constexpr int32_t RuntimeErrorExitCode = 3;
constexpr uint32_t SlotBytes = 8;
constexpr uint32_t SlotAddressStrideShift = 4;

// The locals of the `slotAddress` helper after its parameter (the address): the descriptor for the decimal digits of
// the address in the fault message.
constexpr uint32_t HelperFdLocal = 1;
constexpr uint32_t HelperValueLocal = 2;
constexpr uint32_t HelperRemLocal = 3;
constexpr uint32_t HelperPtrLocal = 4;
constexpr uint32_t HelperLenLocal = 5;
constexpr uint32_t HelperNegLocal = 6;

void appendI32Const(int32_t value, std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(value, out);
}

void appendI64Const(int64_t value, std::vector<uint8_t> &out) {
  out.push_back(WasmOpI64Const);
  appendS64Leb(value, out);
}

// An 8-byte access (alignment 2^3) at a constant offset from the address on the stack.
void appendI64MemArg(uint32_t offset, std::vector<uint8_t> &out) {
  appendU32Leb(3, out);
  appendU32Leb(offset, out);
}

// The runtime error of an invalid address: the VM's message and `proc_exit(3)` on the WASI profile, a trap on the
// browser profile (which has no host function to call).
void appendAddressFault(uint32_t messageAddr,
                        uint32_t messageLength,
                        const WasmRuntimeContext &runtime,
                        std::vector<uint8_t> &out) {
  if (!runtime.hasProcExit) {
    out.push_back(WasmOpUnreachable);
    return;
  }
  appendI32Const(static_cast<int32_t>(StderrFd), out);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(HelperFdLocal, out);
  emitWasiWriteLiteralFromFdLocal(HelperFdLocal, messageAddr, messageLength, runtime, out);
  out.push_back(0x1a); // drop
  out.push_back(WasmOpLocalGet);
  appendU32Leb(0, out);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(HelperValueLocal, out);
  emitWasiWriteDecimalFromI64Local(HelperFdLocal,
                                   HelperValueLocal,
                                   HelperRemLocal,
                                   HelperPtrLocal,
                                   HelperLenLocal,
                                   HelperNegLocal,
                                   false,
                                   runtime,
                                   out);
  out.push_back(0x1a); // drop
  emitWasiWriteLiteralFromFdLocal(HelperFdLocal, runtime.newlineAddr, 1, runtime, out);
  out.push_back(0x1a); // drop
  appendI32Const(RuntimeErrorExitCode, out);
  out.push_back(0x10); // call
  appendU32Leb(runtime.procExitImportIndex, out);
  out.push_back(WasmOpUnreachable);
}

} // namespace

bool wasmFunctionTakesLocalAddress(const IrFunction &function) {
  for (const IrInstruction &inst : function.instructions) {
    if (inst.op == IrOpcode::AddressOfLocal) {
      return true;
    }
  }
  return false;
}

bool wasmModuleUsesLocalMemory(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      if (inst.op == IrOpcode::AddressOfLocal || inst.op == IrOpcode::LoadIndirect ||
          inst.op == IrOpcode::StoreIndirect) {
        return true;
      }
    }
  }
  return false;
}

uint8_t wasmValueTypeByte(uint8_t type) {
  if (type == WasmTypeSlot) {
    return WasmTypeI64;
  }
  return type == WasmTypeUnknown ? WasmTypeI32 : type;
}

// A slot holds 64 raw bits: an `i32` is sign-extended (as in the VM), an `f32` is its 32 bits zero-extended, an `f64`
// and an `i64` are their bits. Converting to a slot widens the value; converting from one picks the bits back.
void emitWasmSlotConversion(uint8_t from, uint8_t to, std::vector<uint8_t> &out) {
  if (from == to || from == WasmTypeUnknown || to == WasmTypeUnknown) {
    return;
  }
  if (to == WasmTypeSlot) {
    switch (from) {
    case WasmTypeI32:
      out.push_back(WasmOpI64ExtendI32S);
      break;
    case WasmTypeF32:
      out.push_back(WasmOpI32ReinterpretF32);
      out.push_back(WasmOpI64ExtendI32U);
      break;
    case WasmTypeF64:
      out.push_back(WasmOpI64ReinterpretF64);
      break;
    default:
      break;
    }
    return;
  }
  switch (to) {
  case WasmTypeI32:
    out.push_back(WasmOpI32WrapI64);
    break;
  case WasmTypeF32:
    out.push_back(WasmOpI32WrapI64);
    out.push_back(WasmOpF32ReinterpretI32);
    break;
  case WasmTypeF64:
    out.push_back(WasmOpF64ReinterpretI64);
    break;
  default:
    break;
  }
}

void emitWasmFramePrologue(const WasmLocalLayout &layout,
                           const WasmRuntimeContext &runtime,
                           std::vector<uint8_t> &out) {
  const auto liveSlotsAfterFrame = [&]() {
    out.push_back(WasmOpLocalGet);
    appendU32Leb(layout.frameBaseLocal, out);
    appendI32Const(static_cast<int32_t>(layout.irLocalCount), out);
    out.push_back(WasmOpI32Add);
  };
  out.push_back(WasmOpGlobalGet);
  appendU32Leb(0, out);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(layout.frameBaseLocal, out);
  // A frame that does not fit the shadow stack ends the program with a wasm trap (the VM's call-stack fault has no
  // message channel here yet).
  liveSlotsAfterFrame();
  appendI32Const(static_cast<int32_t>(runtime.shadowSlots), out);
  out.push_back(WasmOpI32GtU);
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  out.push_back(WasmOpUnreachable);
  out.push_back(WasmOpEnd);
  liveSlotsAfterFrame();
  out.push_back(WasmOpGlobalSet);
  appendU32Leb(0, out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(layout.frameBaseLocal, out);
  appendI32Const(3, out);
  out.push_back(WasmOpI32Shl);
  appendI32Const(static_cast<int32_t>(runtime.shadowBase), out);
  out.push_back(WasmOpI32Add);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(layout.framePtrLocal, out);
  // The parameters arrive in wasm locals (the leading `StoreLocal`s of the IR are skipped): they go to their slots.
  for (uint32_t parameter = 0; parameter < layout.parameterCount; ++parameter) {
    out.push_back(WasmOpLocalGet);
    appendU32Leb(layout.framePtrLocal, out);
    out.push_back(WasmOpLocalGet);
    appendU32Leb(parameter, out);
    emitWasmSlotConversion(layout.localTypes[parameter], WasmTypeSlot, out);
    out.push_back(WasmOpI64Store);
    appendI64MemArg(parameter * SlotBytes, out);
  }
}

void emitWasmFrameEpilogue(const WasmLocalLayout &layout, std::vector<uint8_t> &out) {
  out.push_back(WasmOpLocalGet);
  appendU32Leb(layout.frameBaseLocal, out);
  out.push_back(WasmOpGlobalSet);
  appendU32Leb(0, out);
}

void emitWasmFrameLoad(uint64_t slot, const WasmLocalLayout &layout, std::vector<uint8_t> &out) {
  out.push_back(WasmOpLocalGet);
  appendU32Leb(layout.framePtrLocal, out);
  out.push_back(WasmOpI64Load);
  appendI64MemArg(static_cast<uint32_t>(slot * SlotBytes), out);
}

// Stores the raw slot value held in `valueLocal` (an i64 wasm local) into the slot.
void emitWasmFrameStore(uint64_t slot,
                        uint32_t valueLocal,
                        const WasmLocalLayout &layout,
                        std::vector<uint8_t> &out) {
  out.push_back(WasmOpLocalGet);
  appendU32Leb(layout.framePtrLocal, out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(valueLocal, out);
  out.push_back(WasmOpI64Store);
  appendI64MemArg(static_cast<uint32_t>(slot * SlotBytes), out);
}

void emitWasmAddressOfLocal(uint64_t slot,
                            const WasmLocalLayout &layout,
                            std::vector<uint8_t> &out) {
  out.push_back(WasmOpLocalGet);
  appendU32Leb(layout.frameBaseLocal, out);
  appendI32Const(static_cast<int32_t>(slot + 1), out);
  out.push_back(WasmOpI32Add);
  appendI32Const(static_cast<int32_t>(SlotAddressStrideShift), out);
  out.push_back(WasmOpI32Shl);
  out.push_back(WasmOpI64ExtendI32U);
}

void emitWasmSlotAddressCall(const WasmRuntimeContext &runtime, std::vector<uint8_t> &out) {
  out.push_back(0x10); // call
  appendU32Leb(runtime.slotAddressFunction, out);
}

void emitWasmIndirectLoad(const WasmRuntimeContext &runtime, std::vector<uint8_t> &out) {
  emitWasmSlotAddressCall(runtime, out);
  out.push_back(WasmOpI64Load);
  appendI64MemArg(0, out);
}

void emitWasmIndirectStore(uint32_t valueLocal,
                           const WasmRuntimeContext &runtime,
                           std::vector<uint8_t> &out) {
  emitWasmSlotAddressCall(runtime, out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(valueLocal, out);
  out.push_back(WasmOpI64Store);
  appendI64MemArg(0, out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(valueLocal, out);
}

// (i64 address) -> i32 memory address of the slot. The VM's faults, in its order: an address that is not a multiple
// of the 16-byte stride is unaligned; address 0, an address beyond the live slots and a heap address (bit 63) name no
// local slot.
WasmCodeBody buildWasmSlotAddressHelperBody(const WasmRuntimeContext &runtime) {
  WasmCodeBody body;
  // fd; value, rem; ptr, len, neg (after the address parameter).
  appendU32Leb(3, body.localDecls);
  appendU32Leb(1, body.localDecls);
  body.localDecls.push_back(WasmTypeI32);
  appendU32Leb(2, body.localDecls);
  body.localDecls.push_back(WasmTypeI64);
  appendU32Leb(3, body.localDecls);
  body.localDecls.push_back(WasmTypeI32);
  std::vector<uint8_t> &out = body.instructions;

  out.push_back(WasmOpLocalGet);
  appendU32Leb(0, out);
  appendI64Const((int64_t{1} << SlotAddressStrideShift) - 1, out);
  out.push_back(WasmOpI64And);
  out.push_back(WasmOpI64Eqz);
  out.push_back(WasmOpI32Eqz);
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  appendAddressFault(runtime.unalignedMessageAddr, runtime.unalignedMessageLength, runtime, out);
  out.push_back(WasmOpEnd);

  // (address / 16 - 1) >= top, unsigned: address 0 wraps to the largest value.
  out.push_back(WasmOpLocalGet);
  appendU32Leb(0, out);
  appendI64Const(SlotAddressStrideShift, out);
  out.push_back(WasmOpI64ShrU);
  appendI64Const(1, out);
  out.push_back(WasmOpI64Sub);
  out.push_back(WasmOpGlobalGet);
  appendU32Leb(0, out);
  out.push_back(WasmOpI64ExtendI32U);
  out.push_back(WasmOpI64GeU);
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  appendAddressFault(runtime.invalidMessageAddr, runtime.invalidMessageLength, runtime, out);
  out.push_back(WasmOpEnd);

  // shadowBase + 8 * (address / 16 - 1) = shadowBase - 8 + address / 2
  out.push_back(WasmOpLocalGet);
  appendU32Leb(0, out);
  appendI64Const(1, out);
  out.push_back(WasmOpI64ShrU);
  out.push_back(WasmOpI32WrapI64);
  appendI32Const(static_cast<int32_t>(runtime.shadowBase) - static_cast<int32_t>(SlotBytes), out);
  out.push_back(WasmOpI32Add);
  out.push_back(WasmOpEnd);
  return body;
}

} // namespace primec
