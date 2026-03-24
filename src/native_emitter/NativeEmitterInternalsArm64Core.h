#pragma once

inline void Arm64Emitter::emitHeapAlloc() {
  emitPopReg(1);
  emitHeapAllocFromSlotCountReg(1, 0);
  emitPushReg(0);
}

inline void Arm64Emitter::emitHeapFree() {
  emitPopReg(0);
  emitHeapFreeFromAddressReg(0);
}

inline void Arm64Emitter::emitHeapRealloc() {
  emitPopReg(1);
  emitPopReg(0);
  emitCompareRegZero(0);
  const size_t jumpOldNonNull = emitCondBranchPlaceholder(CondCode::Ne);
  emitHeapAllocFromSlotCountReg(1, 0);
  emitPushReg(0);
  const size_t jumpDone = emitJumpPlaceholder();

  const size_t oldNonNullIndex = currentWordIndex();
  patchCondBranch(jumpOldNonNull,
                  static_cast<int32_t>(oldNonNullIndex - jumpOldNonNull),
                  CondCode::Ne);

  emitCompareRegZero(1);
  const size_t jumpNonZeroSlots = emitCondBranchPlaceholder(CondCode::Ne);
  emitHeapFreeFromAddressReg(0);
  emitMovImm64(0, 0);
  emitPushReg(0);
  const size_t jumpDoneAfterFree = emitJumpPlaceholder();

  const size_t nonZeroSlotsIndex = currentWordIndex();
  patchCondBranch(jumpNonZeroSlots,
                  static_cast<int32_t>(nonZeroSlotsIndex - jumpNonZeroSlots),
                  CondCode::Ne);

  emitMovReg(6, 1);
  emitMovReg(7, 0);
  emitMovReg(12, 6);
  emitMovImm64(8, IrSlotBytes);
  emitSubReg(9, 7, 8);
  emit(encodeLdrRegBase(10, 9, 0));
  emitMovImm64(11, HeapHeaderMagic);
  emitCompareReg(10, 11);
  const size_t jumpInvalidAddress = emitCondBranchPlaceholder(CondCode::Ne);
  emit(encodeLdrRegBase(8, 9, 8));

  emitHeapAllocFromSlotCountReg(6, 0);
  emitCompareRegZero(0);
  const size_t jumpAllocSucceeded = emitCondBranchPlaceholder(CondCode::Ne);
  emitPushReg(0);
  const size_t jumpDoneAfterAllocFailure = emitJumpPlaceholder();

  const size_t allocSucceededIndex = currentWordIndex();
  patchCondBranch(jumpAllocSucceeded,
                  static_cast<int32_t>(allocSucceededIndex - jumpAllocSucceeded),
                  CondCode::Ne);

  emitMovReg(13, 0);
  emitMovImm64(14, IrSlotBytes);
  emitUdivReg(15, 8, 14);
  emitMovImm64(6, 1);
  emitSubReg(15, 15, 6);
  emitCompareReg(15, 12);
  const size_t jumpKeepOldSlots = emitCondBranchPlaceholder(CondCode::Ls);
  emitMovReg(15, 12);
  const size_t keepOldSlotsIndex = currentWordIndex();
  patchCondBranch(jumpKeepOldSlots,
                  static_cast<int32_t>(keepOldSlotsIndex - jumpKeepOldSlots),
                  CondCode::Ls);

  emitCompareRegZero(15);
  const size_t jumpSkipCopy = emitCondBranchPlaceholder(CondCode::Eq);
  emitMovReg(10, 7);
  emitMovReg(11, 13);
  emitMovImm64(14, IrSlotBytes);
  emitMovImm64(6, 1);
  const size_t copyLoopIndex = currentWordIndex();
  emit(encodeLdrRegBase(2, 10, 0));
  emit(encodeStrRegBase(2, 11, 0));
  emit(encodeAddReg(10, 10, 14));
  emit(encodeAddReg(11, 11, 14));
  emitSubReg(15, 15, 6);
  emitCompareRegZero(15);
  const size_t jumpCopyLoop = emitCondBranchPlaceholder(CondCode::Ne);
  patchCondBranch(jumpCopyLoop,
                  static_cast<int32_t>(copyLoopIndex - jumpCopyLoop),
                  CondCode::Ne);

  const size_t skipCopyIndex = currentWordIndex();
  patchCondBranch(jumpSkipCopy,
                  static_cast<int32_t>(skipCopyIndex - jumpSkipCopy),
                  CondCode::Eq);

  emitMovReg(0, 9);
  emitMovReg(1, 8);
  emitMovImm64(16, SysMunmap);
  emit(encodeSvc());
  emitMovReg(0, 13);
  emitPushReg(0);
  const size_t jumpDoneAfterCopy = emitJumpPlaceholder();

  const size_t invalidAddressIndex = currentWordIndex();
  patchCondBranch(jumpInvalidAddress,
                  static_cast<int32_t>(invalidAddressIndex - jumpInvalidAddress),
                  CondCode::Ne);
  emitMovImm64(0, 0);
  emitPushReg(0);

  const size_t doneIndex = currentWordIndex();
  patchJump(jumpDoneAfterAllocFailure,
            static_cast<int32_t>(doneIndex - jumpDoneAfterAllocFailure));
  patchJump(jumpDoneAfterCopy, static_cast<int32_t>(doneIndex - jumpDoneAfterCopy));
  patchJump(jumpDoneAfterFree, static_cast<int32_t>(doneIndex - jumpDoneAfterFree));
  patchJump(jumpDone, static_cast<int32_t>(doneIndex - jumpDone));
}

