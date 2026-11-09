#pragma once

// A text block of the popped size: a byte region of the VM's block size for it (the smallest size class
// that holds it, `size` itself above 64 KiB), zeroed, with the block size in its header. 0 for a size of
// 0 or when the mapping fails.
inline void Arm64Emitter::emitTextAlloc() {
  emitPopReg(1);
  emitCompareRegZero(1);
  const size_t jumpNonZero = emitCondBranchPlaceholder(CondCode::Ne);
  emitMovImm64(0, 0);
  const size_t jumpDoneZero = emitJumpPlaceholder();
  const size_t nonZeroIndex = currentWordIndex();
  patchCondBranch(jumpNonZero, static_cast<int32_t>(nonZeroIndex - jumpNonZero), CondCode::Ne);

  // x2 = the block size for x1 (VmTextHeap::blockSizeFor).
  emitMovReg(2, 1);
  emitMovImm64(3, 64 * 1024);
  emitCompareReg(1, 3);
  const size_t jumpLarge = emitCondBranchPlaceholder(CondCode::Hi);
  emitMovImm64(2, 32);
  emitMovImm64(3, 32);
  emitCompareReg(1, 3);
  const size_t jumpSmall = emitCondBranchPlaceholder(CondCode::Ls);
  const size_t loopIndex = currentWordIndex();
  emitAddReg(4, 3, 3);
  emitCompareReg(4, 1);
  const size_t jumpFound = emitCondBranchPlaceholder(CondCode::Hs);
  emitMovReg(3, 4);
  const size_t jumpBack = emitJumpPlaceholder();
  patchJump(jumpBack, static_cast<int32_t>(loopIndex - jumpBack));
  const size_t foundIndex = currentWordIndex();
  patchCondBranch(jumpFound, static_cast<int32_t>(foundIndex - jumpFound), CondCode::Hs);
  emit(0xD342FC64u); // LSR x4, x3, #2: a quarter of the doubling
  emitSubReg(5, 1, 3);
  emitAddReg(5, 5, 4);
  emitSubRegImm(5, 5, 1);
  emitUdivReg(5, 5, 4);
  emitMulReg(5, 5, 4);
  emitAddReg(2, 3, 5);
  const size_t sizedIndex = currentWordIndex();
  patchCondBranch(jumpLarge, static_cast<int32_t>(sizedIndex - jumpLarge), CondCode::Hi);
  patchCondBranch(jumpSmall, static_cast<int32_t>(sizedIndex - jumpSmall), CondCode::Ls);

  emitAddRegImm(6, 2, static_cast<uint16_t>(IrSlotBytes)); // the block and its header
  emitMovReg(1, 6);
  emitMovImm64(0, 0);
  emitMovImm64(2, MmapProtReadWrite);
  emitMovImm64(3, MmapFlagsPrivateAnon);
  emitMovImm64(4, ~0ull);
  emitMovImm64(5, 0);
  emitMovImm64(16, SysMmap);
  emit(encodeSvc());
  emitCompareRegZero(0);
  const size_t jumpMapped = emitCondBranchPlaceholder(CondCode::Ge);
  emitMovImm64(0, 0);
  const size_t jumpDoneFailed = emitJumpPlaceholder();
  const size_t mappedIndex = currentWordIndex();
  patchCondBranch(jumpMapped, static_cast<int32_t>(mappedIndex - jumpMapped), CondCode::Ge);
  emitMovImm64(2, HeapHeaderMagicBytes);
  emit(encodeStrRegBase(2, 0, 0));
  emit(encodeStrRegBase(6, 0, 8));
  emitAddRegImm(0, 0, static_cast<uint16_t>(IrSlotBytes));

  const size_t doneIndex = currentWordIndex();
  patchJump(jumpDoneZero, static_cast<int32_t>(doneIndex - jumpDoneZero));
  patchJump(jumpDoneFailed, static_cast<int32_t>(doneIndex - jumpDoneFailed));
  emitPushReg(0);
}

