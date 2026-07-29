// x86_64/Linux print, file I/O, string, and argv codegen for X64Emitter.
// Included from NativeEmitterInternalsX64.h, after Core.h and
// Arithmetic.h. Mirrors NativeEmitterInternalsArm64Io.h's structure and
// call-site conventions (pop order, which pieces of the IR-level calling
// convention each opcode expects) - only the underlying instruction
// encoding and register roles differ (see Core.h's header comment for the
// x86_64 register map).

inline size_t X64Emitter::emitResolveDynamicStringAddress(uint64_t offsetTableDelta, uint8_t resultReg) {
  emitPopReg(0); // index
  const size_t fixupIndex = emitLeaRipPlaceholder(1); // reg1 = RIP-relative anchor
  emitMovRegImm64(2, offsetTableDelta);
  emitMovRegReg(9, 1);
  emitSubRegReg(9, 2); // reg9 = table base = anchor - offsetTableDelta
  emitMovRegImm64(3, 8);
  emitImulRegReg(0, 3); // reg0 = index * 8
  emitMovRegReg(10, 1);
  emitAddRegReg(10, 0);  // reg10 = anchor + idx*8 (offset-table entry address)
  emitLoadMem(10, 10, 0); // reg10 = stored offset
  emitMovRegReg(resultReg, 9);
  emitAddRegReg(resultReg, 10); // resultReg = table base + stored offset
  return fixupIndex;
}

inline size_t X64Emitter::emitResolveDynamicStringAddressAndLength(uint64_t offsetTableDelta,
                                                                    uint64_t offsetTableSize,
                                                                    uint8_t indexReg,
                                                                    uint8_t addrReg,
                                                                    uint8_t lengthReg) {
  const size_t fixupIndex = emitLeaRipPlaceholder(1); // reg1 = RIP-relative anchor
  emitMovRegImm64(2, offsetTableDelta);
  emitMovRegReg(9, 1);
  emitSubRegReg(9, 2); // reg9 = table base = anchor - offsetTableDelta
  emitMovRegImm64(3, 8);
  emitImulRegReg(indexReg, 3); // indexReg *= 8
  emitMovRegReg(10, 1);
  emitAddRegReg(10, indexReg); // reg10 = anchor + idx*8 (offset-table entry address)
  emitLoadMem(10, 10, 0);       // reg10 = stored offset
  emitMovRegReg(addrReg, 9);
  emitAddRegReg(addrReg, 10); // addrReg = table base + stored offset

  emitMovRegImm64(6, offsetTableSize);
  emitAddRegReg(6, 1);       // reg6 = anchor + offsetTableSize (length-table anchor)
  emitAddRegReg(6, indexReg); // reg6 += idx*8 (length-table entry address)
  emitLoadMem(lengthReg, 6, 0); // lengthReg = stored length
  return fixupIndex;
}

inline void X64Emitter::emitPrintUnsignedInternal(uint32_t scratchOffset,
                                                   uint32_t scratchBytes,
                                                   bool includeSign,
                                                   uint8_t signReg,
                                                   bool newline,
                                                   uint64_t fd) {
  (void)scratchBytes; // emitLoadFrameOffset already yields the buffer's
                      // outer boundary directly in this backend's
                      // addressing scheme (see its declaration comment);
                      // scratchBytes is kept only for interface parity
                      // with Arm64Emitter.
  emitLoadFrameOffset(1, scratchOffset); // reg1 = outer boundary (fixed)
  emitMovRegReg(9, 1);                    // reg9 = write cursor
  if (newline) {
    emitSubRegImm32(9, 1);
    emitMovRegImm64(11, '\n');
    emitStoreMemByte(9, 0, 11);
  }
  emitMovRegImm64(10, 10); // divisor constant, untouched by DIV
  const size_t loopStart = code_.size();
  emitMovRegImm64(2, 0); // rdx = 0 before each unsigned divide
  emitDivReg(10);        // rax /= 10 (quotient), rdx = remainder (digit)
  emitMovRegReg(11, 2);
  emitAddRegImm32(11, '0');
  emitSubRegImm32(9, 1);
  emitStoreMemByte(9, 0, 11);
  emitCmpRegImm32(0, 0); // quotient == 0?
  const size_t doneBranch = emitCondJumpPlaceholder(CondCode::Eq);
  const size_t jumpBack = emitJumpPlaceholderRaw();
  patchU32(jumpBack, static_cast<uint32_t>(static_cast<int32_t>(loopStart) - static_cast<int32_t>(jumpBack + 4)));
  patchCondJumpHere(doneBranch);

  if (includeSign) {
    emitCmpRegImm32(signReg, 0);
    const size_t skipSign = emitCondJumpPlaceholder(CondCode::Eq);
    emitSubRegImm32(9, 1);
    emitMovRegImm64(11, '-');
    emitStoreMemByte(9, 0, 11);
    patchCondJumpHere(skipSign);
  }

  emitMovRegReg(3, 1);
  emitSubRegReg(3, 9); // length = outer boundary - final cursor
  emitWriteSyscall(fd, 9, 3);
}