inline void Arm64Emitter::emitReturn() {
  emitPopReg(0);
  if (frameSize_ > 0) {
    emitAdjustSp(frameSize_, true);
  }
  emit(encodeRet());
}

inline void Arm64Emitter::emitReturnVoid() {
  flushValueStackCache();
  emitMovImm64(0, 0);
  if (frameSize_ > 0) {
    emitAdjustSp(frameSize_, true);
  }
  emit(encodeRet());
}

inline void Arm64Emitter::emitReturnWithLink(uint32_t linkLocalIndex) {
  emitPopReg(0);
  emitLoadLocalToReg(30, linkLocalIndex);
  if (frameSize_ > 0) {
    emitAdjustSp(frameSize_, true);
  }
  emit(encodeRet());
}

inline void Arm64Emitter::emitReturnVoidWithLink(uint32_t linkLocalIndex) {
  flushValueStackCache();
  emitMovImm64(0, 0);
  emitLoadLocalToReg(30, linkLocalIndex);
  if (frameSize_ > 0) {
    emitAdjustSp(frameSize_, true);
  }
  emit(encodeRet());
}

inline void Arm64Emitter::emitReturnWithFrameAndLink(uint32_t frameLocalIndex,
                                                     uint32_t linkLocalIndex) {
  emitPopReg(0);
  emitLoadLocalToReg(30, linkLocalIndex);
  emitLoadLocalToReg(27, frameLocalIndex);
  if (frameSize_ > 0) {
    emitAdjustSp(frameSize_, true);
  }
  emit(encodeRet());
}

inline void Arm64Emitter::emitReturnVoidWithFrameAndLink(uint32_t frameLocalIndex,
                                                         uint32_t linkLocalIndex) {
  flushValueStackCache();
  emitMovImm64(0, 0);
  emitLoadLocalToReg(30, linkLocalIndex);
  emitLoadLocalToReg(27, frameLocalIndex);
  if (frameSize_ > 0) {
    emitAdjustSp(frameSize_, true);
  }
  emit(encodeRet());
}

inline void Arm64Emitter::emit(uint32_t word) {
  code_.push_back(word);
}

inline void Arm64Emitter::patchWord(size_t index, uint32_t word) {
  if (index < code_.size()) {
    code_[index] = word;
  }
}

inline void Arm64Emitter::emitPushReg(uint8_t reg) {
  counters_.valueStackPushCount += 1;
  if (!valueStackCacheEnabled_) {
    emitSpillReg(reg);
    return;
  }
  if (hasValueStackCache_) {
    emitSpillReg(valueStackCacheReg_);
  }
  if (reg != valueStackCacheReg_) {
    emitMovReg(valueStackCacheReg_, reg);
  }
  hasValueStackCache_ = true;
}

inline void Arm64Emitter::emitPopReg(uint8_t reg) {
  counters_.valueStackPopCount += 1;
  if (!valueStackCacheEnabled_) {
    emitReloadReg(reg);
    return;
  }
  if (hasValueStackCache_) {
    if (reg != valueStackCacheReg_) {
      emitMovReg(reg, valueStackCacheReg_);
    }
    hasValueStackCache_ = false;
    return;
  }
  emitReloadReg(reg);
}