// x8 = the size of the text block at x7 (its total less the header); 0 for the null block or an address
// that is not a block.
inline void Arm64Emitter::emitTextBlockSize() {
  emitMovImm64(8, 0);
  emitCompareRegZero(7);
  const size_t jumpNull = emitCondBranchPlaceholder(CondCode::Eq);
  emitMovImm64(9, IrSlotBytes);
  emitSubReg(9, 7, 9);
  emit(encodeLdrRegBase(10, 9, 0));
  emitMovImm64(11, HeapHeaderMagicBytes);
  emitCompareReg(10, 11);
  const size_t jumpInvalid = emitCondBranchPlaceholder(CondCode::Ne);
  emit(encodeLdrRegBase(8, 9, 8));
  emitSubRegImm(8, 8, static_cast<uint16_t>(IrSlotBytes));
  const size_t endIndex = currentWordIndex();
  patchCondBranch(jumpNull, static_cast<int32_t>(endIndex - jumpNull), CondCode::Eq);
  patchCondBranch(jumpInvalid, static_cast<int32_t>(endIndex - jumpInvalid), CondCode::Ne);
}

inline void Arm64Emitter::emitTextCapacity() {
  emitPopReg(7);
  emitTextBlockSize();
  emitPushReg(8);
}

// The block size less the NUL byte (0 for the null block).
inline void Arm64Emitter::emitTextUsable() {
  emitPopReg(7);
  emitTextBlockSize();
  emitCompareRegZero(8);
  const size_t jumpEmpty = emitCondBranchPlaceholder(CondCode::Eq);
  emitSubRegImm(8, 8, 1);
  const size_t emptyIndex = currentWordIndex();
  patchCondBranch(jumpEmpty, static_cast<int32_t>(emptyIndex - jumpEmpty), CondCode::Eq);
  emitPushReg(8);
}

// The bytes before the first NUL, within the block.
inline void Arm64Emitter::emitTextLength() {
  emitPopReg(7);
  emitTextBlockSize();
  emitMovImm64(0, 0);
  const size_t loopIndex = currentWordIndex();
  emitCompareReg(0, 8);
  const size_t jumpEnd = emitCondBranchPlaceholder(CondCode::Hs);
  emitAddReg(1, 7, 0);
  emit(encodeLdrbRegBase(2, 1, 0));
  emitCompareRegZero(2);
  const size_t jumpNul = emitCondBranchPlaceholder(CondCode::Eq);
  emitAddRegImm(0, 0, 1);
  const size_t jumpBack = emitJumpPlaceholder();
  patchJump(jumpBack, static_cast<int32_t>(loopIndex - jumpBack));
  const size_t endIndex = currentWordIndex();
  patchCondBranch(jumpEnd, static_cast<int32_t>(endIndex - jumpEnd), CondCode::Hs);
  patchCondBranch(jumpNul, static_cast<int32_t>(endIndex - jumpNul), CondCode::Eq);
  emitPushReg(0);
}

// ---- bulk byte operations ----------------------------------------------------------------------------------
// Raw loops over the popped addresses (the native heap has no region table to check against): the arguments pop
// in reverse order into x0, x1, x2.

