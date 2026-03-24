#pragma once

inline void Arm64Emitter::emitPrintSigned(uint32_t scratchOffset,
                                          uint32_t scratchBytes,
                                          bool newline,
                                          uint64_t fd) {
  emitPopReg(0);
  emitCompareRegZero(0);
  size_t nonNegative = emitCondBranchPlaceholder(CondCode::Ge);
  emitSubReg(0, 31, 0);
  emitMovImm64(5, 1);
  size_t afterSign = emitJumpPlaceholder();
  size_t nonNegativeIndex = currentWordIndex();
  patchCondBranch(nonNegative, static_cast<int32_t>(nonNegativeIndex - nonNegative), CondCode::Ge);
  emitMovImm64(5, 0);
  size_t afterSignIndex = currentWordIndex();
  patchJump(afterSign, static_cast<int32_t>(afterSignIndex - afterSign));
  emitPrintUnsignedInternal(scratchOffset, scratchBytes, true, 5, newline, fd);
}

inline void Arm64Emitter::emitPrintSignedRegFromValue(uint32_t scratchOffset,
                                                      uint32_t scratchBytes,
                                                      bool newline,
                                                      uint8_t fdReg) {
  emitCompareRegZero(0);
  size_t nonNegative = emitCondBranchPlaceholder(CondCode::Ge);
  emitSubReg(0, 31, 0);
  emitMovImm64(5, 1);
  size_t afterSign = emitJumpPlaceholder();
  size_t nonNegativeIndex = currentWordIndex();
  patchCondBranch(nonNegative, static_cast<int32_t>(nonNegativeIndex - nonNegative), CondCode::Ge);
  emitMovImm64(5, 0);
  size_t afterSignIndex = currentWordIndex();
  patchJump(afterSign, static_cast<int32_t>(afterSignIndex - afterSign));
  emitPrintUnsignedInternalReg(scratchOffset, scratchBytes, true, 5, newline, fdReg);
}

inline void Arm64Emitter::emitPrintUnsigned(uint32_t scratchOffset,
                                            uint32_t scratchBytes,
                                            bool newline,
                                            uint64_t fd) {
  emitPopReg(0);
  emitPrintUnsignedInternal(scratchOffset, scratchBytes, false, 0, newline, fd);
}

inline void Arm64Emitter::emitPrintUnsignedRegFromValue(uint32_t scratchOffset,
                                                        uint32_t scratchBytes,
                                                        bool newline,
                                                        uint8_t fdReg) {
  emitPrintUnsignedInternalReg(scratchOffset, scratchBytes, false, 0, newline, fdReg);
}

inline size_t Arm64Emitter::emitPrintStringPlaceholder(uint64_t lengthBytes,
                                                       uint32_t scratchOffset,
                                                       bool newline,
                                                       uint64_t fd) {
  size_t fixupIndex = emitAdrPlaceholder(1);
  emitMovImm64(2, lengthBytes);
  emitWriteSyscall(fd, 1, 2);
  if (newline) {
    emitWriteNewline(fd, scratchOffset);
  }
  return fixupIndex;
}

inline size_t Arm64Emitter::emitPrintStringDynamicPlaceholder(uint64_t offsetTableDelta,
                                                              uint64_t offsetTableSize,
                                                              uint32_t scratchOffset,
                                                              bool newline,
                                                              uint64_t fd) {
  emitPopReg(0);
  size_t fixupIndex = emitAdrPlaceholder(1);
  emitMovImm64(2, offsetTableDelta);
  emitSubReg(2, 1, 2);
  emitMovImm64(3, 8);
  emitMulReg(3, 0, 3);
  emitAddReg(4, 1, 3);
  emit(encodeLdrRegBase(4, 4, 0));
  emitAddReg(5, 2, 4);
  emitMovImm64(6, offsetTableSize);
  emitAddReg(6, 1, 6);
  emitAddReg(7, 6, 3);
  emit(encodeLdrRegBase(7, 7, 0));
  emitWriteSyscall(fd, 5, 7);
  if (newline) {
    emitWriteNewline(fd, scratchOffset);
  }
  return fixupIndex;
}

inline size_t Arm64Emitter::emitPrintStringPlaceholderReg(uint64_t lengthBytes,
                                                          uint32_t scratchOffset,
                                                          bool newline,
                                                          uint8_t fdReg) {
  size_t fixupIndex = emitAdrPlaceholder(1);
  emitMovImm64(2, lengthBytes);
  emitWriteSyscallReg(fdReg, 1, 2);
  if (newline) {
    emitWriteNewlineReg(fdReg, scratchOffset);
  }
  return fixupIndex;
}

