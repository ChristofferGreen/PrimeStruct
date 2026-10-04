// In-process execution support for the x86_64 emitter (see JitFault in
// NativeEmitterInternalsX64.h and include/primec/backend/NativeJit.h).

inline void X64Emitter::emitJitDataOperand(uint8_t reg, uint32_t field, uint32_t trailing) {
  emitByte(static_cast<uint8_t>(((reg & 0x7) << 3) | 0x5)); // mod=00, rm=101: [rip + disp32]
  jitDataReferences_.push_back({code_.size(), field, trailing});
  emitU32(0);
}

inline void X64Emitter::emitJitFaultIf(CondCode cc, JitFault fault, uint32_t argument) {
  emitByte(0x0F);
  emitByte(static_cast<uint8_t>(0x80 + condCodeValue(cc))); // Jcc rel32
  jitFaultSites_.push_back({code_.size(), fault, argument});
  emitU32(0);
}

inline void
X64Emitter::emitJitFaultIfWithRegister(CondCode cc, JitFault fault, uint8_t argumentRegister) {
  emitJitFaultIf(cc, fault);
  jitFaultSites_.back().argumentRegister = argumentRegister;
}

inline void X64Emitter::emitJitFault(JitFault fault, uint32_t argument) {
  emitByte(0xE9); // jmp rel32
  jitFaultSites_.push_back({code_.size(), fault, argument});
  emitU32(0);
}

inline void X64Emitter::emitJitZeroFrameLocals(uint32_t count) {
  constexpr uint32_t Unrolled = 8;
  if (count <= Unrolled) {
    for (uint32_t k = 0; k < count; ++k) {
      emitStoreImm64Mem(5, regAllocSlotDisp(k), 0);
    }
    return;
  }
  constexpr uint8_t Pointer = 11;
  constexpr uint8_t Counter = 10;
  emitMovRegReg(Pointer, 5);
  emitAddRegImm32(Pointer, regAllocSlotDisp(0));
  emitMovRegImm64(Counter, count);
  const size_t loop = code_.size();
  emitStoreImm64Mem(Pointer, 0, 0);
  emitAddRegImm32(Pointer, static_cast<int32_t>(IrSlotBytes));
  emitSubRegImm32(Counter, 1);
  emitByte(0x0F); // jnz loop
  emitByte(0x85);
  emitU32(static_cast<uint32_t>(static_cast<int32_t>(loop - (code_.size() + 4))));
}

inline void X64Emitter::emitJitEnterCall() {
  // cmp qword [rip + depth], JitMaxCallDepth; jae fault; inc qword [rip + depth]
  emitByte(0x48);
  emitByte(0x81);
  emitJitDataOperand(7, JitDataCallDepth, 4);
  emitU32(static_cast<uint32_t>(JitMaxCallDepth));
  emitJitFaultIf(CondCode::AboveEq, JitFault::CallStackOverflow);
  emitByte(0x48);
  emitByte(0xFF);
  emitJitDataOperand(0, JitDataCallDepth);
}

inline void X64Emitter::emitJitLeaveCall() {
  emitByte(0x48); // dec qword [rip + depth]
  emitByte(0xFF);
  emitJitDataOperand(1, JitDataCallDepth);
}

inline size_t X64Emitter::emitJitRuntime(size_t entryOffset) {
  // Fault stubs: eax = fault, edx = argument, then the common fault exit.
  std::vector<size_t> exitJumps;
  for (const JitFaultSite &site : jitFaultSites_) {
    const int32_t delta = static_cast<int32_t>(code_.size() - (site.position + 4));
    patchU32(site.position, static_cast<uint32_t>(delta));
    if (site.argumentRegister >= 0) {
      emitMovRegReg(2, static_cast<uint8_t>(site.argumentRegister)); // rdx = the value
    } else {
      emitByte(0xBA); // mov edx, imm32
      emitU32(site.argument);
    }
    emitByte(0xB8); // mov eax, imm32
    emitU32(static_cast<uint32_t>(site.fault));
    emitByte(0xE9);
    exitJumps.push_back(code_.size());
    emitU32(0);
  }
  const size_t faultExit = code_.size();
  for (const size_t position : exitJumps) {
    patchU32(position, static_cast<uint32_t>(static_cast<int32_t>(faultExit - (position + 4))));
  }
  emitByte(0x48); // mov [rip + fault], rax
  emitByte(0x89);
  emitJitDataOperand(0, JitDataFaultCode);
  emitByte(0x48); // mov [rip + argument], rdx
  emitByte(0x89);
  emitJitDataOperand(2, JitDataFaultArgument);
  emitByte(0xE9); // jmp restore
  const size_t toRestore = code_.size();
  emitU32(0);

  // uint64_t trampoline(uint64_t argc, char **argv, void *stackTop): saves the callee-saved
  // registers and the host stack pointer, runs the entry function on the JIT stack with argc
  // and argv in r12 and r13 (where the entry captures them), and returns its result.
  const size_t trampoline = code_.size();
  for (const uint8_t reg : {3, 5}) { // rbx, rbp
    emitByte(static_cast<uint8_t>(0x50 + reg));
  }
  for (const uint8_t reg : {12, 13, 14, 15}) {
    emitByte(0x41);
    emitByte(static_cast<uint8_t>(0x50 + (reg & 7)));
  }
  emitByte(0x48); // mov [rip + saved], rsp
  emitByte(0x89);
  emitJitDataOperand(4, JitDataSavedStack);
  emitMovRegReg(4, 2);  // rsp = stackTop
  emitMovRegReg(12, 7); // r12 = argc
  emitMovRegReg(13, 6); // r13 = argv
  emitByte(0xE8);       // call entry
  const size_t callPosition = code_.size();
  emitU32(static_cast<uint32_t>(static_cast<int32_t>(entryOffset - (callPosition + 4))));
  const size_t restore = code_.size();
  patchU32(toRestore, static_cast<uint32_t>(static_cast<int32_t>(restore - (toRestore + 4))));
  emitByte(0x48); // mov rsp, [rip + saved]
  emitByte(0x8B);
  emitJitDataOperand(4, JitDataSavedStack);
  for (const uint8_t reg : {15, 14, 13, 12}) {
    emitByte(0x41);
    emitByte(static_cast<uint8_t>(0x58 + (reg & 7)));
  }
  for (const uint8_t reg : {5, 3}) {
    emitByte(static_cast<uint8_t>(0x58 + reg));
  }
  emitByte(0xC3); // ret
  return trampoline;
}

inline void X64Emitter::patchJitData(std::vector<uint8_t> &image, size_t dataOffset) const {
  for (const JitDataReference &reference : jitDataReferences_) {
    const int64_t delta = static_cast<int64_t>(dataOffset + reference.field) -
                          static_cast<int64_t>(reference.position + 4 + reference.trailing);
    const uint32_t value = static_cast<uint32_t>(static_cast<int32_t>(delta));
    for (size_t i = 0; i < 4; ++i) {
      image[reference.position + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
  }
}