// move(destination x0, source x1, count x2): forward when the destination is not above the source, else backward.
inline void Arm64Emitter::emitTextMove() {
  emitPopReg(2);
  emitPopReg(1);
  emitPopReg(0);
  emitCompareRegZero(2);
  const size_t jumpDone = emitCondBranchPlaceholder(CondCode::Eq);
  emitCompareReg(0, 1);
  const size_t jumpBackward = emitCondBranchPlaceholder(CondCode::Hi);
  emitMovImm64(3, 0);
  const size_t forwardLoop = currentWordIndex();
  emitCompareReg(3, 2);
  const size_t jumpForwardEnd = emitCondBranchPlaceholder(CondCode::Hs);
  emitAddReg(4, 1, 3);
  emit(encodeLdrbRegBase(5, 4, 0));
  emitAddReg(4, 0, 3);
  emit(encodeStrbRegBase(5, 4, 0));
  emitAddRegImm(3, 3, 1);
  const size_t jumpForwardBack = emitJumpPlaceholder();
  patchJump(jumpForwardBack, static_cast<int32_t>(forwardLoop - jumpForwardBack));
  const size_t jumpPastBackward = emitJumpPlaceholder();
  const size_t backward = currentWordIndex();
  patchCondBranch(jumpBackward, static_cast<int32_t>(backward - jumpBackward), CondCode::Hi);
  emitMovReg(3, 2);
  const size_t backwardLoop = currentWordIndex();
  emitCompareRegZero(3);
  const size_t jumpBackwardEnd = emitCondBranchPlaceholder(CondCode::Eq);
  emitSubRegImm(3, 3, 1);
  emitAddReg(4, 1, 3);
  emit(encodeLdrbRegBase(5, 4, 0));
  emitAddReg(4, 0, 3);
  emit(encodeStrbRegBase(5, 4, 0));
  const size_t jumpBackwardBack = emitJumpPlaceholder();
  patchJump(jumpBackwardBack, static_cast<int32_t>(backwardLoop - jumpBackwardBack));
  const size_t endIndex = currentWordIndex();
  patchCondBranch(jumpDone, static_cast<int32_t>(endIndex - jumpDone), CondCode::Eq);
  patchCondBranch(jumpForwardEnd, static_cast<int32_t>(endIndex - jumpForwardEnd), CondCode::Hs);
  patchCondBranch(jumpBackwardEnd, static_cast<int32_t>(endIndex - jumpBackwardEnd), CondCode::Eq);
  patchJump(jumpPastBackward, static_cast<int32_t>(endIndex - jumpPastBackward));
}

// fill(destination x0, value x1, count x2): the low byte of the value.
inline void Arm64Emitter::emitTextFill() {
  emitPopReg(2);
  emitPopReg(1);
  emitPopReg(0);
  emitMovImm64(3, 0);
  const size_t loopIndex = currentWordIndex();
  emitCompareReg(3, 2);
  const size_t jumpEnd = emitCondBranchPlaceholder(CondCode::Hs);
  emitAddReg(4, 0, 3);
  emit(encodeStrbRegBase(1, 4, 0));
  emitAddRegImm(3, 3, 1);
  const size_t jumpBack = emitJumpPlaceholder();
  patchJump(jumpBack, static_cast<int32_t>(loopIndex - jumpBack));
  const size_t endIndex = currentWordIndex();
  patchCondBranch(jumpEnd, static_cast<int32_t>(endIndex - jumpEnd), CondCode::Hs);
}

// compare(a x0, b x1, count x2) -> -1, 0 or 1 by the first differing byte (unsigned).
inline void Arm64Emitter::emitTextCompare() {
  emitPopReg(2);
  emitPopReg(1);
  emitPopReg(0);
  emitMovImm64(7, 0);
  emitMovImm64(3, 0);
  const size_t loopIndex = currentWordIndex();
  emitCompareReg(3, 2);
  const size_t jumpEnd = emitCondBranchPlaceholder(CondCode::Hs);
  emitAddReg(4, 0, 3);
  emit(encodeLdrbRegBase(5, 4, 0));
  emitAddReg(4, 1, 3);
  emit(encodeLdrbRegBase(6, 4, 0));
  emitCompareReg(5, 6);
  const size_t jumpDiffers = emitCondBranchPlaceholder(CondCode::Ne);
  emitAddRegImm(3, 3, 1);
  const size_t jumpBack = emitJumpPlaceholder();
  patchJump(jumpBack, static_cast<int32_t>(loopIndex - jumpBack));
  const size_t differsIndex = currentWordIndex();
  patchCondBranch(jumpDiffers, static_cast<int32_t>(differsIndex - jumpDiffers), CondCode::Ne);
  emitMovImm64(7, 1);
  emitCompareReg(5, 6);
  const size_t jumpAbove = emitCondBranchPlaceholder(CondCode::Hi);
  emitMovImm64(7, ~uint64_t{0});
  const size_t endIndex = currentWordIndex();
  patchCondBranch(jumpEnd, static_cast<int32_t>(endIndex - jumpEnd), CondCode::Hs);
  patchCondBranch(jumpAbove, static_cast<int32_t>(endIndex - jumpAbove), CondCode::Hi);
  emitPushReg(7);
}

