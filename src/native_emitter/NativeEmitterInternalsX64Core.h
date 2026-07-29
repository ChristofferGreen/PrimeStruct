// x86_64 low-level instruction encoding for X64Emitter. Included from
// inside NativeEmitterInternalsX64.h, after the class body, mirroring how
// NativeEmitterInternalsArm64Core.h attaches to Arm64Emitter.
//
// Register indices 0-15 follow the standard x86_64 encoding order:
// rax=0 rcx=1 rdx=2 rbx=3 rsp=4 rbp=5 rsi=6 rdi=7 r8=8 ... r15=15.
//
// Register roles (see NativeEmitterInternalsX64.h's class comment and
// docs/todo.md's TODO-4747 x86_64 entry for the full rationale):
//   rbp  - frame base, standard push-rbp/mov-rbp,rsp/sub-rsp prologue,
//          paired with call/ret (no manual link-register save/restore -
//          x86_64's call/ret already handle the return address on the
//          real stack, unlike ARM64's bl/ret).
//   r15  - value-stack pointer (mirrors ARM64's x28); starts at rbp for
//          the entry function only (the same "top of the allocated
//          frame, growing downward within the extra headroom" position
//          ARM64's x28 starts at) and is left untouched by every other
//          function's prologue, so it persists correctly across calls.
//   r14  - value-stack top-of-stack cache register (mirrors x26).
//   r12/r13 - saved argc/argv (mirrors x19/x20).

inline void X64Emitter::emitByte(uint8_t byte) {
  code_.push_back(byte);
}

inline void X64Emitter::emitU32(uint32_t value) {
  emitByte(static_cast<uint8_t>(value & 0xFF));
  emitByte(static_cast<uint8_t>((value >> 8) & 0xFF));
  emitByte(static_cast<uint8_t>((value >> 16) & 0xFF));
  emitByte(static_cast<uint8_t>((value >> 24) & 0xFF));
}

inline void X64Emitter::emitU64(uint64_t value) {
  emitU32(static_cast<uint32_t>(value & 0xFFFFFFFFull));
  emitU32(static_cast<uint32_t>((value >> 32) & 0xFFFFFFFFull));
}

inline void X64Emitter::patchByte(size_t index, uint8_t byte) {
  if (index < code_.size()) {
    code_[index] = byte;
  }
}

inline void X64Emitter::patchU32(size_t index, uint32_t value) {
  patchByte(index, static_cast<uint8_t>(value & 0xFF));
  patchByte(index + 1, static_cast<uint8_t>((value >> 8) & 0xFF));
  patchByte(index + 2, static_cast<uint8_t>((value >> 16) & 0xFF));
  patchByte(index + 3, static_cast<uint8_t>((value >> 24) & 0xFF));
}

inline void X64Emitter::emitRex(bool w, uint8_t reg, uint8_t rmOrBase) {
  const uint8_t rBit = (reg >> 3) & 0x1;
  const uint8_t bBit = (rmOrBase >> 3) & 0x1;
  const uint8_t rex = 0x40 | (w ? 0x08 : 0) | (rBit << 2) | bBit;
  emitByte(rex);
}

inline void X64Emitter::emitModRmReg(uint8_t reg, uint8_t rm) {
  const uint8_t modrm = 0xC0 | ((reg & 0x7) << 3) | (rm & 0x7);
  emitByte(modrm);
}

inline void X64Emitter::emitModRmBaseDisp32(uint8_t reg, uint8_t base, int32_t disp) {
  // mod=10 (disp32 follows). `base` must not be rsp/r12 (low3==100) - this
  // backend only ever addresses memory via rbp or r15, neither of which
  // has that low-3-bits value, so no SIB byte is ever required here.
  const uint8_t modrm = 0x80 | ((reg & 0x7) << 3) | (base & 0x7);
  emitByte(modrm);
  emitU32(static_cast<uint32_t>(disp));
}

