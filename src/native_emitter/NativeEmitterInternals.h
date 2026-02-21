#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ir.h"

#if defined(__APPLE__)
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
constexpr uint64_t SysWrite = SYS_write;
constexpr uint64_t SysOpen = SYS_open;
constexpr uint64_t SysClose = SYS_close;
constexpr uint64_t SysFsync = SYS_fsync;
#else
constexpr uint64_t SysWrite = 0;
constexpr uint64_t SysOpen = 0;
constexpr uint64_t SysClose = 0;
constexpr uint64_t SysFsync = 0;
#endif

inline uint64_t alignTo(uint64_t value, uint64_t alignment) {
  if (alignment == 0) {
    return value;
  }
  uint64_t mask = alignment - 1;
  return (value + mask) & ~mask;
}

class Arm64Emitter {
 public:
  static constexpr uint32_t MaxLdrStrOffsetBytes = 0xFFFu * 8u;

  bool beginFunction(uint64_t frameSize, std::string &error) {
    (void)error;
    frameSize_ = frameSize;
    if (frameSize_ > 0) {
      emitAdjustSp(frameSize_, false);
    }
    emit(encodeAddRegImm(27, 31, 0));
    if (frameSize_ == 0) {
      emit(encodeAddRegImm(28, 27, 0));
    } else if (frameSize_ <= 4095) {
      emit(encodeAddRegImm(28, 27, static_cast<uint16_t>(frameSize_)));
    } else {
      emitMovImm64(9, frameSize_);
      emit(encodeAddReg(28, 27, 9));
    }
    return true;
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

  void emitDup() {
    emitPopReg(0);
    emitPushReg(0);
    emitPushReg(0);
  }

  void emitPop() {
    emitPopReg(0);
  }

  void emitAdd() {
    emitBinaryOp(encodeAddReg(0, 1, 0));
  }

  void emitSub() {
    emitBinaryOp(encodeSubReg(0, 1, 0));
  }

  void emitMul() {
    emitBinaryOp(encodeMulReg(0, 1, 0));
  }

  void emitDiv() {
    emitBinaryOp(encodeSdivReg(0, 1, 0));
  }

  void emitDivU() {
    emitBinaryOp(encodeUdivReg(0, 1, 0));
  }

  void emitNeg() {
    emitPopReg(0);
    emit(encodeSubReg(0, 31, 0));
    emitPushReg(0);
  }

  void emitAddF32() {
    emitFloatBinaryOp(false, kFaddD0D0D1, kFaddS0S0S1);
  }

  void emitSubF32() {
    emitFloatBinaryOp(false, kFsubD0D0D1, kFsubS0S0S1);
  }

  void emitMulF32() {
    emitFloatBinaryOp(false, kFmulD0D0D1, kFmulS0S0S1);
  }

  void emitDivF32() {
    emitFloatBinaryOp(false, kFdivD0D0D1, kFdivS0S0S1);
  }

  void emitNegF32() {
    emitFloatUnaryOp(false, kFnegD0, kFnegS0);
  }

  void emitAddF64() {
    emitFloatBinaryOp(true, kFaddD0D0D1, kFaddS0S0S1);
  }

  void emitSubF64() {
    emitFloatBinaryOp(true, kFsubD0D0D1, kFsubS0S0S1);
  }

  void emitMulF64() {
    emitFloatBinaryOp(true, kFmulD0D0D1, kFmulS0S0S1);
  }

  void emitDivF64() {
    emitFloatBinaryOp(true, kFdivD0D0D1, kFdivS0S0S1);
  }

  void emitNegF64() {
    emitFloatUnaryOp(true, kFnegD0, kFnegS0);
  }

  void emitCmpEq() {
    emitCompareAndPush(CondCode::Eq);
  }

  void emitCmpNe() {
    emitCompareAndPush(CondCode::Ne);
  }

  void emitCmpLt() {
    emitCompareAndPush(CondCode::Lt);
  }

  void emitCmpLe() {
    emitCompareAndPush(CondCode::Le);
  }

  void emitCmpGt() {
    emitCompareAndPush(CondCode::Gt);
  }

  void emitCmpGe() {
    emitCompareAndPush(CondCode::Ge);
  }

  void emitCmpLtU() {
    emitCompareAndPush(CondCode::Lo);
  }

  void emitCmpLeU() {
    emitCompareAndPush(CondCode::Ls);
  }

  void emitCmpGtU() {
    emitCompareAndPush(CondCode::Hi);
  }

  void emitCmpGeU() {
    emitCompareAndPush(CondCode::Hs);
  }

  void emitCmpEqF32() {
    emitCompareAndPushFloat(false, CondCode::Eq);
  }

  void emitCmpNeF32() {
    emitCompareAndPushFloat(false, CondCode::Ne);
  }

  void emitCmpLtF32() {
    emitCompareAndPushFloat(false, CondCode::Lt);
  }

  void emitCmpLeF32() {
    emitCompareAndPushFloat(false, CondCode::Le);
  }

  void emitCmpGtF32() {
    emitCompareAndPushFloat(false, CondCode::Gt);
  }

  void emitCmpGeF32() {
    emitCompareAndPushFloat(false, CondCode::Ge);
  }

  void emitCmpEqF64() {
    emitCompareAndPushFloat(true, CondCode::Eq);
  }

  void emitCmpNeF64() {
    emitCompareAndPushFloat(true, CondCode::Ne);
  }

  void emitCmpLtF64() {
    emitCompareAndPushFloat(true, CondCode::Lt);
  }

  void emitCmpLeF64() {
    emitCompareAndPushFloat(true, CondCode::Le);
  }

  void emitCmpGtF64() {
    emitCompareAndPushFloat(true, CondCode::Gt);
  }

  void emitCmpGeF64() {
    emitCompareAndPushFloat(true, CondCode::Ge);
  }

  void emitConvertI32ToF32() {
    emitConvertIntToFloat(false, false);
  }

  void emitConvertI32ToF64() {
    emitConvertIntToFloat(true, false);
  }

  void emitConvertI64ToF32() {
    emitConvertIntToFloat(false, false);
  }

  void emitConvertI64ToF64() {
    emitConvertIntToFloat(true, false);
  }

  void emitConvertU64ToF32() {
    emitConvertIntToFloat(false, true);
  }

  void emitConvertU64ToF64() {
    emitConvertIntToFloat(true, true);
  }

  void emitConvertF32ToI32() {
    emitConvertFloatToInt(false, false);
  }

  void emitConvertF32ToI64() {
    emitConvertFloatToInt(false, false);
  }

  void emitConvertF32ToU64() {
    emitConvertFloatToInt(false, true);
  }

  void emitConvertF64ToI32() {
    emitConvertFloatToInt(true, false);
  }

  void emitConvertF64ToI64() {
    emitConvertFloatToInt(true, false);
  }

  void emitConvertF64ToU64() {
    emitConvertFloatToInt(true, true);
  }

  void emitConvertF32ToF64() {
    emitConvertFloatToFloat(false);
  }

  void emitConvertF64ToF32() {
    emitConvertFloatToFloat(true);
  }

  size_t emitJumpPlaceholder() {
    size_t index = currentWordIndex();
    emit(encodeB(0));
    return index;
  }

  size_t emitJumpIfZeroPlaceholder() {
    emitPopReg(0);
    emitCompareRegZero(0);
    return emitCondBranchPlaceholder(CondCode::Eq);
  }

  void patchJump(size_t index, int32_t offsetWords) {
    patchWord(index, encodeB(offsetWords));
  }

  void patchJumpIfZero(size_t index, int32_t offsetWords) {
    patchCondBranch(index, offsetWords, CondCode::Eq);
  }

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
    emitMovImm64(0, 0);
    if (frameSize_ > 0) {
      emitAdjustSp(frameSize_, true);
    }
    emit(encodeRet());
  }