inline void Arm64Emitter::emitMovImm64(uint8_t rd, uint64_t value) {
  emit(encodeMovz(rd, static_cast<uint16_t>(value & 0xFFFF), 0));
  emit(encodeMovk(rd, static_cast<uint16_t>((value >> 16) & 0xFFFF), 16));
  emit(encodeMovk(rd, static_cast<uint16_t>((value >> 32) & 0xFFFF), 32));
  emit(encodeMovk(rd, static_cast<uint16_t>((value >> 48) & 0xFFFF), 48));
}

inline void Arm64Emitter::emitAdjustSp(uint64_t amount, bool add) {
  if (amount == 0) {
    return;
  }
  constexpr uint16_t kChunk = 4080;
  while (amount > 4095) {
    emit(add ? encodeAddSpImm(kChunk) : encodeSubSpImm(kChunk));
    amount -= kChunk;
  }
  if (amount > 0) {
    emit(add ? encodeAddSpImm(static_cast<uint16_t>(amount))
             : encodeSubSpImm(static_cast<uint16_t>(amount)));
  }
}

inline size_t Arm64Emitter::emitAdrPlaceholder(uint8_t rd) {
  size_t index = currentWordIndex();
  emit(encodeAdrp(rd, 0));
  emit(encodeAddRegImm(rd, rd, 0));
  return index;
}

inline void Arm64Emitter::emitMovReg(uint8_t rd, uint8_t rn) {
  emit(encodeAddRegImm(rd, rn, 0));
}

inline void Arm64Emitter::emitAddRegImm(uint8_t rd, uint8_t rn, uint16_t imm) {
  emit(encodeAddRegImm(rd, rn, imm));
}

inline void Arm64Emitter::emitSubRegImm(uint8_t rd, uint8_t rn, uint16_t imm) {
  emit(encodeSubRegImm(rd, rn, imm));
}

inline void Arm64Emitter::emitAddReg(uint8_t rd, uint8_t rn, uint8_t rm) {
  emit(encodeAddReg(rd, rn, rm));
}

inline void Arm64Emitter::emitSubReg(uint8_t rd, uint8_t rn, uint8_t rm) {
  emit(encodeSubReg(rd, rn, rm));
}

inline void Arm64Emitter::emitMulReg(uint8_t rd, uint8_t rn, uint8_t rm) {
  emit(encodeMulReg(rd, rn, rm));
}

inline void Arm64Emitter::emitUdivReg(uint8_t rd, uint8_t rn, uint8_t rm) {
  emit(encodeUdivReg(rd, rn, rm));
}

inline void Arm64Emitter::emitHeapAllocFromSlotCountReg(uint8_t slotCountReg,
                                                        uint8_t resultReg) {
  emitCompareRegZero(slotCountReg);
  const size_t jumpNonZeroSlots = emitCondBranchPlaceholder(CondCode::Ne);
  emitMovImm64(resultReg, 0);
  const size_t jumpDone = emitJumpPlaceholder();

  const size_t nonZeroSlotsIndex = currentWordIndex();
  patchCondBranch(jumpNonZeroSlots,
                  static_cast<int32_t>(nonZeroSlotsIndex - jumpNonZeroSlots),
                  CondCode::Ne);

  emitMovReg(1, slotCountReg);
  emitMovImm64(6, 1);
  emit(encodeAddReg(1, 1, 6));
  emitMovImm64(2, IrSlotBytes);
  emitMulReg(1, 1, 2);
  emitMovReg(6, 1);
  emitMovImm64(0, 0);
  emitMovImm64(2, MmapProtReadWrite);
  emitMovImm64(3, MmapFlagsPrivateAnon);
  emitMovImm64(4, ~0ull);
  emitMovImm64(5, 0);
  emitMovImm64(16, SysMmap);
  emit(encodeSvc());

  emitCompareRegZero(0);
  const size_t jumpNonNegative = emitCondBranchPlaceholder(CondCode::Ge);
  emitMovImm64(resultReg, 0);
  const size_t jumpDoneAfterFailure = emitJumpPlaceholder();

  const size_t nonNegativeIndex = currentWordIndex();
  patchCondBranch(jumpNonNegative,
                  static_cast<int32_t>(nonNegativeIndex - jumpNonNegative),
                  CondCode::Ge);

  emitMovImm64(2, HeapHeaderMagic);
  emit(encodeStrRegBase(2, 0, 0));
  emit(encodeStrRegBase(6, 0, 8));
  emitMovImm64(2, IrSlotBytes);
  emit(encodeAddReg(resultReg, 0, 2));

  const size_t doneIndex = currentWordIndex();
  patchJump(jumpDoneAfterFailure,
            static_cast<int32_t>(doneIndex - jumpDoneAfterFailure));
  patchJump(jumpDone, static_cast<int32_t>(doneIndex - jumpDone));
}