inline size_t X64Emitter::emitModRmRipRelPlaceholder(uint8_t reg) {
  // mod=00, rm=101 is the reserved encoding for [RIP + disp32] in 64-bit
  // mode (there is no "base register 5, no displacement" form - mod=00
  // with rm=101 always means RIP-relative).
  const uint8_t modrm = 0x00 | ((reg & 0x7) << 3) | 0x5;
  emitByte(modrm);
  const size_t fixupIndex = code_.size();
  emitU32(0);
  return fixupIndex;
}

inline void X64Emitter::emitMovRegImm64(uint8_t rd, uint64_t imm) {
  emitRex(true, 0, rd);
  emitByte(static_cast<uint8_t>(0xB8 + (rd & 0x7)));
  emitU64(imm);
}

inline void X64Emitter::emitMovRegReg(uint8_t rd, uint8_t rs) {
  // mov r/m64, r64 (opcode 0x89): rd is the destination (r/m slot), rs is
  // the source (reg slot) - REX fields must match that role assignment.
  emitRex(true, rs, rd);
  emitByte(0x89);
  emitModRmReg(rs, rd);
}

inline void X64Emitter::emitLoadMem(uint8_t rd, uint8_t base, int32_t disp) {
  emitRex(true, rd, base);
  emitByte(0x8B); // mov r64, r/m64
  emitModRmBaseDisp32(rd, base, disp);
}

inline void X64Emitter::emitStoreMem(uint8_t base, int32_t disp, uint8_t rs) {
  emitRex(true, rs, base);
  emitByte(0x89); // mov r/m64, r64
  emitModRmBaseDisp32(rs, base, disp);
}

inline void X64Emitter::emitAddRegImm32(uint8_t rd, int32_t imm) {
  emitRex(true, 0, rd);
  emitByte(0x81);
  emitByte(static_cast<uint8_t>(0xC0 | (rd & 0x7))); // /0 = add
  emitU32(static_cast<uint32_t>(imm));
}

inline void X64Emitter::emitSubRegImm32(uint8_t rd, int32_t imm) {
  emitRex(true, 0, rd);
  emitByte(0x81);
  emitByte(static_cast<uint8_t>(0xE8 | (rd & 0x7))); // /5 = sub
  emitU32(static_cast<uint32_t>(imm));
}

inline void X64Emitter::emitPushReg64(uint8_t reg) {
  if (reg >= 8) {
    emitByte(0x41); // REX.B only
  }
  emitByte(static_cast<uint8_t>(0x50 + (reg & 0x7)));
}

inline void X64Emitter::emitPopReg64(uint8_t reg) {
  if (reg >= 8) {
    emitByte(0x41);
  }
  emitByte(static_cast<uint8_t>(0x58 + (reg & 0x7)));
}

inline void X64Emitter::emitSyscall() {
  emitByte(0x0F);
  emitByte(0x05);
}

inline void X64Emitter::emitRet() {
  emitByte(0xC3);
}

inline uint64_t X64Emitter::localOffset(uint32_t index) {
  return static_cast<uint64_t>(index) * 16u + 8u;
}

inline void X64Emitter::emitSpillReg(uint8_t reg) {
  counters_.spillCount += 1;
  emitSubRegImm32(15, 16); // sub r15, 16
  emitStoreMem(15, 8, reg); // mov [r15+8], reg
}

inline void X64Emitter::emitReloadReg(uint8_t reg) {
  counters_.reloadCount += 1;
  emitLoadMem(reg, 15, 8); // mov reg, [r15+8]
  emitAddRegImm32(15, 16); // add r15, 16
}

inline void X64Emitter::flushValueStackCache() {
  if (!hasValueStackCache_) {
    return;
  }
  emitSpillReg(valueStackCacheReg_);
  hasValueStackCache_ = false;
}

inline void X64Emitter::emitPushReg(uint8_t reg) {
  counters_.valueStackPushCount += 1;
  if (!valueStackCacheEnabled_) {
    emitSpillReg(reg);
    return;
  }
  if (hasValueStackCache_) {
    emitSpillReg(valueStackCacheReg_);
  }
  if (reg != valueStackCacheReg_) {
    emitMovRegReg(valueStackCacheReg_, reg);
  }
  hasValueStackCache_ = true;
}