// find(address x0, count x1, value x2) -> the index of the first byte equal to the value, or -1.
inline void Arm64Emitter::emitTextFind() {
  emitPopReg(2);
  emitPopReg(1);
  emitPopReg(0);
  emit(0x53001C42u); // UXTB w2, w2: compare only the low byte
  emitMovImm64(7, ~uint64_t{0});
  emitMovImm64(3, 0);
  const size_t loopIndex = currentWordIndex();
  emitCompareReg(3, 1);
  const size_t jumpEnd = emitCondBranchPlaceholder(CondCode::Hs);
  emitAddReg(4, 0, 3);
  emit(encodeLdrbRegBase(5, 4, 0));
  emitCompareReg(5, 2);
  const size_t jumpFound = emitCondBranchPlaceholder(CondCode::Eq);
  emitAddRegImm(3, 3, 1);
  const size_t jumpBack = emitJumpPlaceholder();
  patchJump(jumpBack, static_cast<int32_t>(loopIndex - jumpBack));
  const size_t foundIndex = currentWordIndex();
  patchCondBranch(jumpFound, static_cast<int32_t>(foundIndex - jumpFound), CondCode::Eq);
  emitMovReg(7, 3);
  const size_t endIndex = currentWordIndex();
  patchCondBranch(jumpEnd, static_cast<int32_t>(endIndex - jumpEnd), CondCode::Hs);
  emitPushReg(7);
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

inline void Arm64Emitter::emitTrap(int32_t code) {
  emitMovImm64(0, static_cast<uint64_t>(static_cast<int64_t>(code)));
  emitMovImm64(16, SysExit);
  emit(encodeSvc());
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
  code_.patch(index, word);
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

// A constant push: built straight in the cache register when the cache is on.
inline void Arm64Emitter::emitPushImm64(uint64_t value) {
  if (!valueStackCacheEnabled_) {
    emitMovImm64(0, value);
    emitPushReg(0);
    return;
  }
  counters_.valueStackPushCount += 1;
  if (hasValueStackCache_) {
    emitSpillReg(valueStackCacheReg_);
  }
  emitMovImm64(valueStackCacheReg_, value);
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

// The fewest instructions that build `value`: MOVZ or MOVN of one 16-bit half plus a MOVK for
// each other half that differs from the background (0 for MOVZ, 0xFFFF for MOVN); MOVN on the w register
// when the high word is 0 and one low half is 0xFFFF; and ORR from xzr when the value is a bitmask
// immediate and every MOV form needs more than one instruction. Ties go to MOVZ, then MOVN.
inline void Arm64Emitter::emitMovImm64(uint8_t rd, uint64_t value) {
  uint16_t halves[4];
  int zeroHalves = 0;
  int onesHalves = 0;
  for (int i = 0; i < 4; ++i) {
    halves[i] = static_cast<uint16_t>((value >> (16 * i)) & 0xFFFFu);
    zeroHalves += halves[i] == 0 ? 1 : 0;
    onesHalves += halves[i] == 0xFFFF ? 1 : 0;
  }
  const int movzCount = zeroHalves == 4 ? 1 : 4 - zeroHalves;
  const int movnCount = onesHalves == 4 ? 1 : 4 - onesHalves;
  if (movzCount > 1 && movnCount > 1) {
    // movn w: the low word is ~(half << shift) and the high word is cleared.
    if ((value >> 32) == 0 && (halves[0] == 0xFFFF || halves[1] == 0xFFFF)) {
      const bool high = halves[0] == 0xFFFF;
      const uint16_t inverted = static_cast<uint16_t>(~halves[high ? 1 : 0]);
      emit(encodeMovnW(rd, inverted, high ? 16 : 0));
      return;
    }
    uint32_t logical = 0;
    if (encodeLogicalImmediate64(value, logical)) {
      emit(encodeOrrImm64(rd, 31, logical));
      return;
    }
  }
  const bool useMovn = movnCount < movzCount;
  const uint16_t background = useMovn ? 0xFFFF : 0;
  int first = 0;
  while (first < 4 && halves[first] == background) {
    ++first;
  }
  if (first == 4) {
    first = 0;
  }
  if (useMovn) {
    emit(encodeMovn(rd, static_cast<uint16_t>(~halves[first]), static_cast<uint8_t>(16 * first)));
  } else {
    emit(encodeMovz(rd, halves[first], static_cast<uint8_t>(16 * first)));
  }
  for (int i = first + 1; i < 4; ++i) {
    if (halves[i] != background) {
      emit(encodeMovk(rd, halves[i], static_cast<uint8_t>(16 * i)));
    }
  }
}

// The stack pointer moves by up to 0xFFF000 in one `#imm, lsl #12` step, then by the low 12 bits.
inline void Arm64Emitter::emitAdjustSp(uint64_t amount, bool add) {
  while (amount > 0xFFFu) {
    const uint64_t pages = std::min<uint64_t>(amount >> 12, 0xFFFu);
    emit(add ? encodeAddSpImmShifted(static_cast<uint16_t>(pages))
             : encodeSubSpImmShifted(static_cast<uint16_t>(pages)));
    amount -= pages << 12;
  }
  if (amount > 0) {
    emit(add ? encodeAddSpImm(static_cast<uint16_t>(amount))
             : encodeSubSpImm(static_cast<uint16_t>(amount)));
  }
}

// The 12-bit add/sub immediate of `value`: the value itself, or the value shifted right by 12 when its
// low 12 bits are 0 (`#imm, lsl #12`).
inline bool Arm64Emitter::addSubImmediate(uint64_t value, uint16_t &imm12, bool &shifted) {
  if (value <= 0xFFFu) {
    imm12 = static_cast<uint16_t>(value);
    shifted = false;
    return true;
  }
  if ((value & 0xFFFu) == 0 && (value >> 12) <= 0xFFFu) {
    imm12 = static_cast<uint16_t>(value >> 12);
    shifted = true;
    return true;
  }
  return false;
}

// The N:immr:imms field of a 64-bit logical immediate: `value` repeats an element of 2 to 64 bits that
// is one rotated run of ones. 0 and ~0 have no encoding.
inline bool Arm64Emitter::encodeLogicalImmediate64(uint64_t value, uint32_t &field) {
  if (value == 0 || value == ~uint64_t{0}) {
    return false;
  }
  uint32_t size = 64;
  while (size > 2) {
    const uint32_t half = size / 2;
    const uint64_t mask = (uint64_t{1} << half) - 1;
    if ((value & mask) != ((value >> half) & mask)) {
      break;
    }
    size = half;
  }
  const uint64_t elementMask = size == 64 ? ~uint64_t{0} : (uint64_t{1} << size) - 1;
  const uint64_t element = value & elementMask;
  const uint32_t ones = static_cast<uint32_t>(std::popcount(element));
  const uint64_t run = (uint64_t{1} << ones) - 1; // ones < size <= 64
  for (uint32_t rotation = 0; rotation < size; ++rotation) {
    // The element is the run rotated right by `rotation` within `size` bits.
    const uint64_t rotated =
        rotation == 0 ? run : (((run >> rotation) | (run << (size - rotation))) & elementMask);
    if (rotated == element) {
      const uint32_t n = size == 64 ? 1u : 0u;
      const uint32_t imms = ((~(size * 2 - 1)) & 0x3Fu) | (ones - 1);
      field = (n << 12) | (rotation << 6) | imms;
      return true;
    }
  }
  return false;
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

inline uint32_t Arm64Emitter::encodeAddSpImm(uint16_t imm) {
  return 0x910003FF | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10);
}

inline uint32_t Arm64Emitter::encodeSubSpImm(uint16_t imm) {
  return 0xD10003FF | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10);
}

inline uint32_t Arm64Emitter::encodeAddSpImmShifted(uint16_t imm) {
  return 0x914003FF | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10);
}

inline uint32_t Arm64Emitter::encodeSubSpImmShifted(uint16_t imm) {
  return 0xD14003FF | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10);
}