inline void Arm64Emitter::emitHeapFreeFromAddressReg(uint8_t addressReg) {
  emitCompareRegZero(addressReg);
  const size_t jumpDone = emitCondBranchPlaceholder(CondCode::Eq);

  emitMovReg(0, addressReg);
  emitMovImm64(1, IrSlotBytes);
  emitSubReg(0, 0, 1);
  emit(encodeLdrRegBase(1, 0, 0));
  emitMovImm64(2, HeapHeaderMagic);
  emitCompareReg(1, 2);
  const size_t jumpSkip = emitCondBranchPlaceholder(CondCode::Ne);
  emit(encodeLdrRegBase(1, 0, 8));
  emitMovImm64(16, SysMunmap);
  emit(encodeSvc());

  const size_t skipIndex = currentWordIndex();
  patchCondBranch(jumpSkip, static_cast<int32_t>(skipIndex - jumpSkip), CondCode::Ne);
  const size_t doneIndex = currentWordIndex();
  patchCondBranch(jumpDone, static_cast<int32_t>(doneIndex - jumpDone), CondCode::Eq);
}

inline uint32_t Arm64Emitter::encodeAddSpImm(uint16_t imm) {
  return 0x910003FF | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10);
}

inline uint32_t Arm64Emitter::encodeSubSpImm(uint16_t imm) {
  return 0xD10003FF | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10);
}

