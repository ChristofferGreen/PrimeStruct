#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ir.h"

#if defined(__APPLE__)
#include <sys/mman.h>
#include <sys/syscall.h>
#endif

namespace primec::native_emitter {

constexpr uint64_t PageSize =
#if defined(__arm64__) || defined(__aarch64__)
    0x4000;
#else
    0x1000;
#endif
constexpr uint64_t PageZeroSize = 0x100000000ull;
constexpr uint64_t TextVmAddr = 0x100000000ull;
#if defined(__APPLE__)
constexpr uint32_t PrintScratchBytes = 32;
constexpr uint32_t PrintScratchSlots = (PrintScratchBytes + 15) / 16;
#endif
#if defined(__APPLE__)
constexpr uint64_t SysRead = SYS_read;
constexpr uint64_t SysWrite = SYS_write;
constexpr uint64_t SysOpen = SYS_open;
constexpr uint64_t SysClose = SYS_close;
constexpr uint64_t SysFsync = SYS_fsync;
#if defined(SYS_mmap)
constexpr uint64_t SysMmap = SYS_mmap;
#else
constexpr uint64_t SysMmap = 197;
#endif
constexpr uint64_t SysMunmap = SYS_munmap;
constexpr uint64_t MmapProtReadWrite = static_cast<uint64_t>(PROT_READ | PROT_WRITE);
#if defined(MAP_ANON)
constexpr uint64_t MmapFlagsPrivateAnon = static_cast<uint64_t>(MAP_PRIVATE | MAP_ANON);
#else
constexpr uint64_t MmapFlagsPrivateAnon = static_cast<uint64_t>(MAP_PRIVATE | MAP_ANONYMOUS);
#endif
#else
constexpr uint64_t SysRead = 0;
constexpr uint64_t SysWrite = 0;
constexpr uint64_t SysOpen = 0;
constexpr uint64_t SysClose = 0;
constexpr uint64_t SysFsync = 0;
constexpr uint64_t SysMmap = 0;
constexpr uint64_t SysMunmap = 0;
constexpr uint64_t MmapProtReadWrite = 0;
constexpr uint64_t MmapFlagsPrivateAnon = 0;
#endif
constexpr uint64_t HeapHeaderMagic = 0x5053484541503031ull;

inline uint64_t alignTo(uint64_t value, uint64_t alignment) {
  if (alignment == 0) {
    return value;
  }
  uint64_t mask = alignment - 1;
  return (value + mask) & ~mask;
}

struct Arm64InstrumentationCounters {
  uint64_t valueStackPushCount = 0;
  uint64_t valueStackPopCount = 0;
  uint64_t spillCount = 0;
  uint64_t reloadCount = 0;
};

class Arm64Emitter {
 public:
  static constexpr uint32_t MaxLdrStrOffsetBytes = 0xFFFu * 8u;

  void setValueStackCacheEnabled(bool enabled) {
    valueStackCacheEnabled_ = enabled;
    if (!valueStackCacheEnabled_) {
      hasValueStackCache_ = false;
    }
  }

  void flushValueStackCachePublic() {
    flushValueStackCache();
  }

  bool beginFunction(uint64_t frameSize, bool resetValueStack, std::string &error) {
    (void)error;
    hasValueStackCache_ = false;
    frameSize_ = frameSize;
    if (frameSize_ > 0) {
      emitAdjustSp(frameSize_, false);
    }
    emit(encodeAddRegImm(27, 31, 0));
    if (resetValueStack) {
      if (frameSize_ == 0) {
        emit(encodeAddRegImm(28, 27, 0));
      } else if (frameSize_ <= 4095) {
        emit(encodeAddRegImm(28, 27, static_cast<uint16_t>(frameSize_)));
      } else {
        emitMovImm64(9, frameSize_);
        emit(encodeAddReg(28, 27, 9));
      }
    }
    return true;
  }

  void emitCaptureEntryArgs() {
    emitMovReg(19, 0);
    emitMovReg(20, 1);
  }

  void emitMovRegPublic(uint8_t rd, uint8_t rn) {
    emitMovReg(rd, rn);
  }