inline size_t Arm64Emitter::emitFileOpenPlaceholder(uint64_t flags, uint64_t mode) {
  size_t fixupIndex = emitAdrPlaceholder(1);
  emitMovReg(0, 1);
  emitMovImm64(1, flags);
  emitMovImm64(2, mode);
  emitMovImm64(16, SysOpen);
  emit(encodeSvc());
  emitCompareRegZero(0);
  size_t nonNegative = emitCondBranchPlaceholder(CondCode::Ge);
  emitMovImm64(1, 1);
  emitMovImm64(0, 0);
  size_t afterError = emitJumpPlaceholder();
  size_t nonNegativeIndex = currentWordIndex();
  patchCondBranch(nonNegative, static_cast<int32_t>(nonNegativeIndex - nonNegative), CondCode::Ge);
  emitMovImm64(1, 0);
  size_t afterIndex = currentWordIndex();
  patchJump(afterError, static_cast<int32_t>(afterIndex - afterError));
  emitMovImm64(2, 4294967296ull);
  emitMulReg(1, 1, 2);
  emitAddReg(0, 0, 1);
  emitPushReg(0);
  return fixupIndex;
}

inline size_t Arm64Emitter::emitFileOpenDynamicPlaceholder(uint64_t offsetTableDelta,
                                                           uint64_t flags,
                                                           uint64_t mode) {
  emitPopReg(0);
  size_t fixupIndex = emitAdrPlaceholder(1);
  emitMovImm64(2, offsetTableDelta);
  emitSubReg(2, 1, 2);
  emitMovImm64(3, 8);
  emitMulReg(3, 0, 3);
  emitAddReg(4, 1, 3);
  emit(encodeLdrRegBase(4, 4, 0));
  emitAddReg(5, 2, 4);
  emitMovReg(0, 5);
  emitMovImm64(1, flags);
  emitMovImm64(2, mode);
  emitMovImm64(16, SysOpen);
  emit(encodeSvc());
  emitCompareRegZero(0);
  size_t nonNegative = emitCondBranchPlaceholder(CondCode::Ge);
  emitMovImm64(1, 1);
  emitMovImm64(0, 0);
  size_t afterError = emitJumpPlaceholder();
  size_t nonNegativeIndex = currentWordIndex();
  patchCondBranch(nonNegative, static_cast<int32_t>(nonNegativeIndex - nonNegative), CondCode::Ge);
  emitMovImm64(1, 0);
  size_t afterIndex = currentWordIndex();
  patchJump(afterError, static_cast<int32_t>(afterIndex - afterError));
  emitMovImm64(2, 4294967296ull);
  emitMulReg(1, 1, 2);
  emitAddReg(0, 0, 1);
  emitPushReg(0);
  return fixupIndex;
}

inline void Arm64Emitter::emitFileWriteI32(uint32_t scratchOffset, uint32_t scratchBytes) {
  emitPopReg(0);
  emitPopReg(1);
  emitMovReg(6, 1);
  emitPrintSignedRegFromValue(scratchOffset, scratchBytes, false, 6);
  emitMovImm64(0, 0);
  emitPushReg(0);
}

inline void Arm64Emitter::emitFileWriteI64(uint32_t scratchOffset, uint32_t scratchBytes) {
  emitPopReg(0);
  emitPopReg(1);
  emitMovReg(6, 1);
  emitPrintSignedRegFromValue(scratchOffset, scratchBytes, false, 6);
  emitMovImm64(0, 0);
  emitPushReg(0);
}

inline void Arm64Emitter::emitFileWriteU64(uint32_t scratchOffset, uint32_t scratchBytes) {
  emitPopReg(0);
  emitPopReg(1);
  emitMovReg(6, 1);
  emitPrintUnsignedRegFromValue(scratchOffset, scratchBytes, false, 6);
  emitMovImm64(0, 0);
  emitPushReg(0);
}

inline size_t Arm64Emitter::emitFileWriteStringPlaceholder(uint64_t lengthBytes,
                                                           uint32_t scratchOffset) {
  emitPopReg(3);
  size_t fixupIndex = emitPrintStringPlaceholderReg(lengthBytes, scratchOffset, false, 3);
  emitMovImm64(0, 0);
  emitPushReg(0);
  return fixupIndex;
}