// ADD, SUB, ADDS (CMN with rd 31) and SUBS (CMP with rd 31), 64-bit, `#imm` or `#imm, lsl #12`.
inline uint32_t
Arm64Emitter::encodeAddSubImm(uint32_t base, uint8_t rd, uint8_t rn, uint16_t imm, bool shifted) {
  return base | (shifted ? (1u << 22) : 0u) | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10) |
         (static_cast<uint32_t>(rn & 0x1Fu) << 5) | static_cast<uint32_t>(rd & 0x1Fu);
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

inline uint32_t Arm64Emitter::encodeMovn(uint8_t rd, uint16_t imm, uint8_t shift) {
  uint32_t shiftField = static_cast<uint32_t>((shift / 16) & 0x3u);
  return 0x92800000 | (shiftField << 21) | (static_cast<uint32_t>(imm) << 5) |
         static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeMovnW(uint8_t rd, uint16_t imm, uint8_t shift) {
  uint32_t shiftField = static_cast<uint32_t>((shift / 16) & 0x1u);
  return 0x12800000 | (shiftField << 21) | (static_cast<uint32_t>(imm) << 5) |
         static_cast<uint32_t>(rd);
}

inline uint32_t Arm64Emitter::encodeOrrImm64(uint8_t rd, uint8_t rn, uint32_t logicalField) {
  return 0xB2000000 | ((logicalField & 0x1FFFu) << 10) | (static_cast<uint32_t>(rn & 0x1Fu) << 5) |
         static_cast<uint32_t>(rd & 0x1Fu);
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

// The value stack grows down from x28 in 16-byte slots, the top value at [x28]: a push is one
// pre-indexed store and a pop one post-indexed load.
inline void Arm64Emitter::emitSpillReg(uint8_t reg) {
  counters_.spillCount += 1;
  if (!code_.fusionWindowOpen()) {
    pendingSpills_.clear();
    pendingMoveTargets_.clear();
  }
  pendingSpills_.push_back({code_.nextIndexForFusion(), reg});
  emit(0xF81F0F80u | (static_cast<uint32_t>(reg) & 0x1Fu)); // str reg, [x28, #-16]!
  code_.openFusionWindow();
}

// A pop that follows its push with only pushes and such moves between them (no label, branch site
// or other word) moves the pushed register instead of going through memory: the push is removed
// and the move appended, unless a move after the push writes the pushed register. Nothing has
// taken the index of a word after the push, and the slot it wrote is dead once popped.
inline void Arm64Emitter::emitReloadReg(uint8_t reg) {
  if (code_.fusionWindowOpen() && !pendingSpills_.empty()) {
    const PendingSpill spill = pendingSpills_.back();
    bool clobbered = false;
    // Every move of the run counts, also one before this push (conservative).
    for (const uint8_t target : pendingMoveTargets_) {
      clobbered = clobbered || target == spill.reg;
    }
    if (!clobbered) {
      pendingSpills_.pop_back();
      code_.eraseFusedPush(spill.word);
      counters_.spillCount -= 1;
      if (reg != spill.reg) {
        emitMovReg(reg, spill.reg);
        pendingMoveTargets_.push_back(reg);
      }
      code_.openFusionWindow();
      return;
    }
  }
  code_.closeFusionWindow();
  counters_.reloadCount += 1;
  emit(0xF8410780u | (static_cast<uint32_t>(reg) & 0x1Fu)); // ldr reg, [x28], #16
}

inline void Arm64Emitter::flushValueStackCache() {
  if (!hasValueStackCache_) {
    return;
  }
  emitSpillReg(valueStackCacheReg_);
  hasValueStackCache_ = false;
}