  void emitPushI32(int32_t value) {
    emitMovImm64(0, static_cast<uint64_t>(static_cast<int64_t>(value)));
    emitPushReg(0);
  }

  void emitPushI64(uint64_t value) {
    emitMovImm64(0, value);
    emitPushReg(0);
  }

  void emitPushF32(uint32_t bits) {
    emitMovImm64(0, bits);
    emitPushReg(0);
  }

  void emitPushF64(uint64_t bits) {
    emitMovImm64(0, bits);
    emitPushReg(0);
  }

  void emitLoadLocal(uint32_t index) {
    emitLoadLocalToReg(0, index);
    emitPushReg(0);
  }

  void emitLoadLocalToReg(uint8_t reg, uint32_t index) {
    uint64_t offset = localOffset(index);
    if (offset <= MaxLdrStrOffsetBytes) {
      emit(encodeLdrRegBase(reg, 27, static_cast<uint16_t>(offset)));
    } else {
      uint8_t tmp = (reg == 1) ? 2 : 1;
      emitMovImm64(tmp, offset);
      emit(encodeAddReg(tmp, 27, tmp));
      emit(encodeLdrRegBase(reg, tmp, 0));
    }
  }

  void emitAddressOfLocal(uint32_t index) {
    uint64_t offset = localOffset(index);
    if (offset <= 4095) {
      emit(encodeAddRegImm(0, 27, static_cast<uint16_t>(offset)));
    } else {
      emitMovImm64(1, offset);
      emit(encodeAddReg(0, 27, 1));
    }
    emitPushReg(0);
  }

  void emitStoreLocal(uint32_t index) {
    emitPopReg(0);
    emitStoreLocalFromReg(index, 0);
  }

  void emitStoreLocalFromReg(uint32_t index, uint8_t reg) {
    uint64_t offset = localOffset(index);
    if (offset <= MaxLdrStrOffsetBytes) {
      emit(encodeStrRegBase(reg, 27, static_cast<uint16_t>(offset)));
    } else {
      uint8_t tmp = (reg == 1) ? 2 : 1;
      emitMovImm64(tmp, offset);
      emit(encodeAddReg(tmp, 27, tmp));
      emit(encodeStrRegBase(reg, tmp, 0));
    }
  }

  void emitLoadIndirect() {
    emitPopReg(0);
    emit(encodeLdrRegBase(1, 0, 0));
    emitPushReg(1);
  }

  void emitStoreIndirect() {
    emitPopReg(0);
    emitPopReg(1);
    emit(encodeStrRegBase(0, 1, 0));
    emitPushReg(0);
  }

  void emitHeapAlloc() {
    emitPopReg(1);
    emitHeapAllocFromSlotCountReg(1, 0);
    emitPushReg(0);
  }

  void emitHeapFree() {
    emitPopReg(0);
    emitHeapFreeFromAddressReg(0);
  }

  void emitHeapRealloc() {
    emitPopReg(1);
    emitPopReg(0);
    emitCompareRegZero(0);
    const size_t jumpOldNonNull = emitCondBranchPlaceholder(CondCode::Ne);
    emitHeapAllocFromSlotCountReg(1, 0);
    emitPushReg(0);
    const size_t jumpDone = emitJumpPlaceholder();

    const size_t oldNonNullIndex = currentWordIndex();
    patchCondBranch(jumpOldNonNull, static_cast<int32_t>(oldNonNullIndex - jumpOldNonNull), CondCode::Ne);

    emitCompareRegZero(1);
    const size_t jumpNonZeroSlots = emitCondBranchPlaceholder(CondCode::Ne);
    emitHeapFreeFromAddressReg(0);
    emitMovImm64(0, 0);
    emitPushReg(0);
    const size_t jumpDoneAfterFree = emitJumpPlaceholder();

    const size_t nonZeroSlotsIndex = currentWordIndex();
    patchCondBranch(jumpNonZeroSlots, static_cast<int32_t>(nonZeroSlotsIndex - jumpNonZeroSlots), CondCode::Ne);

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
    patchCondBranch(jumpCopyLoop, static_cast<int32_t>(copyLoopIndex - jumpCopyLoop), CondCode::Ne);

    const size_t skipCopyIndex = currentWordIndex();
    patchCondBranch(jumpSkipCopy, static_cast<int32_t>(skipCopyIndex - jumpSkipCopy), CondCode::Eq);

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
    patchJump(jumpDoneAfterAllocFailure, static_cast<int32_t>(doneIndex - jumpDoneAfterAllocFailure));
    patchJump(jumpDoneAfterCopy, static_cast<int32_t>(doneIndex - jumpDoneAfterCopy));
    patchJump(jumpDoneAfterFree, static_cast<int32_t>(doneIndex - jumpDoneAfterFree));
    patchJump(jumpDone, static_cast<int32_t>(doneIndex - jumpDone));
  }