inline void X64Emitter::emitPrintUnsignedInternalReg(uint32_t scratchOffset,
                                                      uint32_t scratchBytes,
                                                      bool includeSign,
                                                      uint8_t signReg,
                                                      bool newline,
                                                      uint8_t fdReg) {
  (void)scratchBytes;
  emitLoadFrameOffset(1, scratchOffset);
  emitMovRegReg(9, 1);
  if (newline) {
    emitSubRegImm32(9, 1);
    emitMovRegImm64(11, '\n');
    emitStoreMemByte(9, 0, 11);
  }
  emitMovRegImm64(10, 10);
  const size_t loopStart = code_.size();
  emitMovRegImm64(2, 0);
  emitDivReg(10);
  emitMovRegReg(11, 2);
  emitAddRegImm32(11, '0');
  emitSubRegImm32(9, 1);
  emitStoreMemByte(9, 0, 11);
  emitCmpRegImm32(0, 0);
  const size_t doneBranch = emitCondJumpPlaceholder(CondCode::Eq);
  const size_t jumpBack = emitJumpPlaceholderRaw();
  patchU32(jumpBack, static_cast<uint32_t>(static_cast<int32_t>(loopStart) - static_cast<int32_t>(jumpBack + 4)));
  patchCondJumpHere(doneBranch);

  if (includeSign) {
    emitCmpRegImm32(signReg, 0);
    const size_t skipSign = emitCondJumpPlaceholder(CondCode::Eq);
    emitSubRegImm32(9, 1);
    emitMovRegImm64(11, '-');
    emitStoreMemByte(9, 0, 11);
    patchCondJumpHere(skipSign);
  }

  emitMovRegReg(3, 1);
  emitSubRegReg(3, 9);
  emitWriteSyscallReg(fdReg, 9, 3);
}

inline void X64Emitter::emitPrintSigned(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint64_t fd) {
  emitPopReg(0);
  emitCmpRegImm32(0, 0);
  const size_t jumpNonNegative = emitCondJumpPlaceholder(CondCode::Ge);
  emitNegReg(0);
  emitMovRegImm64(8, 1);
  const size_t jumpAfterSign = emitJumpPlaceholderRaw();
  patchCondJumpHere(jumpNonNegative);
  emitMovRegImm64(8, 0);
  patchJumpHere(jumpAfterSign);
  emitPrintUnsignedInternal(scratchOffset, scratchBytes, true, 8, newline, fd);
}

inline void X64Emitter::emitPrintSignedRegFromValue(uint32_t scratchOffset,
                                                     uint32_t scratchBytes,
                                                     bool newline,
                                                     uint8_t fdReg) {
  emitCmpRegImm32(0, 0);
  const size_t jumpNonNegative = emitCondJumpPlaceholder(CondCode::Ge);
  emitNegReg(0);
  emitMovRegImm64(8, 1);
  const size_t jumpAfterSign = emitJumpPlaceholderRaw();
  patchCondJumpHere(jumpNonNegative);
  emitMovRegImm64(8, 0);
  patchJumpHere(jumpAfterSign);
  emitPrintUnsignedInternalReg(scratchOffset, scratchBytes, true, 8, newline, fdReg);
}