inline void X64Emitter::emitPopReg(uint8_t reg) {
  counters_.valueStackPopCount += 1;
  if (!valueStackCacheEnabled_) {
    emitReloadReg(reg);
    return;
  }
  if (hasValueStackCache_) {
    if (reg != valueStackCacheReg_) {
      emitMovRegReg(reg, valueStackCacheReg_);
    }
    hasValueStackCache_ = false;
    return;
  }
  emitReloadReg(reg);
}

inline bool X64Emitter::beginFunction(uint64_t frameSize, bool resetValueStack, std::string &error) {
  if (frameSize > static_cast<uint64_t>(INT32_MAX)) {
    error = "native backend frame size exceeds 32-bit immediate range";
    return false;
  }
  hasValueStackCache_ = false;
  frameSize_ = frameSize;
  // `resetValueStack` is only ever passed true for the entry function
  // (NativeEmitterFunctionEmit.cpp: `beginFunction(frameSize,
  // isEntryFunction, error)`) - reuse it as this backend's own
  // "is this the entry function" flag, since a raw ELF entry point needs
  // a different Return* lowering than an ordinary called function (see
  // isEntryFunction_'s declaration comment).
  isEntryFunction_ = resetValueStack;
  emitPushReg64(5); // push rbp
  emitMovRegReg(5, 4); // mov rbp, rsp
  if (frameSize_ > 0) {
    emitSubRegImm32(4, static_cast<int32_t>(frameSize_)); // sub rsp, frameSize
  }
  if (resetValueStack) {
    emitMovRegReg(15, 5); // r15 = rbp (top of this frame's allocated region)
  }
  return true;
}

inline void X64Emitter::emitCaptureEntryArgs() {
  // Raw Linux ELF _start receives no register arguments - argc is at
  // [rsp] and argv[0] at [rsp+8] on the *initial* process stack, per the
  // SysV ABI. This must run before beginFunction's push/sub touch rsp;
  // NativeEmitterFunctionEmit.cpp calls emitCaptureEntryArgs() after
  // beginFunction for the ARM64 backend (where argc/argv arrive in
  // registers from dyld, unaffected by the frame setup), so this method
  // reads them relative to rbp instead: beginFunction has already done
  // `push rbp; mov rbp, rsp`, so the original entry-time rsp is exactly
  // `rbp + 8` (the pushed rbp occupies [rbp], the return slot below that
  // doesn't exist for _start, so argc lives at [rbp + 8], argv at
  // [rbp + 16]) - _start has no return address on the stack the way a
  // `call`ed function would, only whatever `push rbp` itself added.
  emitLoadMem(12, 5, 8);  // r12 (argc) = [rbp + 8]
  // r13 (argv) must be the ARRAY BASE ADDRESS (rbp+16 itself), not the
  // pointer stored there - that value is argv[0] (a string pointer), not
  // a separate "argv" pointer. The SysV ABI lays argv[] inline on the
  // initial stack right after argc; there is no indirection to load
  // through, only an address to compute.
  emitMovRegReg(13, 5);
  emitAddRegImm32(13, 16); // r13 = rbp + 16 = &argv[0]
}

inline void X64Emitter::emitMovRegPublic(uint8_t rd, uint8_t rn) {
  emitMovRegReg(rd, rn);
}

inline void X64Emitter::emitPushI32(int32_t value) {
  emitMovRegImm64(0, static_cast<uint64_t>(static_cast<int64_t>(value)));
  emitPushReg(0);
}

inline void X64Emitter::emitPushI64(uint64_t value) {
  emitMovRegImm64(0, value);
  emitPushReg(0);
}

inline void X64Emitter::emitPushF32(uint32_t bits) {
  emitMovRegImm64(0, bits);
  emitPushReg(0);
}

inline void X64Emitter::emitPushF64(uint64_t bits) {
  emitMovRegImm64(0, bits);
  emitPushReg(0);
}

inline void X64Emitter::emitLoadLocalToReg(uint8_t reg, uint32_t index) {
  emitLoadMem(reg, 5, -static_cast<int32_t>(localOffset(index)));
}

inline void X64Emitter::emitLoadLocal(uint32_t index) {
  emitLoadLocalToReg(0, index);
  emitPushReg(0);
}