  void emitPrintSigned(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint64_t fd) {
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
  void emitPrintSignedRegFromValue(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint8_t fdReg) {
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

  void emitPrintUnsigned(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint64_t fd) {
    emitPopReg(0);
    emitPrintUnsignedInternal(scratchOffset, scratchBytes, false, 0, newline, fd);
  }
  void emitPrintUnsignedRegFromValue(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint8_t fdReg) {
    emitPrintUnsignedInternalReg(scratchOffset, scratchBytes, false, 0, newline, fdReg);
  }

  size_t emitPrintStringPlaceholder(uint64_t lengthBytes, uint32_t scratchOffset, bool newline, uint64_t fd) {
    size_t fixupIndex = emitAdrPlaceholder(1);
    emitMovImm64(2, lengthBytes);
    emitWriteSyscall(fd, 1, 2);
    if (newline) {
      emitWriteNewline(fd, scratchOffset);
    }
    return fixupIndex;
  }
  size_t emitPrintStringPlaceholderReg(uint64_t lengthBytes, uint32_t scratchOffset, bool newline, uint8_t fdReg) {
    size_t fixupIndex = emitAdrPlaceholder(1);
    emitMovImm64(2, lengthBytes);
    emitWriteSyscallReg(fdReg, 1, 2);
    if (newline) {
      emitWriteNewlineReg(fdReg, scratchOffset);
    }
    return fixupIndex;
  }
  size_t emitFileOpenPlaceholder(uint64_t flags, uint64_t mode) {
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
  void emitFileWriteI32(uint32_t scratchOffset, uint32_t scratchBytes) {
    emitPopReg(0);
    emitPopReg(1);
    emitMovReg(6, 1);
    emitPrintSignedRegFromValue(scratchOffset, scratchBytes, false, 6);
    emitMovImm64(0, 0);
    emitPushReg(0);
  }
  void emitFileWriteI64(uint32_t scratchOffset, uint32_t scratchBytes) {
    emitPopReg(0);
    emitPopReg(1);
    emitMovReg(6, 1);
    emitPrintSignedRegFromValue(scratchOffset, scratchBytes, false, 6);
    emitMovImm64(0, 0);
    emitPushReg(0);
  }
  void emitFileWriteU64(uint32_t scratchOffset, uint32_t scratchBytes) {
    emitPopReg(0);
    emitPopReg(1);
    emitMovReg(6, 1);
    emitPrintUnsignedRegFromValue(scratchOffset, scratchBytes, false, 6);
    emitMovImm64(0, 0);
    emitPushReg(0);
  }
  size_t emitFileWriteStringPlaceholder(uint64_t lengthBytes, uint32_t scratchOffset) {
    emitPopReg(3);
    size_t fixupIndex = emitPrintStringPlaceholderReg(lengthBytes, scratchOffset, false, 3);
    emitMovImm64(0, 0);
    emitPushReg(0);
    return fixupIndex;
  }
  void emitFileWriteByte(uint32_t scratchOffset) {
    emitPopReg(0);
    emitPopReg(3);
    emitLoadFrameOffset(1, scratchOffset);
    emitStrbRegBase(0, 1, 0);
    emitMovImm64(2, 1);
    emitWriteSyscallReg(3, 1, 2);
    emitMovImm64(0, 0);
    emitPushReg(0);
  }
  void emitFileWriteNewline(uint32_t scratchOffset) {
    emitPopReg(3);
    emitWriteNewlineReg(3, scratchOffset);
    emitMovImm64(0, 0);
    emitPushReg(0);
  }
  void emitFileClose() {
    emitPopReg(0);
    emitMovImm64(16, SysClose);
    emit(encodeSvc());
    emitMovImm64(0, 0);
    emitPushReg(0);
  }
  void emitFileFlush() {
    emitPopReg(0);
    emitMovImm64(16, SysFsync);
    emit(encodeSvc());
    emitMovImm64(0, 0);
    emitPushReg(0);
  }

  size_t emitLoadStringBytePlaceholder() {
    emitPopReg(0);
    size_t fixupIndex = emitAdrPlaceholder(1);
    emitAddReg(1, 1, 0);
    emit(encodeLdrbRegBase(2, 1, 0));
    emitPushReg(2);
    return fixupIndex;
  }

  void emitPrintArgv(uint32_t argcLocalIndex,
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

  void emitBinaryOp(uint32_t opWord) {
    emitPopReg(0);
    emitPopReg(1);
    emit(opWord);
    emitPushReg(0);
  }

  void emitFloatBinaryOp(bool isF64, uint32_t opD, uint32_t opS) {
    emitPopReg(0);
    emitPopReg(1);
    if (isF64) {
      emit(encodeFmovDX(0, 1));
      emit(encodeFmovDX(1, 0));
      emit(opD);
      emit(encodeFmovXD(0, 0));
    } else {
      emit(encodeFmovSW(0, 1));
      emit(encodeFmovSW(1, 0));
      emit(opS);
      emit(encodeFmovWS(0, 0));
    }
    emitPushReg(0);
  }

  void emitFloatUnaryOp(bool isF64, uint32_t opD, uint32_t opS) {
    emitPopReg(0);
    if (isF64) {
      emit(encodeFmovDX(0, 0));
      emit(opD);
      emit(encodeFmovXD(0, 0));
    } else {
      emit(encodeFmovSW(0, 0));
      emit(opS);
      emit(encodeFmovWS(0, 0));
    }
    emitPushReg(0);
  }

  void emitCompareAndPushFloat(bool isF64, CondCode cond) {
    emitPopReg(0);
    emitPopReg(1);
    if (isF64) {
      emit(encodeFmovDX(0, 1));
      emit(encodeFmovDX(1, 0));
      emit(kFcmpD0D1);
    } else {
      emit(encodeFmovSW(0, 1));
      emit(encodeFmovSW(1, 0));
      emit(kFcmpS0S1);
    }
    emit(encodeBCond(6, static_cast<uint8_t>(cond)));
    emitMovImm64(0, 0);
    emit(encodeB(5));
    emitMovImm64(0, 1);
    emitPushReg(0);
  }

  void emitConvertIntToFloat(bool toF64, bool isUnsigned) {
    emitPopReg(0);
    if (isUnsigned) {
      emit(toF64 ? kUcvtfD0X0 : kUcvtfS0X0);
    } else {
      emit(toF64 ? kScvtfD0X0 : kScvtfS0X0);
    }
    if (toF64) {
      emit(encodeFmovXD(0, 0));
    } else {
      emit(encodeFmovWS(0, 0));
    }
    emitPushReg(0);
  }

  void emitConvertFloatToInt(bool fromF64, bool isUnsigned) {
    emitPopReg(0);
    if (fromF64) {
      emit(encodeFmovDX(0, 0));
    } else {
      emit(encodeFmovSW(0, 0));
      emit(kFcvtD0S0);
    }
    emit(isUnsigned ? kFcvtzuX0D0 : kFcvtzsX0D0);
    emitPushReg(0);
  }

  void emitConvertFloatToFloat(bool fromF64) {
    emitPopReg(0);
    if (fromF64) {
      emit(encodeFmovDX(0, 0));
      emit(kFcvtS0D0);
      emit(encodeFmovWS(0, 0));
    } else {
      emit(encodeFmovSW(0, 0));
      emit(kFcvtD0S0);
      emit(encodeFmovXD(0, 0));
    }
    emitPushReg(0);
  }

  void emitCompareRegZero(uint8_t reg) {
    emit(encodeSubsReg(31, reg, 31));
  }

  void emitCompareReg(uint8_t left, uint8_t right) {
    emit(encodeSubsReg(31, left, right));
  }

  size_t emitCondBranchPlaceholder(CondCode cond) {
    size_t index = currentWordIndex();
    emit(encodeBCond(0, static_cast<uint8_t>(cond)));
    emit(encodeB(0));
    return index;
  }

  void patchCondBranch(size_t index, int32_t offsetWords, CondCode cond) {
    constexpr int32_t kMinCond = -(1 << 18);
    constexpr int32_t kMaxCond = (1 << 18) - 1;
    if (offsetWords >= kMinCond && offsetWords <= kMaxCond) {
      patchWord(index, encodeBCond(offsetWords, static_cast<uint8_t>(cond)));
      patchWord(index + 1, encodeB(1));
      return;
    }
    CondCode inverted = invertCond(cond);
    patchWord(index, encodeBCond(2, static_cast<uint8_t>(inverted)));
    patchWord(index + 1, encodeB(offsetWords - 1));
  }

  size_t emitCbzPlaceholder(uint8_t reg) {
    size_t index = currentWordIndex();
    emit(encodeCbz(reg, 0));
    return index;
  }

  void patchCbz(size_t index, uint8_t reg, int32_t offsetWords) {
    patchWord(index, encodeCbz(reg, offsetWords));
  }

  void emitCompareAndPush(CondCode cond) {
    emitPopReg(0);
    emitPopReg(1);
    emit(encodeSubsReg(31, 1, 0));
    // Offsets assume emitMovImm64 emits 4 instructions and emitPushReg emits 2.
    emit(encodeBCond(6, static_cast<uint8_t>(cond)));
    emitMovImm64(0, 0);
    emit(encodeB(5));
    emitMovImm64(0, 1);
    emitPushReg(0);
  }

  void emitPushReg(uint8_t reg) {
    emit(encodeSubRegImm(28, 28, 16));
    emit(encodeStrRegBase(reg, 28, 8));
  }

  void emitPopReg(uint8_t reg) {
    emit(encodeLdrRegBase(reg, 28, 8));
    emit(encodeAddRegImm(28, 28, 16));
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

  void emitStrbRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes) {
    emit(encodeStrbRegBase(rt, rn, offsetBytes));
  }

  void emitWriteSyscall(uint64_t fd, uint8_t bufferReg, uint8_t lengthReg) {
    emitMovImm64(0, fd);
    emitMovReg(1, bufferReg);
    emitMovReg(2, lengthReg);
    emitMovImm64(16, SysWrite);
    emit(encodeSvc());
  }
  void emitWriteSyscallReg(uint8_t fdReg, uint8_t bufferReg, uint8_t lengthReg) {
    emitMovReg(0, fdReg);
    emitMovReg(1, bufferReg);
    emitMovReg(2, lengthReg);
    emitMovImm64(16, SysWrite);
    emit(encodeSvc());
  }

  void emitWriteNewline(uint64_t fd, uint32_t scratchOffset) {
    emitLoadFrameOffset(1, scratchOffset);
    emitMovImm64(4, '\n');
    emitStrbRegBase(4, 1, 0);
    emitMovImm64(2, 1);
    emitWriteSyscall(fd, 1, 2);
  }
  void emitWriteNewlineReg(uint8_t fdReg, uint32_t scratchOffset) {
    emitLoadFrameOffset(1, scratchOffset);
    emitMovImm64(4, '\n');
    emitStrbRegBase(4, 1, 0);
    emitMovImm64(2, 1);
    emitWriteSyscallReg(fdReg, 1, 2);
  }

  void emitLoadFrameOffset(uint8_t rd, uint32_t offsetBytes) {
    if (offsetBytes <= 4095) {
      emitAddRegImm(rd, 27, static_cast<uint16_t>(offsetBytes));
      return;
    }
    emitMovImm64(9, offsetBytes);
    emitAddReg(rd, 27, 9);
  }

  void emitAddOffset(uint8_t rd, uint8_t rn, uint32_t offsetBytes) {
    if (offsetBytes <= 4095) {
      emitAddRegImm(rd, rn, static_cast<uint16_t>(offsetBytes));
      return;
    }
    emitMovImm64(9, offsetBytes);
    emitAddReg(rd, rn, 9);
  }

  void emitPrintUnsignedInternal(uint32_t scratchOffset,
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
  void emitPrintUnsignedInternalReg(uint32_t scratchOffset,
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

  std::vector<uint32_t> code_;
  uint64_t frameSize_ = 0;
  uint64_t codeBaseOffset_ = 0;
};

bool computeMaxStackDepth(const IrFunction &fn, int64_t &maxDepth, std::string &error);

bool writeBinaryFile(const std::string &path, const std::vector<uint8_t> &data, std::string &error);

bool buildMachO(const std::vector<uint8_t> &code, std::vector<uint8_t> &image, std::string &error);

#if defined(__APPLE__)
uint32_t computeMachOCodeOffset();
#endif

} // namespace primec::native_emitter