inline void X64Emitter::emitPrintUnsigned(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint64_t fd) {
  emitPopReg(0);
  emitPrintUnsignedInternal(scratchOffset, scratchBytes, false, 0, newline, fd);
}

inline void X64Emitter::emitPrintUnsignedRegFromValue(uint32_t scratchOffset,
                                                       uint32_t scratchBytes,
                                                       bool newline,
                                                       uint8_t fdReg) {
  emitPrintUnsignedInternalReg(scratchOffset, scratchBytes, false, 0, newline, fdReg);
}

inline size_t X64Emitter::emitPrintStringPlaceholder(uint64_t lengthBytes,
                                                      uint32_t scratchOffset,
                                                      bool newline,
                                                      uint64_t fd) {
  const size_t fixupIndex = emitLeaRipPlaceholder(1);
  emitMovRegImm64(2, lengthBytes);
  emitWriteSyscall(fd, 1, 2);
  if (newline) {
    emitWriteNewline(fd, scratchOffset);
  }
  return fixupIndex;
}

inline size_t X64Emitter::emitPrintStringDynamicPlaceholder(uint64_t offsetTableDelta,
                                                             uint64_t offsetTableSize,
                                                             uint32_t scratchOffset,
                                                             bool newline,
                                                             uint64_t fd) {
  emitPopReg(0); // index
  const size_t fixupIndex = emitResolveDynamicStringAddressAndLength(offsetTableDelta, offsetTableSize, 0, 8, 7);
  emitWriteSyscall(fd, 8, 7);
  if (newline) {
    emitWriteNewline(fd, scratchOffset);
  }
  return fixupIndex;
}

inline size_t X64Emitter::emitPrintStringPlaceholderReg(uint64_t lengthBytes,
                                                         uint32_t scratchOffset,
                                                         bool newline,
                                                         uint8_t fdReg) {
  const size_t fixupIndex = emitLeaRipPlaceholder(1);
  emitMovRegImm64(2, lengthBytes);
  emitWriteSyscallReg(fdReg, 1, 2);
  if (newline) {
    emitWriteNewlineReg(fdReg, scratchOffset);
  }
  return fixupIndex;
}

inline size_t X64Emitter::emitFileOpenPlaceholder(uint64_t flags, uint64_t mode) {
  const size_t fixupIndex = emitLeaRipPlaceholder(1); // reg1 = path address
  emitMovRegReg(7, 1);       // rdi = path
  emitMovRegImm64(6, flags); // rsi = flags
  emitMovRegImm64(2, mode);  // rdx = mode
  emitMovRegImm64(0, LinuxSysOpen);
  emitSyscall();
  emitCmpRegImm32(0, 0);
  const size_t jumpNonNegative = emitCondJumpPlaceholder(CondCode::Ge);
  emitMovRegImm64(1, 1); // error flag
  emitMovRegImm64(0, 0);
  const size_t jumpAfterError = emitJumpPlaceholderRaw();
  patchCondJumpHere(jumpNonNegative);
  emitMovRegImm64(1, 0);
  patchJumpHere(jumpAfterError);
  emitMovRegImm64(2, 4294967296ull);
  emitImulRegReg(1, 2);
  emitAddRegReg(0, 1);
  emitPushReg(0);
  return fixupIndex;
}