inline void X64Emitter::emitAddressOfLocal(uint32_t index) {
  emitMovRegReg(0, 5); // mov rax, rbp
  emitSubRegImm32(0, static_cast<int32_t>(localOffset(index)));
  emitPushReg(0);
}

inline void X64Emitter::emitStoreLocalFromReg(uint32_t index, uint8_t reg) {
  emitStoreMem(5, -static_cast<int32_t>(localOffset(index)), reg);
}

inline void X64Emitter::emitStoreLocal(uint32_t index) {
  emitPopReg(0);
  emitStoreLocalFromReg(index, 0);
}

inline void X64Emitter::emitLoadIndirect() {
  emitPopReg(0);
  emitLoadMem(1, 0, 0);
  emitPushReg(1);
}

inline void X64Emitter::emitStoreIndirect() {
  emitPopReg(0);
  emitPopReg(1);
  emitStoreMem(1, 0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitPushReg0() {
  emitPushReg(0);
}

inline void X64Emitter::emitDup() {
  emitPopReg(0);
  emitPushReg(0);
  emitPushReg(0);
}

inline void X64Emitter::emitPop() {
  emitPopReg(0);
}

inline void X64Emitter::emitLoadMemByte(uint8_t rd, uint8_t base, int32_t disp) {
  emitRex(true, rd, base);
  emitByte(0x0F);
  emitByte(0xB6); // MOVZX r64, r/m8
  emitModRmBaseDisp32(rd, base, disp);
}

inline void X64Emitter::emitStoreMemByte(uint8_t base, int32_t disp, uint8_t rs) {
  // Always emit a REX prefix (even with no bits set) so rsp/rbp/rsi/rdi's
  // low-byte forms are addressable here - see emitSetccReg's identical
  // rationale in NativeEmitterInternalsX64Arithmetic.h.
  emitRex(false, rs, base);
  emitByte(0x88); // MOV r/m8, r8
  emitModRmBaseDisp32(rs, base, disp);
}

inline void X64Emitter::emitCmpRegImm32(uint8_t reg, int32_t imm) {
  emitRex(true, 0, reg);
  emitByte(0x81);
  emitByte(static_cast<uint8_t>(0xF8 | (reg & 0x7))); // /7 = CMP
  emitU32(static_cast<uint32_t>(imm));
}

inline void X64Emitter::emitLoadFrameOffset(uint8_t rd, uint32_t offsetBytes) {
  emitMovRegReg(rd, 5); // rd = rbp
  emitSubRegImm32(rd, static_cast<int32_t>(offsetBytes));
}

inline size_t X64Emitter::emitLeaRipPlaceholder(uint8_t rd) {
  emitRex(true, rd, 0);
  emitByte(0x8D); // LEA r64, [rip+disp32]
  return emitModRmRipRelPlaceholder(rd);
}

inline void X64Emitter::emitWriteSyscall(uint64_t fd, uint8_t bufferReg, uint8_t lengthReg) {
  emitMovRegImm64(7, fd);          // rdi = fd
  emitMovRegReg(6, bufferReg);     // rsi = buffer
  emitMovRegReg(2, lengthReg);     // rdx = length
  emitMovRegImm64(0, LinuxSysWrite);
  emitSyscall();
}

inline void X64Emitter::emitWriteSyscallReg(uint8_t fdReg, uint8_t bufferReg, uint8_t lengthReg) {
  emitMovRegReg(7, fdReg);
  emitMovRegReg(6, bufferReg);
  emitMovRegReg(2, lengthReg);
  emitMovRegImm64(0, LinuxSysWrite);
  emitSyscall();
}

inline void X64Emitter::emitReadSyscallReg(uint8_t fdReg, uint8_t bufferReg, uint8_t lengthReg) {
  emitMovRegReg(7, fdReg);
  emitMovRegReg(6, bufferReg);
  emitMovRegReg(2, lengthReg);
  emitMovRegImm64(0, LinuxSysRead);
  emitSyscall();
}

inline void X64Emitter::emitWriteNewline(uint64_t fd, uint32_t scratchOffset) {
  emitLoadFrameOffset(1, scratchOffset + 1);
  emitMovRegImm64(3, '\n');
  emitStoreMemByte(1, 0, 3);
  emitMovRegImm64(2, 1);
  emitWriteSyscall(fd, 1, 2);
}

inline void X64Emitter::emitWriteNewlineReg(uint8_t fdReg, uint32_t scratchOffset) {
  emitLoadFrameOffset(1, scratchOffset + 1);
  emitMovRegImm64(3, '\n');
  emitStoreMemByte(1, 0, 3);
  emitMovRegImm64(2, 1);
  emitWriteSyscallReg(fdReg, 1, 2);
}

// Heap allocation layout mirrors Arm64Emitter's exactly: a 16-byte header
// (magic + mmap length in bytes) precedes the returned pointer, which
// points IrSlotBytes into the mmap'd region.
//
// Register-lifetime note: the raw `syscall` instruction clobbers rax,
// rcx, and r11, and every other free GPR this backend has doubles as a
// syscall argument register (rdi/rsi/rdx/r10/r8/r9) - so nothing survives
// a syscall in a register except rbx (register 3), which this backend
// never uses for syscall arguments. Values that must survive a syscall
// and don't fit in rbx are saved via emitPushReg/emitPopReg (the real,
// memory-backed value stack) instead - see emitHeapRealloc below.
inline void X64Emitter::emitHeapAllocFromSlotCountReg(uint8_t slotCountReg, uint8_t resultReg) {
  emitCmpRegImm32(slotCountReg, 0);
  const size_t jumpNonZeroSlots = emitCondJumpPlaceholder(CondCode::Ne);
  emitMovRegImm64(resultReg, 0);
  const size_t jumpDone = emitJumpPlaceholderRaw();

  patchCondJumpHere(jumpNonZeroSlots);
  emitMovRegReg(3, slotCountReg); // rbx = mmap length = (slotCount+1)*IrSlotBytes
  emitAddRegImm32(3, 1);
  emitMovRegImm64(2, IrSlotBytes);
  emitImulRegReg(3, 2);

  emitMovRegImm64(7, 0);                          // rdi = addr (NULL)
  emitMovRegReg(6, 3);                            // rsi = length
  emitMovRegImm64(2, LinuxMmapProtReadWrite);     // rdx = prot
  emitMovRegImm64(10, LinuxMmapFlagsPrivateAnon); // r10 = flags
  emitMovRegImm64(8, ~0ull);                      // r8 = fd (-1)
  emitMovRegImm64(9, 0);                          // r9 = offset
  emitMovRegImm64(0, LinuxSysMmap);
  emitSyscall();

  emitCmpRegImm32(0, 0);
  const size_t jumpNonNegative = emitCondJumpPlaceholder(CondCode::Ge);
  emitMovRegImm64(resultReg, 0);
  const size_t jumpDoneAfterFailure = emitJumpPlaceholderRaw();

  patchCondJumpHere(jumpNonNegative);
  emitMovRegImm64(2, HeapHeaderMagic);
  emitStoreMem(0, 0, 2);
  emitStoreMem(0, 8, 3); // rbx (mmap length) survived the syscall
  emitMovRegImm64(2, IrSlotBytes);
  emitMovRegReg(resultReg, 0);
  emitAddRegReg(resultReg, 2);

  patchJumpHere(jumpDoneAfterFailure);
  patchJumpHere(jumpDone);
}

inline void X64Emitter::emitHeapFreeFromAddressReg(uint8_t addressReg) {
  emitCmpRegImm32(addressReg, 0);
  const size_t jumpDone = emitCondJumpPlaceholder(CondCode::Eq);

  emitMovRegReg(0, addressReg);
  emitSubRegImm32(0, IrSlotBytes);
  emitLoadMem(1, 0, 0);
  emitMovRegImm64(2, HeapHeaderMagic);
  emitCmpRegReg(1, 2);
  const size_t jumpSkip = emitCondJumpPlaceholder(CondCode::Ne);
  emitLoadMem(6, 0, 8); // rsi = mmap length (read before rax is overwritten below)
  emitMovRegReg(7, 0);  // rdi = mmap base
  emitMovRegImm64(0, LinuxSysMunmap);
  emitSyscall();

  patchCondJumpHere(jumpSkip);
  patchCondJumpHere(jumpDone);
}

inline void X64Emitter::emitHeapAlloc() {
  emitPopReg(1);
  emitHeapAllocFromSlotCountReg(1, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitHeapFree() {
  emitPopReg(0);
  emitHeapFreeFromAddressReg(0);
}

inline void X64Emitter::emitHeapRealloc() {
  emitPopReg(1); // reg1 = newSlotCount
  emitPopReg(0); // reg0 = oldAddr
  emitCmpRegImm32(0, 0);
  const size_t jumpOldNonNull = emitCondJumpPlaceholder(CondCode::Ne);
  emitHeapAllocFromSlotCountReg(1, 0);
  emitPushReg(0);
  const size_t jumpDone = emitJumpPlaceholderRaw();

  patchCondJumpHere(jumpOldNonNull);
  emitCmpRegImm32(1, 0);
  const size_t jumpNonZeroSlots = emitCondJumpPlaceholder(CondCode::Ne);
  emitHeapFreeFromAddressReg(0);
  emitMovRegImm64(0, 0);
  emitPushReg(0);
  const size_t jumpDoneAfterFree = emitJumpPlaceholderRaw();

  patchCondJumpHere(jumpNonZeroSlots);
  // reg0 = oldAddr, reg1 = newSlotCount. Validate the header at oldAddr-16.
  emitMovRegReg(2, 0);
  emitSubRegImm32(2, IrSlotBytes); // reg2 = mmap base
  emitLoadMem(3, 2, 0);            // reg3 = magic
  emitMovRegImm64(6, HeapHeaderMagic);
  emitCmpRegReg(3, 6);
  const size_t jumpInvalidAddress = emitCondJumpPlaceholder(CondCode::Ne);
  emitLoadMem(3, 2, 8); // reg3 = old mmap length

  // reg1 (newSlotCount) survives emitHeapAllocFromSlotCountReg untouched
  // (it only ever reads its slotCountReg parameter); oldAddr and
  // oldMmapLength don't survive the mmap syscall inside that call, so
  // save them on the real value stack first.
  emitPushReg(0); // save oldAddr
  emitPushReg(3); // save oldMmapLength
  emitHeapAllocFromSlotCountReg(1, 0); // reg0 = newPtr or 0
  emitCmpRegImm32(0, 0);
  const size_t jumpAllocSucceeded = emitCondJumpPlaceholder(CondCode::Ne);
  // Alloc failed: discard the 2 saved temporaries, return null.
  emitPopReg(3);
  emitPopReg(3);
  emitPushReg(0);
  const size_t jumpDoneAfterAllocFailure = emitJumpPlaceholderRaw();

  patchCondJumpHere(jumpAllocSucceeded);
  emitMovRegReg(2, 0); // reg2 = newPtr
  emitPopReg(3);       // reg3 = oldMmapLength (LIFO: pushed last, popped first)
  emitPopReg(0);       // reg0 = oldAddr
  // reg0=oldAddr, reg1=newSlotCount, reg2=newPtr, reg3=oldMmapLength - move
  // oldAddr/newPtr somewhere that survives the division below (which uses
  // rax/rdx) and, for oldAddr specifically, somewhere that survives being
  // used as a mutable copy-loop cursor too.
  emitMovRegReg(8, 0); // r8 = oldAddr (preserved)
  emitMovRegReg(9, 2); // r9 = newPtr (preserved)

  emitMovRegReg(0, 3);              // rax = oldMmapLength
  emitMovRegImm64(10, IrSlotBytes); // r10 = divisor
  emitMovRegImm64(2, 0);            // rdx = 0
  emitDivReg(10);
  emitSubRegImm32(0, 1); // rax = oldSlotCount
  emitCmpRegReg(0, 1);   // oldSlotCount - newSlotCount
  const size_t jumpKeepOldSlots = emitCondJumpPlaceholder(CondCode::BelowEq);
  emitMovRegReg(0, 1); // oldSlotCount > newSlotCount: copy count = newSlotCount
  patchCondJumpHere(jumpKeepOldSlots);
  // rax = slots to copy = min(oldSlotCount, newSlotCount).
  emitCmpRegImm32(0, 0);
  const size_t jumpSkipCopy = emitCondJumpPlaceholder(CondCode::Eq);
  emitMovRegImm64(10, IrSlotBytes);
  emitImulRegReg(0, 10); // rax = bytes to copy
  emitMovRegReg(6, 8);   // rsi = src cursor (copy of oldAddr)
  emitMovRegReg(7, 9);   // rdi = dst cursor (copy of newPtr)
  {
    const size_t copyLoop = code_.size();
    emitLoadMem(2, 6, 0);
    emitStoreMem(7, 0, 2);
    emitAddRegImm32(6, 8);
    emitAddRegImm32(7, 8);
    emitSubRegImm32(0, 8);
    emitCmpRegImm32(0, 0);
    const size_t jumpCopyLoop = emitCondJumpPlaceholder(CondCode::Ne);
    patchU32(jumpCopyLoop,
             static_cast<uint32_t>(static_cast<int32_t>(copyLoop) - static_cast<int32_t>(jumpCopyLoop + 4)));
  }

  patchCondJumpHere(jumpSkipCopy);
  // munmap(oldAddr - IrSlotBytes, oldMmapLength), using the preserved r8
  // (untouched by the copy loop, which only mutated rsi/rdi's copies)
  // and reg3 (oldMmapLength, untouched since the division read it into
  // rax before clobbering anything).
  emitMovRegReg(7, 8);
  emitSubRegImm32(7, IrSlotBytes); // rdi = mmap base
  emitMovRegReg(6, 3);             // rsi = oldMmapLength
  emitMovRegImm64(0, LinuxSysMunmap);
  emitSyscall();
  emitMovRegReg(0, 9); // rax = newPtr (r9 survives: munmap never touches it)
  emitPushReg(0);
  const size_t jumpDoneAfterCopy = emitJumpPlaceholderRaw();

  patchCondJumpHere(jumpInvalidAddress);
  emitMovRegImm64(0, 0);
  emitPushReg(0);

  patchJumpHere(jumpDoneAfterAllocFailure);
  patchJumpHere(jumpDoneAfterCopy);
  patchJumpHere(jumpDoneAfterFree);
  patchJumpHere(jumpDone);
}

inline void X64Emitter::emitExitSyscall() {
  // exit_group(rdi = rax) - terminates the whole (single-threaded)
  // process, matching what a real _start uses to end main(); the kernel
  // truncates the exit code to 8 bits regardless of width.
  emitMovRegReg(7, 0);                    // mov rdi, rax
  emitMovRegImm64(0, LinuxSysExitGroup);  // mov rax, __NR_exit_group
  emitSyscall();
}

inline void X64Emitter::emitReturn() {
  emitPopReg(0);
  if (isEntryFunction_) {
    emitExitSyscall();
    return;
  }
  emitMovRegReg(4, 5); // mov rsp, rbp
  emitPopReg64(5);     // pop rbp
  emitRet();
}

inline void X64Emitter::emitReturnVoid() {
  flushValueStackCache();
  emitMovRegImm64(0, 0);
  if (isEntryFunction_) {
    emitExitSyscall();
    return;
  }
  emitMovRegReg(4, 5);
  emitPopReg64(5);
  emitRet();
}

inline void X64Emitter::emitReturnWithLink(uint32_t linkLocalIndex) {
  (void)linkLocalIndex; // x86_64's call/ret manage the return address on
                        // the real stack automatically - no manual
                        // link-register slot needed (see class header).
  emitReturn();
}

inline void X64Emitter::emitReturnVoidWithLink(uint32_t linkLocalIndex) {
  (void)linkLocalIndex;
  emitReturnVoid();
}

inline void X64Emitter::emitReturnWithFrameAndLink(uint32_t frameLocalIndex, uint32_t linkLocalIndex) {
  (void)frameLocalIndex; // caller's rbp is restored via `pop rbp` below,
                         // not from a saved-locals slot - see class header.
  (void)linkLocalIndex;
  emitReturn();
}

inline void X64Emitter::emitReturnVoidWithFrameAndLink(uint32_t frameLocalIndex, uint32_t linkLocalIndex) {
  (void)frameLocalIndex;
  (void)linkLocalIndex;
  emitReturnVoid();
}

inline size_t X64Emitter::emitJumpPlaceholder() {
  emitByte(0xE9); // jmp rel32
  const size_t fixupIndex = code_.size();
  emitU32(0);
  return fixupIndex;
}

inline size_t X64Emitter::emitCallPlaceholder() {
  emitByte(0xE8); // call rel32
  const size_t fixupIndex = code_.size();
  emitU32(0);
  return fixupIndex;
}

inline size_t X64Emitter::emitJumpIfZeroPlaceholder() {
  // Condition (0 or 1) is popped into rax and tested; `jz` branches when
  // the popped value was zero, matching IrOpcode::JumpIfZero exactly (no
  // inversion needed, unlike the TODO-4748 wasm bug this session already
  // fixed elsewhere - this is a plain rel32 conditional jump, not a
  // structured if/else that could suffer the same then/else mix-up).
  emitPopReg(0);
  emitByte(0x48); // REX.W so `test` operates on the full 64-bit value
  emitByte(0x85); // test r/m64, r64
  emitModRmReg(0, 0);
  emitByte(0x0F);
  emitByte(0x84); // jz rel32
  const size_t fixupIndex = code_.size();
  emitU32(0);
  return fixupIndex;
}

// `offsetWords` (kept as the parameter name for interface parity with
// Arm64Emitter) arrives as target-position-minus-fixup-position, computed
// by NativeEmitterEmit.cpp's shared, architecture-agnostic fixup
// resolution from two `currentWordIndex()` values (byte offsets on this
// backend) - i.e. as if the jump/call were self-relative, matching
// ARM64's branch encoding. x86_64's rel32 is instead relative to the
// address of the *following* instruction (fixup position + 4, since the
// disp32 field itself is 4 bytes), so that shared computation is short by
// exactly 4 here - corrected by subtracting it internally rather than
// pushing an x86-specific adjustment into the shared driver.
inline void X64Emitter::patchJump(size_t index, int32_t offsetWords) {
  patchU32(index, static_cast<uint32_t>(offsetWords - 4));
}

inline void X64Emitter::patchCall(size_t index, int32_t offsetWords) {
  patchU32(index, static_cast<uint32_t>(offsetWords - 4));
}

inline void X64Emitter::patchJumpIfZero(size_t index, int32_t offsetWords) {
  patchU32(index, static_cast<uint32_t>(offsetWords - 4));
}

inline void X64Emitter::patchAdr(size_t index, uint8_t rd, int32_t deltaBytes) {
  (void)rd; // the register is already baked into the ModRM byte emitted
           // alongside the placeholder - only the disp32 needs patching.
  // Same "-4" correction as patchJump/patchCall/patchJumpIfZero above:
  // `deltaBytes` arrives as a plain target-minus-fixup-position delta
  // (NativeEmitterEmit.cpp's shared string-fixup resolution computes it
  // that way for both backends), but x86_64 RIP-relative addressing is
  // relative to the *next* instruction (fixup position + 4).
  patchU32(index, static_cast<uint32_t>(deltaBytes - 4));
}

inline X64Emitter::CondCode X64Emitter::invertCond(CondCode cond) {
  switch (cond) {
    case CondCode::Eq:
      return CondCode::Ne;
    case CondCode::Ne:
      return CondCode::Eq;
    case CondCode::Lt:
      return CondCode::Ge;
    case CondCode::Le:
      return CondCode::Gt;
    case CondCode::Gt:
      return CondCode::Le;
    case CondCode::Ge:
      return CondCode::Lt;
    case CondCode::Below:
      return CondCode::AboveEq;
    case CondCode::BelowEq:
      return CondCode::Above;
    case CondCode::Above:
      return CondCode::BelowEq;
    case CondCode::AboveEq:
      return CondCode::Below;
  }
  return CondCode::Ne;
}