  void emitDup();
  void emitPop();
  void emitAdd();
  void emitSub();
  void emitMul();
  void emitDiv();
  void emitDivU();
  void emitNeg();
  void emitAddF32();
  void emitSubF32();
  void emitMulF32();
  void emitDivF32();
  void emitNegF32();
  void emitAddF64();
  void emitSubF64();
  void emitMulF64();
  void emitDivF64();
  void emitNegF64();
  void emitCmpEq();
  void emitCmpNe();
  void emitCmpLt();
  void emitCmpLe();
  void emitCmpGt();
  void emitCmpGe();
  void emitCmpLtU();
  void emitCmpLeU();
  void emitCmpGtU();
  void emitCmpGeU();
  void emitCmpEqF32();
  void emitCmpNeF32();
  void emitCmpLtF32();
  void emitCmpLeF32();
  void emitCmpGtF32();
  void emitCmpGeF32();
  void emitCmpEqF64();
  void emitCmpNeF64();
  void emitCmpLtF64();
  void emitCmpLeF64();
  void emitCmpGtF64();
  void emitCmpGeF64();
  void emitConvertI32ToF32();
  void emitConvertI32ToF64();
  void emitConvertI64ToF32();
  void emitConvertI64ToF64();
  void emitConvertU64ToF32();
  void emitConvertU64ToF64();
  void emitConvertF32ToI32();
  void emitConvertF32ToI64();
  void emitConvertF32ToU64();
  void emitConvertF64ToI32();
  void emitConvertF64ToI64();
  void emitConvertF64ToU64();
  void emitConvertF32ToF64();
  void emitConvertF64ToF32();
  size_t emitJumpPlaceholder();
  size_t emitCallPlaceholder();
  size_t emitJumpIfZeroPlaceholder();
  void patchJump(size_t index, int32_t offsetWords);
  void patchCall(size_t index, int32_t offsetWords);
  void patchJumpIfZero(size_t index, int32_t offsetWords);
  void emitPushReg0();

  size_t currentWordIndex() const {
    return code_.size();
  }

  void emitReturn() {
    emitPopReg(0);
    if (frameSize_ > 0) {
      emitAdjustSp(frameSize_, true);
    }
    emit(encodeRet());
  }

  void emitReturnVoid() {
    flushValueStackCache();
    emitMovImm64(0, 0);
    if (frameSize_ > 0) {
      emitAdjustSp(frameSize_, true);
    }
    emit(encodeRet());
  }

  void emitReturnWithLink(uint32_t linkLocalIndex) {
    emitPopReg(0);
    emitLoadLocalToReg(30, linkLocalIndex);
    if (frameSize_ > 0) {
      emitAdjustSp(frameSize_, true);
    }
    emit(encodeRet());
  }

  void emitReturnVoidWithLink(uint32_t linkLocalIndex) {
    flushValueStackCache();
    emitMovImm64(0, 0);
    emitLoadLocalToReg(30, linkLocalIndex);
    if (frameSize_ > 0) {
      emitAdjustSp(frameSize_, true);
    }
    emit(encodeRet());
  }

  void emitReturnWithFrameAndLink(uint32_t frameLocalIndex, uint32_t linkLocalIndex) {
    emitPopReg(0);
    emitLoadLocalToReg(30, linkLocalIndex);
    emitLoadLocalToReg(27, frameLocalIndex);
    if (frameSize_ > 0) {
      emitAdjustSp(frameSize_, true);
    }
    emit(encodeRet());
  }