inline size_t Arm64Emitter::emitFileWriteStringDynamicPlaceholder(uint64_t offsetTableDelta,
                                                                  uint64_t offsetTableSize) {
  emitPopReg(0);
  emitPopReg(3);
  emitMovReg(8, 3);
  size_t fixupIndex = emitAdrPlaceholder(1);
  emitMovImm64(2, offsetTableDelta);
  emitSubReg(2, 1, 2);
  emitMovImm64(3, 8);
  emitMulReg(3, 0, 3);
  emitAddReg(4, 1, 3);
  emit(encodeLdrRegBase(4, 4, 0));
  emitAddReg(5, 2, 4);
  emitMovImm64(6, offsetTableSize);
  emitAddReg(6, 1, 6);
  emitAddReg(7, 6, 3);
  emit(encodeLdrRegBase(7, 7, 0));
  emitWriteSyscallReg(8, 5, 7);
  emitMovImm64(0, 0);
  emitPushReg(0);
  return fixupIndex;
}

inline void Arm64Emitter::emitFileWriteByte(uint32_t scratchOffset) {
  emitPopReg(0);
  emitPopReg(3);
  emitLoadFrameOffset(1, scratchOffset);
  emitStrbRegBase(0, 1, 0);
  emitMovImm64(2, 1);
  emitWriteSyscallReg(3, 1, 2);
  emitMovImm64(0, 0);
  emitPushReg(0);
}

inline void Arm64Emitter::emitFileReadByte(uint32_t localIndex, uint32_t scratchOffset) {
  emitPopReg(3);
  emitLoadFrameOffset(1, scratchOffset);
  emitMovImm64(2, 1);
  emitReadSyscallReg(3, 1, 2);

  emitMovImm64(2, 1);
  emitCompareReg(0, 2);
  size_t successBranch = emitCondBranchPlaceholder(CondCode::Eq);
  emitCompareRegZero(0);
  size_t eofBranch = emitCondBranchPlaceholder(CondCode::Eq);

  emitMovImm64(0, 1);
  size_t afterError = emitJumpPlaceholder();

  size_t successIndex = currentWordIndex();
  patchCondBranch(successBranch, static_cast<int32_t>(successIndex - successBranch), CondCode::Eq);
  emitLoadFrameOffset(1, scratchOffset);
  emit(encodeLdrbRegBase(2, 1, 0));
  emitStoreLocalFromReg(localIndex, 2);
  emitMovImm64(0, 0);
  size_t afterSuccess = emitJumpPlaceholder();

  size_t eofIndex = currentWordIndex();
  patchCondBranch(eofBranch, static_cast<int32_t>(eofIndex - eofBranch), CondCode::Eq);
  emitMovImm64(0, FileReadEofCode);

  size_t afterIndex = currentWordIndex();
  patchJump(afterError, static_cast<int32_t>(afterIndex - afterError));
  patchJump(afterSuccess, static_cast<int32_t>(afterIndex - afterSuccess));
  emitPushReg(0);
}

inline void Arm64Emitter::emitFileWriteNewline(uint32_t scratchOffset) {
  emitPopReg(3);
  emitWriteNewlineReg(3, scratchOffset);
  emitMovImm64(0, 0);
  emitPushReg(0);
}

inline void Arm64Emitter::emitFileClose() {
  emitPopReg(0);
  emitMovImm64(16, SysClose);
  emit(encodeSvc());
  emitMovImm64(0, 0);
  emitPushReg(0);
}

inline void Arm64Emitter::emitFileFlush() {
  emitPopReg(0);
  emitMovImm64(16, SysFsync);
  emit(encodeSvc());
  emitMovImm64(0, 0);
  emitPushReg(0);
}

inline size_t Arm64Emitter::emitLoadStringBytePlaceholder() {
  emitPopReg(0);
  size_t fixupIndex = emitAdrPlaceholder(1);
  emitAddReg(1, 1, 0);
  emit(encodeLdrbRegBase(2, 1, 0));
  emitPushReg(2);
  return fixupIndex;
}

inline size_t Arm64Emitter::emitLoadStringLengthPlaceholder(uint64_t offsetTableSize) {
  emitPopReg(0);
  size_t fixupIndex = emitAdrPlaceholder(1);
  emitMovImm64(2, offsetTableSize);
  emitAddReg(2, 1, 2);
  emitMovImm64(3, 8);
  emitMulReg(3, 0, 3);
  emitAddReg(2, 2, 3);
  emit(encodeLdrRegBase(4, 2, 0));
  emitPushReg(4);
  return fixupIndex;
}