inline size_t X64Emitter::emitFileOpenDynamicPlaceholder(uint64_t offsetTableDelta, uint64_t flags, uint64_t mode) {
  const size_t fixupIndex = emitResolveDynamicStringAddress(offsetTableDelta, 8); // reg8 = path address
  emitMovRegReg(7, 8);
  emitMovRegImm64(6, flags);
  emitMovRegImm64(2, mode);
  emitMovRegImm64(0, LinuxSysOpen);
  emitSyscall();
  emitCmpRegImm32(0, 0);
  const size_t jumpNonNegative = emitCondJumpPlaceholder(CondCode::Ge);
  emitMovRegImm64(1, 1);
  emitMovRegImm64(0, 0);
  const size_t jumpAfterError = emitJumpPlaceholderRaw();
  patchCondJumpHere(jumpNonNegative);
  emitMovRegImm64(1, 0);
  patchJumpHere(jumpAfterError);
  emitMovRegImm64(2, 4294967296ull);
  emitImulRegReg(1, 2);
  emitAddRegReg(0, 1);
  emitPushReg(0);
  return fixupIndex;
}

inline void X64Emitter::emitFileWriteI32(uint32_t scratchOffset, uint32_t scratchBytes) {
  emitPopReg(0); // value
  emitPopReg(1); // fd
  emitMovRegReg(6, 1);
  emitPrintSignedRegFromValue(scratchOffset, scratchBytes, false, 6);
  emitMovRegImm64(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitFileWriteI64(uint32_t scratchOffset, uint32_t scratchBytes) {
  emitPopReg(0);
  emitPopReg(1);
  emitMovRegReg(6, 1);
  emitPrintSignedRegFromValue(scratchOffset, scratchBytes, false, 6);
  emitMovRegImm64(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitFileWriteU64(uint32_t scratchOffset, uint32_t scratchBytes) {
  emitPopReg(0);
  emitPopReg(1);
  emitMovRegReg(6, 1);
  emitPrintUnsignedRegFromValue(scratchOffset, scratchBytes, false, 6);
  emitMovRegImm64(0, 0);
  emitPushReg(0);
}

inline size_t X64Emitter::emitFileWriteStringPlaceholder(uint64_t lengthBytes, uint32_t scratchOffset) {
  emitPopReg(3); // fd
  const size_t fixupIndex = emitPrintStringPlaceholderReg(lengthBytes, scratchOffset, false, 3);
  emitMovRegImm64(0, 0);
  emitPushReg(0);
  return fixupIndex;
}

inline size_t X64Emitter::emitFileWriteStringDynamicPlaceholder(uint64_t offsetTableDelta, uint64_t offsetTableSize) {
  emitPopReg(0); // index
  emitPopReg(8); // fd (r8 - not used internally by the resolve helper below)
  const size_t fixupIndex = emitResolveDynamicStringAddressAndLength(offsetTableDelta, offsetTableSize, 0, 11, 7);
  emitWriteSyscallReg(8, 11, 7);
  emitMovRegImm64(0, 0);
  emitPushReg(0);
  return fixupIndex;
}

inline void X64Emitter::emitFileWriteByte(uint32_t scratchOffset) {
  emitPopReg(0); // byte value
  emitPopReg(3); // fd
  emitLoadFrameOffset(1, scratchOffset + 1);
  emitStoreMemByte(1, 0, 0);
  emitMovRegImm64(2, 1);
  emitWriteSyscallReg(3, 1, 2);
  emitMovRegImm64(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitFileReadByte(uint32_t localIndex, uint32_t scratchOffset) {
  emitPopReg(3); // fd
  emitLoadFrameOffset(1, scratchOffset + 1);
  emitMovRegImm64(2, 1);
  emitReadSyscallReg(3, 1, 2); // reg0 = bytes read, or a negative errno

  emitCmpRegImm32(0, 1);
  const size_t successBranch = emitCondJumpPlaceholder(CondCode::Eq);
  emitCmpRegImm32(0, 0);
  const size_t eofBranch = emitCondJumpPlaceholder(CondCode::Eq);
  emitMovRegImm64(0, 1);
  const size_t afterError = emitJumpPlaceholderRaw();

  patchCondJumpHere(successBranch);
  emitLoadFrameOffset(1, scratchOffset + 1);
  emitLoadMemByte(2, 1, 0);
  emitStoreLocalFromReg(localIndex, 2);
  emitMovRegImm64(0, 0);
  const size_t afterSuccess = emitJumpPlaceholderRaw();

  patchCondJumpHere(eofBranch);
  emitMovRegImm64(0, FileReadEofCode);

  patchJumpHere(afterError);
  patchJumpHere(afterSuccess);
  emitPushReg(0);
}

inline void X64Emitter::emitFileWriteNewline(uint32_t scratchOffset) {
  emitPopReg(3); // fd
  emitWriteNewlineReg(3, scratchOffset);
  emitMovRegImm64(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitFileClose() {
  emitPopReg(0);
  emitMovRegReg(7, 0); // rdi = fd
  emitMovRegImm64(0, LinuxSysClose);
  emitSyscall();
  emitMovRegImm64(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitFileFlush() {
  emitPopReg(0);
  emitMovRegReg(7, 0);
  emitMovRegImm64(0, LinuxSysFsync);
  emitSyscall();
  emitMovRegImm64(0, 0);
  emitPushReg(0);
}

inline size_t X64Emitter::emitLoadStringBytePlaceholder() {
  emitPopReg(0); // index
  const size_t fixupIndex = emitLeaRipPlaceholder(1); // reg1 = string base
  emitAddRegReg(1, 0);
  emitLoadMemByte(2, 1, 0);
  emitPushReg(2);
  return fixupIndex;
}

inline size_t X64Emitter::emitLoadStringLengthPlaceholder(uint64_t offsetTableSize) {
  emitPopReg(0); // index
  const size_t fixupIndex = emitLeaRipPlaceholder(1); // reg1 = anchor
  emitMovRegReg(2, 1);
  emitAddRegImm32(2, static_cast<int32_t>(offsetTableSize));
  emitMovRegImm64(3, 8);
  emitImulRegReg(0, 3);
  emitAddRegReg(2, 0); // reg2 = length-table entry address
  emitLoadMem(9, 2, 0);
  emitPushReg(9);
  return fixupIndex;
}

inline void X64Emitter::emitPrintArgv(uint32_t argcLocalIndex,
                                      uint32_t argvLocalIndex,
                                      uint32_t scratchOffset,
                                      bool newline,
                                      uint64_t fd) {
  emitPopReg(0); // index
  emitCmpRegImm32(0, 0);
  const size_t negativeBranch = emitCondJumpPlaceholder(CondCode::Lt);

  emitLoadLocalToReg(1, argcLocalIndex);
  emitCmpRegReg(0, 1); // flags = index - argc
  const size_t oobBranch = emitCondJumpPlaceholder(CondCode::Ge);

  emitLoadLocalToReg(2, argvLocalIndex);
  emitMovRegImm64(3, 8);
  emitImulRegReg(0, 3); // reg0 = index * 8
  emitAddRegReg(2, 0);  // reg2 = argv + idx*8
  emitLoadMem(1, 2, 0);  // reg1 = argv[idx]
  emitCmpRegImm32(1, 0);
  const size_t nullBranch = emitCondJumpPlaceholder(CondCode::Eq);

  emitMovRegReg(3, 1); // reg3 = string cursor
  emitMovRegImm64(2, 0); // reg2 = length accumulator
  const size_t loopStart = code_.size();
  emitLoadMemByte(9, 3, 0);
  emitCmpRegImm32(9, 0);
  const size_t doneBranch = emitCondJumpPlaceholder(CondCode::Eq);
  emitAddRegImm32(2, 1);
  emitAddRegImm32(3, 1);
  const size_t loopJump = emitJumpPlaceholderRaw();
  patchU32(loopJump, static_cast<uint32_t>(static_cast<int32_t>(loopStart) - static_cast<int32_t>(loopJump + 4)));
  patchCondJumpHere(doneBranch);

  emitWriteSyscall(fd, 1, 2);
  if (newline) {
    emitWriteNewline(fd, scratchOffset);
  }

  patchCondJumpHere(negativeBranch);
  patchCondJumpHere(oobBranch);
  patchCondJumpHere(nullBranch);
}