  void emitReturnVoidWithFrameAndLink(uint32_t frameLocalIndex, uint32_t linkLocalIndex) {
    flushValueStackCache();
    emitMovImm64(0, 0);
    emitLoadLocalToReg(30, linkLocalIndex);
    emitLoadLocalToReg(27, frameLocalIndex);
    if (frameSize_ > 0) {
      emitAdjustSp(frameSize_, true);
    }
    emit(encodeRet());
  }

  void emitPrintSigned(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint64_t fd);
  void emitPrintSignedRegFromValue(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint8_t fdReg);
  void emitPrintUnsigned(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint64_t fd);
  void emitPrintUnsignedRegFromValue(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint8_t fdReg);
  size_t emitPrintStringPlaceholder(uint64_t lengthBytes, uint32_t scratchOffset, bool newline, uint64_t fd);
  size_t emitPrintStringDynamicPlaceholder(uint64_t offsetTableDelta,
                                           uint64_t offsetTableSize,
                                           uint32_t scratchOffset,
                                           bool newline,
                                           uint64_t fd);
  size_t emitPrintStringPlaceholderReg(uint64_t lengthBytes, uint32_t scratchOffset, bool newline, uint8_t fdReg);
  size_t emitFileOpenPlaceholder(uint64_t flags, uint64_t mode);
  size_t emitFileOpenDynamicPlaceholder(uint64_t offsetTableDelta, uint64_t flags, uint64_t mode);
  void emitFileWriteI32(uint32_t scratchOffset, uint32_t scratchBytes);
  void emitFileWriteI64(uint32_t scratchOffset, uint32_t scratchBytes);
  void emitFileWriteU64(uint32_t scratchOffset, uint32_t scratchBytes);
  size_t emitFileWriteStringPlaceholder(uint64_t lengthBytes, uint32_t scratchOffset);
  size_t emitFileWriteStringDynamicPlaceholder(uint64_t offsetTableDelta, uint64_t offsetTableSize);
  void emitFileWriteByte(uint32_t scratchOffset);
  void emitFileReadByte(uint32_t localIndex, uint32_t scratchOffset);
  void emitFileWriteNewline(uint32_t scratchOffset);
  void emitFileClose();
  void emitFileFlush();
  size_t emitLoadStringBytePlaceholder();
  size_t emitLoadStringLengthPlaceholder(uint64_t offsetTableSize);
  void emitPrintArgv(uint32_t argcLocalIndex,
                     uint32_t argvLocalIndex,
                     uint32_t scratchOffset,
                     bool newline,
                     uint64_t fd);

  void patchAdr(size_t index, uint8_t rd, int32_t deltaBytes) {
    uint64_t instrAddr = codeBaseOffset_ + static_cast<uint64_t>(index) * 4;
    uint64_t targetAddr = instrAddr + static_cast<int64_t>(deltaBytes);
    uint64_t instrPage = instrAddr & ~0xFFFull;
    uint64_t targetPage = targetAddr & ~0xFFFull;
    int64_t pageDelta = static_cast<int64_t>(targetPage) - static_cast<int64_t>(instrPage);
    int32_t pageImm = static_cast<int32_t>(pageDelta >> 12);
    patchWord(index, encodeAdrp(rd, pageImm));
    uint16_t lo12 = static_cast<uint16_t>(targetAddr & 0xFFFu);
    patchWord(index + 1, encodeAddRegImm(rd, rd, lo12));
  }

  void setCodeBaseOffset(uint32_t offsetBytes) {
    codeBaseOffset_ = offsetBytes;
  }

  std::vector<uint8_t> finalize() const {
    std::vector<uint8_t> bytes;
    bytes.reserve(code_.size() * 4);
    for (uint32_t word : code_) {
      bytes.push_back(static_cast<uint8_t>(word & 0xFF));
      bytes.push_back(static_cast<uint8_t>((word >> 8) & 0xFF));
      bytes.push_back(static_cast<uint8_t>((word >> 16) & 0xFF));
      bytes.push_back(static_cast<uint8_t>((word >> 24) & 0xFF));
    }
    return bytes;
  }