inline void Arm64Emitter::emitPrintArgv(uint32_t argcLocalIndex,
                                        uint32_t argvLocalIndex,
                                        uint32_t scratchOffset,
                                        bool newline,
                                        uint64_t fd) {
  emitPopReg(0);
  emitCompareRegZero(0);
  size_t negativeBranch = emitCondBranchPlaceholder(CondCode::Lt);

  emitLoadLocalToReg(1, argcLocalIndex);
  emitCompareReg(0, 1);
  size_t oobBranch = emitCondBranchPlaceholder(CondCode::Ge);

  emitLoadLocalToReg(2, argvLocalIndex);
  emitMovImm64(3, 8);
  emitMulReg(3, 0, 3);
  emitAddReg(2, 2, 3);
  emit(encodeLdrRegBase(1, 2, 0));
  emitCompareRegZero(1);
  size_t nullBranch = emitCondBranchPlaceholder(CondCode::Eq);

  emitMovReg(3, 1);
  emitMovImm64(2, 0);
  size_t loopStart = currentWordIndex();
  emit(encodeLdrbRegBase(4, 3, 0));
  emitCompareRegZero(4);
  size_t doneBranch = emitCondBranchPlaceholder(CondCode::Eq);
  emitAddRegImm(2, 2, 1);
  emitAddRegImm(3, 3, 1);
  size_t loopJump = emitJumpPlaceholder();
  patchJump(loopJump, static_cast<int32_t>(loopStart - loopJump));
  size_t doneIndex = currentWordIndex();
  patchCondBranch(doneBranch, static_cast<int32_t>(doneIndex - doneBranch), CondCode::Eq);

  emitWriteSyscall(fd, 1, 2);
  if (newline) {
    emitWriteNewline(fd, scratchOffset);
  }

  size_t skipIndex = currentWordIndex();
  patchCondBranch(negativeBranch, static_cast<int32_t>(skipIndex - negativeBranch), CondCode::Lt);
  patchCondBranch(oobBranch, static_cast<int32_t>(skipIndex - oobBranch), CondCode::Ge);
  patchCondBranch(nullBranch, static_cast<int32_t>(skipIndex - nullBranch), CondCode::Eq);
}

inline void Arm64Emitter::emitStrbRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes) {
  emit(encodeStrbRegBase(rt, rn, offsetBytes));
}

inline void Arm64Emitter::emitWriteSyscall(uint64_t fd, uint8_t bufferReg, uint8_t lengthReg) {
  emitMovImm64(0, fd);
  emitMovReg(1, bufferReg);
  emitMovReg(2, lengthReg);
  emitMovImm64(16, SysWrite);
  emit(encodeSvc());
}

inline void Arm64Emitter::emitWriteSyscallReg(uint8_t fdReg, uint8_t bufferReg, uint8_t lengthReg) {
  emitMovReg(0, fdReg);
  emitMovReg(1, bufferReg);
  emitMovReg(2, lengthReg);
  emitMovImm64(16, SysWrite);
  emit(encodeSvc());
}

inline void Arm64Emitter::emitReadSyscallReg(uint8_t fdReg, uint8_t bufferReg, uint8_t lengthReg) {
  emitMovReg(0, fdReg);
  emitMovReg(1, bufferReg);
  emitMovReg(2, lengthReg);
  emitMovImm64(16, SysRead);
  emit(encodeSvc());
}

inline void Arm64Emitter::emitWriteNewline(uint64_t fd, uint32_t scratchOffset) {
  emitLoadFrameOffset(1, scratchOffset);
  emitMovImm64(4, '\n');
  emitStrbRegBase(4, 1, 0);
  emitMovImm64(2, 1);
  emitWriteSyscall(fd, 1, 2);
}

inline void Arm64Emitter::emitWriteNewlineReg(uint8_t fdReg, uint32_t scratchOffset) {
  emitLoadFrameOffset(1, scratchOffset);
  emitMovImm64(4, '\n');
  emitStrbRegBase(4, 1, 0);
  emitMovImm64(2, 1);
  emitWriteSyscallReg(fdReg, 1, 2);
}

inline void Arm64Emitter::emitLoadFrameOffset(uint8_t rd, uint32_t offsetBytes) {
  if (offsetBytes <= 4095) {
    emitAddRegImm(rd, 27, static_cast<uint16_t>(offsetBytes));
    return;
  }
  emitMovImm64(9, offsetBytes);
  emitAddReg(rd, 27, 9);
}