inline uint32_t Arm64Emitter::encodeAddRegImm(uint8_t rd, uint8_t rn, uint16_t imm) {
  return 0x91000000 | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10) |
         (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeSubRegImm(uint8_t rd, uint8_t rn, uint16_t imm) {
  return 0xD1000000 | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10) |
         (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeStrRegBase(uint8_t rt,
                                               uint8_t rn,
                                               uint16_t offsetBytes) {
  uint32_t imm = static_cast<uint32_t>((offsetBytes / 8) & 0xFFFu);
  return 0xF9000000 | (imm << 10) | (static_cast<uint32_t>(rn) << 5) |
         (static_cast<uint32_t>(rt) & 0x1Fu);
}

inline uint32_t Arm64Emitter::encodeLdrRegBase(uint8_t rt,
                                               uint8_t rn,
                                               uint16_t offsetBytes) {
  uint32_t imm = static_cast<uint32_t>((offsetBytes / 8) & 0xFFFu);
  return 0xF9400000 | (imm << 10) | (static_cast<uint32_t>(rn) << 5) |
         (static_cast<uint32_t>(rt) & 0x1Fu);
}

inline uint32_t Arm64Emitter::encodeAddReg(uint8_t rd, uint8_t rn, uint8_t rm) {
  return 0x8B000000 | (static_cast<uint32_t>(rm) << 16) |
         (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeSubReg(uint8_t rd, uint8_t rn, uint8_t rm) {
  return 0xCB000000 | (static_cast<uint32_t>(rm) << 16) |
         (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeSubsReg(uint8_t rd, uint8_t rn, uint8_t rm) {
  return 0xEB000000 | (static_cast<uint32_t>(rm) << 16) |
         (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeMulReg(uint8_t rd, uint8_t rn, uint8_t rm) {
  return 0x9B007C00 | (static_cast<uint32_t>(rm) << 16) |
         (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeSdivReg(uint8_t rd, uint8_t rn, uint8_t rm) {
  return 0x9AC00C00 | (static_cast<uint32_t>(rm) << 16) |
         (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeUdivReg(uint8_t rd, uint8_t rn, uint8_t rm) {
  return 0x9AC00800 | (static_cast<uint32_t>(rm) << 16) |
         (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeMovz(uint8_t rd, uint16_t imm, uint8_t shift) {
  uint32_t shiftField = static_cast<uint32_t>((shift / 16) & 0x3u);
  return 0xD2800000 | (shiftField << 21) |
         (static_cast<uint32_t>(imm) << 5) | static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeMovk(uint8_t rd, uint16_t imm, uint8_t shift) {
  uint32_t shiftField = static_cast<uint32_t>((shift / 16) & 0x3u);
  return 0xF2800000 | (shiftField << 21) |
         (static_cast<uint32_t>(imm) << 5) | static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeFmovDX(uint8_t d, uint8_t x) {
  return 0x9E670000 | (static_cast<uint32_t>(x & 0x1Fu) << 5) |
         static_cast<uint32_t>(d & 0x1Fu);
}

inline uint32_t Arm64Emitter::encodeFmovXD(uint8_t x, uint8_t d) {
  return 0x9E660000 | (static_cast<uint32_t>(d & 0x1Fu) << 5) |
         static_cast<uint32_t>(x & 0x1Fu);
}

inline uint32_t Arm64Emitter::encodeFmovSW(uint8_t s, uint8_t w) {
  return 0x1E270000 | (static_cast<uint32_t>(w & 0x1Fu) << 5) |
         static_cast<uint32_t>(s & 0x1Fu);
}

inline uint32_t Arm64Emitter::encodeFmovWS(uint8_t w, uint8_t s) {
  return 0x1E260000 | (static_cast<uint32_t>(s & 0x1Fu) << 5) |
         static_cast<uint32_t>(w & 0x1Fu);
}

inline uint32_t Arm64Emitter::encodeAdr(uint8_t rd, int32_t imm21) {
  uint32_t imm = static_cast<uint32_t>(imm21) & 0x1FFFFFu;
  uint32_t immlo = imm & 0x3u;
  uint32_t immhi = (imm >> 2) & 0x7FFFFu;
  return 0x10000000 | (immlo << 29) | (immhi << 5) |
         static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeAdrp(uint8_t rd, int32_t imm21) {
  uint32_t imm = static_cast<uint32_t>(imm21) & 0x1FFFFFu;
  uint32_t immlo = imm & 0x3u;
  uint32_t immhi = (imm >> 2) & 0x7FFFFu;
  return 0x90000000 | (immlo << 29) | (immhi << 5) |
         static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeB(int32_t imm26) {
  uint32_t imm = static_cast<uint32_t>(imm26) & 0x03FFFFFFu;
  return 0x14000000 | imm;
}

inline uint32_t Arm64Emitter::encodeBl(int32_t imm26) {
  uint32_t imm = static_cast<uint32_t>(imm26) & 0x03FFFFFFu;
  return 0x94000000 | imm;
}

inline uint32_t Arm64Emitter::encodeBCond(int32_t imm19, uint8_t cond) {
  uint32_t imm = static_cast<uint32_t>(imm19) & 0x7FFFFu;
  return 0x54000000 | (imm << 5) | (static_cast<uint32_t>(cond) & 0xFu);
}

inline uint32_t Arm64Emitter::encodeCbz(uint8_t rt, int32_t imm19) {
  uint32_t imm = static_cast<uint32_t>(imm19) & 0x7FFFFu;
  return 0xB4000000 | (imm << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
}

inline uint32_t Arm64Emitter::encodeStrbRegBase(uint8_t rt,
                                                uint8_t rn,
                                                uint16_t offsetBytes) {
  uint32_t imm = static_cast<uint32_t>(offsetBytes & 0xFFFu);
  return 0x39000000 | (imm << 10) | (static_cast<uint32_t>(rn) << 5) |
         (static_cast<uint32_t>(rt) & 0x1Fu);
}

inline uint32_t Arm64Emitter::encodeLdrbRegBase(uint8_t rt,
                                                uint8_t rn,
                                                uint16_t offsetBytes) {
  uint32_t imm = static_cast<uint32_t>(offsetBytes & 0xFFFu);
  return 0x39400000 | (imm << 10) | (static_cast<uint32_t>(rn) << 5) |
         (static_cast<uint32_t>(rt) & 0x1Fu);
}

inline uint32_t Arm64Emitter::encodeSvc() {
  return 0xD4001001;
}

inline uint32_t Arm64Emitter::encodeRet() {
  return 0xD65F03C0;
}

inline uint64_t Arm64Emitter::localOffset(uint32_t index) {
  return static_cast<uint64_t>(index) * 16u + 8u;
}

inline void Arm64Emitter::emitSpillReg(uint8_t reg) {
  counters_.spillCount += 1;
  emit(encodeSubRegImm(28, 28, 16));
  emit(encodeStrRegBase(reg, 28, 8));
}

inline void Arm64Emitter::emitReloadReg(uint8_t reg) {
  counters_.reloadCount += 1;
  emit(encodeLdrRegBase(reg, 28, 8));
  emit(encodeAddRegImm(28, 28, 16));
}

inline void Arm64Emitter::flushValueStackCache() {
  if (!hasValueStackCache_) {
    return;
  }
  emitSpillReg(valueStackCacheReg_);
  hasValueStackCache_ = false;
}