  const Arm64InstrumentationCounters &instrumentationCounters() const {
    return counters_;
  }

 private:
  enum class CondCode : uint8_t {
    Eq = 0x0,
    Ne = 0x1,
    Hs = 0x2,
    Lo = 0x3,
    Hi = 0x8,
    Ls = 0x9,
    Ge = 0xA,
    Lt = 0xB,
    Gt = 0xC,
    Le = 0xD,
  };

  static CondCode invertCond(CondCode cond) {
    switch (cond) {
      case CondCode::Eq:
        return CondCode::Ne;
      case CondCode::Ne:
        return CondCode::Eq;
      case CondCode::Hs:
        return CondCode::Lo;
      case CondCode::Lo:
        return CondCode::Hs;
      case CondCode::Hi:
        return CondCode::Ls;
      case CondCode::Ls:
        return CondCode::Hi;
      case CondCode::Ge:
        return CondCode::Lt;
      case CondCode::Lt:
        return CondCode::Ge;
      case CondCode::Gt:
        return CondCode::Le;
      case CondCode::Le:
        return CondCode::Gt;
    }
    return CondCode::Ne;
  }

  void emit(uint32_t word) {
    code_.push_back(word);
  }

  void patchWord(size_t index, uint32_t word) {
    if (index < code_.size()) {
      code_[index] = word;
    }
  }

  void emitBinaryOp(uint32_t opWord);
  void emitFloatBinaryOp(bool isF64, uint32_t opD, uint32_t opS);
  void emitFloatUnaryOp(bool isF64, uint32_t opD, uint32_t opS);
  void emitCompareAndPushFloat(bool isF64, CondCode cond);
  void emitConvertIntToFloat(bool toF64, bool isUnsigned);
  void emitConvertFloatToInt(bool fromF64, bool isUnsigned);
  void emitConvertFloatToFloat(bool fromF64);
  void emitCompareRegZero(uint8_t reg);
  void emitCompareReg(uint8_t left, uint8_t right);
  size_t emitCondBranchPlaceholder(CondCode cond);
  void patchCondBranch(size_t index, int32_t offsetWords, CondCode cond);
  size_t emitCbzPlaceholder(uint8_t reg);
  void patchCbz(size_t index, uint8_t reg, int32_t offsetWords);
  void emitCompareAndPush(CondCode cond);