inline void Arm64Emitter::emitAddOffset(uint8_t rd, uint8_t rn, uint32_t offsetBytes) {
  if (offsetBytes <= 4095) {
    emitAddRegImm(rd, rn, static_cast<uint16_t>(offsetBytes));
    return;
  }
  emitMovImm64(9, offsetBytes);
  emitAddReg(rd, rn, 9);
}

inline void Arm64Emitter::emitPrintUnsignedInternal(uint32_t scratchOffset,
                                                    uint32_t scratchBytes,
                                                    bool includeSign,
                                                    uint8_t signReg,
                                                    bool newline,
                                                    uint64_t fd) {
  emitLoadFrameOffset(1, scratchOffset);
  emitAddOffset(1, 1, scratchBytes);
  emitMovReg(2, 1);
  if (newline) {
    emitSubRegImm(2, 2, 1);
    emitMovImm64(4, '\n');
    emitStrbRegBase(4, 2, 0);
  }
  emitMovImm64(10, 10);

  size_t loopStart = currentWordIndex();
  emitUdivReg(3, 0, 10);
  emitMulReg(4, 3, 10);
  emitSubReg(4, 0, 4);
  emitAddRegImm(4, 4, static_cast<uint16_t>('0'));
  emitSubRegImm(2, 2, 1);
  emitStrbRegBase(4, 2, 0);
  emitMovReg(0, 3);
  size_t doneBranch = emitCbzPlaceholder(0);
  size_t jumpBack = emitJumpPlaceholder();
  int32_t loopDelta = static_cast<int32_t>(static_cast<int64_t>(loopStart) - static_cast<int64_t>(jumpBack));
  patchJump(jumpBack, loopDelta);
  size_t doneIndex = currentWordIndex();
  int32_t doneDelta = static_cast<int32_t>(static_cast<int64_t>(doneIndex) - static_cast<int64_t>(doneBranch));
  patchCbz(doneBranch, 0, doneDelta);

  if (includeSign) {
    size_t skipSign = emitCbzPlaceholder(signReg);
    emitSubRegImm(2, 2, 1);
    emitMovImm64(4, '-');
    emitStrbRegBase(4, 2, 0);
    size_t afterSign = currentWordIndex();
    int32_t signDelta = static_cast<int32_t>(static_cast<int64_t>(afterSign) - static_cast<int64_t>(skipSign));
    patchCbz(skipSign, signReg, signDelta);
  }

  emitSubReg(3, 1, 2);
  emitWriteSyscall(fd, 2, 3);
}

inline void Arm64Emitter::emitPrintUnsignedInternalReg(uint32_t scratchOffset,
                                                       uint32_t scratchBytes,
                                                       bool includeSign,
                                                       uint8_t signReg,
                                                       bool newline,
                                                       uint8_t fdReg) {
  emitLoadFrameOffset(1, scratchOffset);
  emitAddOffset(1, 1, scratchBytes);
  emitMovReg(2, 1);
  if (newline) {
    emitSubRegImm(2, 2, 1);
    emitMovImm64(4, '\n');
    emitStrbRegBase(4, 2, 0);
  }
  emitMovImm64(10, 10);

  size_t loopStart = currentWordIndex();
  emitUdivReg(3, 0, 10);
  emitMulReg(4, 3, 10);
  emitSubReg(4, 0, 4);
  emitAddRegImm(4, 4, static_cast<uint16_t>('0'));
  emitSubRegImm(2, 2, 1);
  emitStrbRegBase(4, 2, 0);
  emitMovReg(0, 3);
  size_t doneBranch = emitCbzPlaceholder(0);
  size_t jumpBack = emitJumpPlaceholder();
  int32_t loopDelta = static_cast<int32_t>(static_cast<int64_t>(loopStart) - static_cast<int64_t>(jumpBack));
  patchJump(jumpBack, loopDelta);
  size_t doneIndex = currentWordIndex();
  int32_t doneDelta = static_cast<int32_t>(static_cast<int64_t>(doneIndex) - static_cast<int64_t>(doneBranch));
  patchCbz(doneBranch, 0, doneDelta);

  if (includeSign) {
    size_t skipSign = emitCbzPlaceholder(signReg);
    emitSubRegImm(2, 2, 1);
    emitMovImm64(4, '-');
    emitStrbRegBase(4, 2, 0);
    size_t afterSign = currentWordIndex();
    int32_t signDelta = static_cast<int32_t>(static_cast<int64_t>(afterSign) - static_cast<int64_t>(skipSign));
    patchCbz(skipSign, signReg, signDelta);
  }

  emitSubReg(3, 1, 2);
  emitWriteSyscallReg(fdReg, 2, 3);
}