  void emitPushReg(uint8_t reg) {
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

  void emitPopReg(uint8_t reg) {
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

  void emitMovImm64(uint8_t rd, uint64_t value) {
    emit(encodeMovz(rd, static_cast<uint16_t>(value & 0xFFFF), 0));
    emit(encodeMovk(rd, static_cast<uint16_t>((value >> 16) & 0xFFFF), 16));
    emit(encodeMovk(rd, static_cast<uint16_t>((value >> 32) & 0xFFFF), 32));
    emit(encodeMovk(rd, static_cast<uint16_t>((value >> 48) & 0xFFFF), 48));
  }

  void emitAdjustSp(uint64_t amount, bool add) {
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

  size_t emitAdrPlaceholder(uint8_t rd) {
    size_t index = currentWordIndex();
    emit(encodeAdrp(rd, 0));
    emit(encodeAddRegImm(rd, rd, 0));
    return index;
  }

  void emitMovReg(uint8_t rd, uint8_t rn) {
    emit(encodeAddRegImm(rd, rn, 0));
  }

  void emitAddRegImm(uint8_t rd, uint8_t rn, uint16_t imm) {
    emit(encodeAddRegImm(rd, rn, imm));
  }

  void emitSubRegImm(uint8_t rd, uint8_t rn, uint16_t imm) {
    emit(encodeSubRegImm(rd, rn, imm));
  }

  void emitAddReg(uint8_t rd, uint8_t rn, uint8_t rm) {
    emit(encodeAddReg(rd, rn, rm));
  }

  void emitSubReg(uint8_t rd, uint8_t rn, uint8_t rm) {
    emit(encodeSubReg(rd, rn, rm));
  }

  void emitMulReg(uint8_t rd, uint8_t rn, uint8_t rm) {
    emit(encodeMulReg(rd, rn, rm));
  }

  void emitUdivReg(uint8_t rd, uint8_t rn, uint8_t rm) {
    emit(encodeUdivReg(rd, rn, rm));
  }

  void emitHeapAllocFromSlotCountReg(uint8_t slotCountReg, uint8_t resultReg) {
    emitCompareRegZero(slotCountReg);
    const size_t jumpNonZeroSlots = emitCondBranchPlaceholder(CondCode::Ne);
    emitMovImm64(resultReg, 0);
    const size_t jumpDone = emitJumpPlaceholder();

    const size_t nonZeroSlotsIndex = currentWordIndex();
    patchCondBranch(jumpNonZeroSlots, static_cast<int32_t>(nonZeroSlotsIndex - jumpNonZeroSlots), CondCode::Ne);

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
    patchCondBranch(jumpNonNegative, static_cast<int32_t>(nonNegativeIndex - jumpNonNegative), CondCode::Ge);

    emitMovImm64(2, HeapHeaderMagic);
    emit(encodeStrRegBase(2, 0, 0));
    emit(encodeStrRegBase(6, 0, 8));
    emitMovImm64(2, IrSlotBytes);
    emit(encodeAddReg(resultReg, 0, 2));

    const size_t doneIndex = currentWordIndex();
    patchJump(jumpDoneAfterFailure, static_cast<int32_t>(doneIndex - jumpDoneAfterFailure));
    patchJump(jumpDone, static_cast<int32_t>(doneIndex - jumpDone));
  }

  void emitHeapFreeFromAddressReg(uint8_t addressReg) {
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

  void emitStrbRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes);
  void emitWriteSyscall(uint64_t fd, uint8_t bufferReg, uint8_t lengthReg);
  void emitWriteSyscallReg(uint8_t fdReg, uint8_t bufferReg, uint8_t lengthReg);
  void emitReadSyscallReg(uint8_t fdReg, uint8_t bufferReg, uint8_t lengthReg);
  void emitWriteNewline(uint64_t fd, uint32_t scratchOffset);
  void emitWriteNewlineReg(uint8_t fdReg, uint32_t scratchOffset);
  void emitLoadFrameOffset(uint8_t rd, uint32_t offsetBytes);
  void emitAddOffset(uint8_t rd, uint8_t rn, uint32_t offsetBytes);
  void emitPrintUnsignedInternal(uint32_t scratchOffset,
                                 uint32_t scratchBytes,
                                 bool includeSign,
                                 uint8_t signReg,
                                 bool newline,
                                 uint64_t fd);
  void emitPrintUnsignedInternalReg(uint32_t scratchOffset,
                                    uint32_t scratchBytes,
                                    bool includeSign,
                                    uint8_t signReg,
                                    bool newline,
                                    uint8_t fdReg);

  static constexpr uint32_t kFaddD0D0D1 = 0x1e612800;
  static constexpr uint32_t kFsubD0D0D1 = 0x1e613800;
  static constexpr uint32_t kFmulD0D0D1 = 0x1e610800;
  static constexpr uint32_t kFdivD0D0D1 = 0x1e611800;
  static constexpr uint32_t kFnegD0 = 0x1e614000;
  static constexpr uint32_t kFcmpD0D1 = 0x1e612000;
  static constexpr uint32_t kFaddS0S0S1 = 0x1e212800;
  static constexpr uint32_t kFsubS0S0S1 = 0x1e213800;
  static constexpr uint32_t kFmulS0S0S1 = 0x1e210800;
  static constexpr uint32_t kFdivS0S0S1 = 0x1e211800;
  static constexpr uint32_t kFnegS0 = 0x1e214000;
  static constexpr uint32_t kFcmpS0S1 = 0x1e212000;
  static constexpr uint32_t kScvtfD0X0 = 0x9e620000;
  static constexpr uint32_t kScvtfS0X0 = 0x9e220000;
  static constexpr uint32_t kUcvtfD0X0 = 0x9e630000;
  static constexpr uint32_t kUcvtfS0X0 = 0x9e230000;
  static constexpr uint32_t kFcvtzsX0D0 = 0x9e780000;
  static constexpr uint32_t kFcvtzuX0D0 = 0x9e790000;
  static constexpr uint32_t kFcvtD0S0 = 0x1e22c000;
  static constexpr uint32_t kFcvtS0D0 = 0x1e624000;

  static uint32_t encodeAddSpImm(uint16_t imm) {
    return 0x910003FF | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10);
  }

  static uint32_t encodeSubSpImm(uint16_t imm) {
    return 0xD10003FF | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10);
  }

  static uint32_t encodeAddRegImm(uint8_t rd, uint8_t rn, uint16_t imm) {
    return 0x91000000 | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10) |
           (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
  }

  static uint32_t encodeSubRegImm(uint8_t rd, uint8_t rn, uint16_t imm) {
    return 0xD1000000 | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10) |
           (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
  }

  static uint32_t encodeStrRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes) {
    uint32_t imm = static_cast<uint32_t>((offsetBytes / 8) & 0xFFFu);
    return 0xF9000000 | (imm << 10) | (static_cast<uint32_t>(rn) << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
  }

  static uint32_t encodeLdrRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes) {
    uint32_t imm = static_cast<uint32_t>((offsetBytes / 8) & 0xFFFu);
    return 0xF9400000 | (imm << 10) | (static_cast<uint32_t>(rn) << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
  }

  static uint32_t encodeAddReg(uint8_t rd, uint8_t rn, uint8_t rm) {
    return 0x8B000000 | (static_cast<uint32_t>(rm) << 16) |
           (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
  }

  static uint32_t encodeSubReg(uint8_t rd, uint8_t rn, uint8_t rm) {
    return 0xCB000000 | (static_cast<uint32_t>(rm) << 16) |
           (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
  }

  static uint32_t encodeSubsReg(uint8_t rd, uint8_t rn, uint8_t rm) {
    return 0xEB000000 | (static_cast<uint32_t>(rm) << 16) |
           (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
  }

  static uint32_t encodeMulReg(uint8_t rd, uint8_t rn, uint8_t rm) {
    return 0x9B007C00 | (static_cast<uint32_t>(rm) << 16) |
           (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
  }

  static uint32_t encodeSdivReg(uint8_t rd, uint8_t rn, uint8_t rm) {
    return 0x9AC00C00 | (static_cast<uint32_t>(rm) << 16) |
           (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
  }

  static uint32_t encodeUdivReg(uint8_t rd, uint8_t rn, uint8_t rm) {
    return 0x9AC00800 | (static_cast<uint32_t>(rm) << 16) |
           (static_cast<uint32_t>(rn) << 5) | static_cast<uint32_t>(rd);
  }

  static uint32_t encodeMovz(uint8_t rd, uint16_t imm, uint8_t shift) {
    uint32_t shiftField = static_cast<uint32_t>((shift / 16) & 0x3u);
    return 0xD2800000 | (shiftField << 21) | (static_cast<uint32_t>(imm) << 5) |
           static_cast<uint32_t>(rd);
  }

  static uint32_t encodeMovk(uint8_t rd, uint16_t imm, uint8_t shift) {
    uint32_t shiftField = static_cast<uint32_t>((shift / 16) & 0x3u);
    return 0xF2800000 | (shiftField << 21) | (static_cast<uint32_t>(imm) << 5) |
           static_cast<uint32_t>(rd);
  }

  static uint32_t encodeFmovDX(uint8_t d, uint8_t x) {
    return 0x9E670000 | (static_cast<uint32_t>(x & 0x1Fu) << 5) | static_cast<uint32_t>(d & 0x1Fu);
  }

  static uint32_t encodeFmovXD(uint8_t x, uint8_t d) {
    return 0x9E660000 | (static_cast<uint32_t>(d & 0x1Fu) << 5) | static_cast<uint32_t>(x & 0x1Fu);
  }

  static uint32_t encodeFmovSW(uint8_t s, uint8_t w) {
    return 0x1E270000 | (static_cast<uint32_t>(w & 0x1Fu) << 5) | static_cast<uint32_t>(s & 0x1Fu);
  }

  static uint32_t encodeFmovWS(uint8_t w, uint8_t s) {
    return 0x1E260000 | (static_cast<uint32_t>(s & 0x1Fu) << 5) | static_cast<uint32_t>(w & 0x1Fu);
  }

  static uint32_t encodeAdr(uint8_t rd, int32_t imm21) {
    uint32_t imm = static_cast<uint32_t>(imm21) & 0x1FFFFFu;
    uint32_t immlo = imm & 0x3u;
    uint32_t immhi = (imm >> 2) & 0x7FFFFu;
    return 0x10000000 | (immlo << 29) | (immhi << 5) | static_cast<uint32_t>(rd);
  }

  static uint32_t encodeAdrp(uint8_t rd, int32_t imm21) {
    uint32_t imm = static_cast<uint32_t>(imm21) & 0x1FFFFFu;
    uint32_t immlo = imm & 0x3u;
    uint32_t immhi = (imm >> 2) & 0x7FFFFu;
    return 0x90000000 | (immlo << 29) | (immhi << 5) | static_cast<uint32_t>(rd);
  }

  static uint32_t encodeB(int32_t imm26) {
    uint32_t imm = static_cast<uint32_t>(imm26) & 0x03FFFFFFu;
    return 0x14000000 | imm;
  }

  static uint32_t encodeBl(int32_t imm26) {
    uint32_t imm = static_cast<uint32_t>(imm26) & 0x03FFFFFFu;
    return 0x94000000 | imm;
  }

  static uint32_t encodeBCond(int32_t imm19, uint8_t cond) {
    uint32_t imm = static_cast<uint32_t>(imm19) & 0x7FFFFu;
    return 0x54000000 | (imm << 5) | (static_cast<uint32_t>(cond) & 0xFu);
  }

  static uint32_t encodeCbz(uint8_t rt, int32_t imm19) {
    uint32_t imm = static_cast<uint32_t>(imm19) & 0x7FFFFu;
    return 0xB4000000 | (imm << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
  }

  static uint32_t encodeStrbRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes) {
    uint32_t imm = static_cast<uint32_t>(offsetBytes & 0xFFFu);
    return 0x39000000 | (imm << 10) | (static_cast<uint32_t>(rn) << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
  }

  static uint32_t encodeLdrbRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes) {
    uint32_t imm = static_cast<uint32_t>(offsetBytes & 0xFFFu);
    return 0x39400000 | (imm << 10) | (static_cast<uint32_t>(rn) << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
  }

  static uint32_t encodeSvc() {
    return 0xD4001001;
  }

  static uint32_t encodeRet() {
    return 0xD65F03C0;
  }

  static uint64_t localOffset(uint32_t index) {
    return static_cast<uint64_t>(index) * 16u + 8u;
  }

  void emitSpillReg(uint8_t reg) {
    counters_.spillCount += 1;
    emit(encodeSubRegImm(28, 28, 16));
    emit(encodeStrRegBase(reg, 28, 8));
  }

  void emitReloadReg(uint8_t reg) {
    counters_.reloadCount += 1;
    emit(encodeLdrRegBase(reg, 28, 8));
    emit(encodeAddRegImm(28, 28, 16));
  }

  void flushValueStackCache() {
    if (!hasValueStackCache_) {
      return;
    }
    emitSpillReg(valueStackCacheReg_);
    hasValueStackCache_ = false;
  }

  std::vector<uint32_t> code_;
  uint64_t frameSize_ = 0;
  uint64_t codeBaseOffset_ = 0;
  Arm64InstrumentationCounters counters_;
  static constexpr uint8_t valueStackCacheReg_ = 26;
  bool hasValueStackCache_ = false;
  bool valueStackCacheEnabled_ = true;
};

#include "NativeEmitterInternalsArm64Arithmetic.h"
#include "NativeEmitterInternalsArm64Io.h"

bool computeMaxStackDepth(const IrFunction &fn, int64_t &maxDepth, std::string &error);

bool writeBinaryFile(const std::string &path, const std::vector<uint8_t> &data, std::string &error);

bool buildMachO(const std::vector<uint8_t> &code, std::vector<uint8_t> &image, std::string &error);

#if defined(__APPLE__)
uint32_t computeMachOCodeOffset();
#endif

} // namespace primec::native_emitter
