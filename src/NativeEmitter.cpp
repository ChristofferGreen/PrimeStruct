#include "primec/NativeEmitter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach/machine.h>
#include <mach-o/loader.h>
#include <sys/syscall.h>
#endif

namespace primec {
namespace {

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
#else
constexpr uint64_t SysWrite = 0;
#endif

uint64_t alignTo(uint64_t value, uint64_t alignment) {
  if (alignment == 0) {
    return value;
  }
  uint64_t mask = alignment - 1;
  return (value + mask) & ~mask;
}

class Arm64Emitter {
 public:
  bool beginFunction(uint64_t frameSize, std::string &error) {
    frameSize_ = frameSize;
    if (frameSize_ > 4095) {
      error = "native backend frame size too large";
      return false;
    }
    if (frameSize_ > 0) {
      emit(encodeSubSpImm(static_cast<uint16_t>(frameSize_)));
    }
    emit(encodeAddRegImm(27, 31, 0));
    emit(encodeAddRegImm(28, 31, static_cast<uint16_t>(frameSize_)));
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

  void emitLoadLocal(uint32_t index) {
    emit(encodeLdrRegBase(0, 27, localOffset(index)));
    emitPushReg(0);
  }

  void emitAddressOfLocal(uint32_t index) {
    uint32_t offset = localOffset(index);
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
    emit(encodeStrRegBase(0, 27, localOffset(index)));
  }

  void emitStoreLocalFromReg(uint32_t index, uint8_t reg) {
    emit(encodeStrRegBase(reg, 27, localOffset(index)));
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

  size_t emitJumpPlaceholder() {
    size_t index = currentWordIndex();
    emit(encodeB(0));
    return index;
  }

  size_t emitJumpIfZeroPlaceholder() {
    emitPopReg(0);
    size_t index = currentWordIndex();
    emit(encodeCbz(0, 0));
    return index;
  }

  void patchJump(size_t index, int32_t offsetWords) {
    patchWord(index, encodeB(offsetWords));
  }

  void patchJumpIfZero(size_t index, int32_t offsetWords) {
    patchWord(index, encodeCbz(0, offsetWords));
  }

  size_t currentWordIndex() const {
    return code_.size();
  }

  void emitReturn() {
    emitPopReg(0);
    if (frameSize_ > 0) {
      emit(encodeAddSpImm(static_cast<uint16_t>(frameSize_)));
    }
    emit(encodeRet());
  }

  void emitReturnVoid() {
    emitMovImm64(0, 0);
    if (frameSize_ > 0) {
      emit(encodeAddSpImm(static_cast<uint16_t>(frameSize_)));
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

  void emitPrintUnsigned(uint32_t scratchOffset, uint32_t scratchBytes, bool newline, uint64_t fd) {
    emitPopReg(0);
    emitPrintUnsignedInternal(scratchOffset, scratchBytes, false, 0, newline, fd);
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

    emit(encodeLdrRegBase(1, 27, localOffset(argcLocalIndex)));
    emitCompareReg(0, 1);
    size_t oobBranch = emitCondBranchPlaceholder(CondCode::Ge);

    emit(encodeLdrRegBase(2, 27, localOffset(argvLocalIndex)));
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

  void patchAdr(size_t index, uint8_t rd, int32_t imm21) {
    patchWord(index, encodeAdr(rd, imm21));
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

  void emitCompareRegZero(uint8_t reg) {
    emit(encodeSubsReg(31, reg, 31));
  }

  void emitCompareReg(uint8_t left, uint8_t right) {
    emit(encodeSubsReg(31, left, right));
  }

  size_t emitCondBranchPlaceholder(CondCode cond) {
    size_t index = currentWordIndex();
    emit(encodeBCond(0, static_cast<uint8_t>(cond)));
    return index;
  }

  void patchCondBranch(size_t index, int32_t offsetWords, CondCode cond) {
    patchWord(index, encodeBCond(offsetWords, static_cast<uint8_t>(cond)));
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

  size_t emitAdrPlaceholder(uint8_t rd) {
    size_t index = currentWordIndex();
    emit(encodeAdr(rd, 0));
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

  void emitWriteNewline(uint64_t fd, uint32_t scratchOffset) {
    emitLoadFrameOffset(1, scratchOffset);
    emitMovImm64(4, '\n');
    emitStrbRegBase(4, 1, 0);
    emitMovImm64(2, 1);
    emitWriteSyscall(fd, 1, 2);
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

  static uint32_t encodeAdr(uint8_t rd, int32_t imm21) {
    uint32_t imm = static_cast<uint32_t>(imm21) & 0x1FFFFFu;
    uint32_t immlo = imm & 0x3u;
    uint32_t immhi = (imm >> 2) & 0x7FFFFu;
    return 0x10000000 | (immlo << 29) | (immhi << 5) | static_cast<uint32_t>(rd);
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

  static uint16_t localOffset(uint32_t index) {
    return static_cast<uint16_t>(index * 16 + 8);
  }

  std::vector<uint32_t> code_;
  uint64_t frameSize_ = 0;
};

bool computeMaxStackDepth(const IrFunction &fn, int64_t &maxDepth, std::string &error) {
  if (fn.instructions.empty()) {
    error = "native backend requires at least one instruction";
    return false;
  }
  auto opcodeName = [](IrOpcode op) -> const char * {
    switch (op) {
      case IrOpcode::PushI32:
        return "PushI32";
      case IrOpcode::PushI64:
        return "PushI64";
      case IrOpcode::PushArgc:
        return "PushArgc";
      case IrOpcode::LoadLocal:
        return "LoadLocal";
      case IrOpcode::StoreLocal:
        return "StoreLocal";
      case IrOpcode::AddressOfLocal:
        return "AddressOfLocal";
      case IrOpcode::LoadIndirect:
        return "LoadIndirect";
      case IrOpcode::StoreIndirect:
        return "StoreIndirect";
      case IrOpcode::Dup:
        return "Dup";
      case IrOpcode::Pop:
        return "Pop";
      case IrOpcode::AddI32:
        return "AddI32";
      case IrOpcode::SubI32:
        return "SubI32";
      case IrOpcode::MulI32:
        return "MulI32";
      case IrOpcode::DivI32:
        return "DivI32";
      case IrOpcode::NegI32:
        return "NegI32";
      case IrOpcode::AddI64:
        return "AddI64";
      case IrOpcode::SubI64:
        return "SubI64";
      case IrOpcode::MulI64:
        return "MulI64";
      case IrOpcode::DivI64:
        return "DivI64";
      case IrOpcode::DivU64:
        return "DivU64";
      case IrOpcode::NegI64:
        return "NegI64";
      case IrOpcode::CmpEqI32:
        return "CmpEqI32";
      case IrOpcode::CmpNeI32:
        return "CmpNeI32";
      case IrOpcode::CmpLtI32:
        return "CmpLtI32";
      case IrOpcode::CmpLeI32:
        return "CmpLeI32";
      case IrOpcode::CmpGtI32:
        return "CmpGtI32";
      case IrOpcode::CmpGeI32:
        return "CmpGeI32";
      case IrOpcode::CmpEqI64:
        return "CmpEqI64";
      case IrOpcode::CmpNeI64:
        return "CmpNeI64";
      case IrOpcode::CmpLtI64:
        return "CmpLtI64";
      case IrOpcode::CmpLeI64:
        return "CmpLeI64";
      case IrOpcode::CmpGtI64:
        return "CmpGtI64";
      case IrOpcode::CmpGeI64:
        return "CmpGeI64";
      case IrOpcode::CmpLtU64:
        return "CmpLtU64";
      case IrOpcode::CmpLeU64:
        return "CmpLeU64";
      case IrOpcode::CmpGtU64:
        return "CmpGtU64";
      case IrOpcode::CmpGeU64:
        return "CmpGeU64";
      case IrOpcode::JumpIfZero:
        return "JumpIfZero";
      case IrOpcode::Jump:
        return "Jump";
      case IrOpcode::ReturnVoid:
        return "ReturnVoid";
      case IrOpcode::ReturnI32:
        return "ReturnI32";
      case IrOpcode::ReturnI64:
        return "ReturnI64";
      case IrOpcode::PrintI32:
        return "PrintI32";
      case IrOpcode::PrintI64:
        return "PrintI64";
      case IrOpcode::PrintU64:
        return "PrintU64";
      case IrOpcode::PrintString:
        return "PrintString";
      case IrOpcode::PrintArgv:
        return "PrintArgv";
      case IrOpcode::PrintArgvUnsafe:
        return "PrintArgvUnsafe";
      case IrOpcode::LoadStringByte:
        return "LoadStringByte";
      default:
        return "Unknown";
    }
  };
  auto stackDelta = [](IrOpcode op) -> int32_t {
    switch (op) {
      case IrOpcode::PushI32:
      case IrOpcode::PushI64:
      case IrOpcode::PushArgc:
      case IrOpcode::LoadLocal:
      case IrOpcode::AddressOfLocal:
        return 1;
      case IrOpcode::StoreLocal:
      case IrOpcode::Pop:
        return -1;
      case IrOpcode::LoadIndirect:
        return 0;
      case IrOpcode::StoreIndirect:
        return -1;
      case IrOpcode::Dup:
        return 1;
      case IrOpcode::AddI32:
      case IrOpcode::SubI32:
      case IrOpcode::MulI32:
      case IrOpcode::DivI32:
      case IrOpcode::AddI64:
      case IrOpcode::SubI64:
      case IrOpcode::MulI64:
      case IrOpcode::DivI64:
      case IrOpcode::DivU64:
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
        return -1;
      case IrOpcode::NegI32:
      case IrOpcode::NegI64:
        return 0;
      case IrOpcode::JumpIfZero:
        return -1;
      case IrOpcode::Jump:
        return 0;
      case IrOpcode::ReturnVoid:
        return 0;
      case IrOpcode::ReturnI32:
      case IrOpcode::ReturnI64:
        return -1;
      case IrOpcode::PrintI32:
      case IrOpcode::PrintI64:
      case IrOpcode::PrintU64:
        return -1;
      case IrOpcode::PrintString:
        return 0;
      case IrOpcode::PrintArgv:
      case IrOpcode::PrintArgvUnsafe:
        return -1;
      case IrOpcode::LoadStringByte:
        return 0;
      default:
        return 0;
    }
  };

  const int64_t kUnset = std::numeric_limits<int64_t>::min();
  std::vector<int64_t> depth(fn.instructions.size(), kUnset);
  std::vector<size_t> worklist;
  depth[0] = 0;
  worklist.push_back(0);
  maxDepth = 0;

  while (!worklist.empty()) {
    size_t index = worklist.back();
    worklist.pop_back();
    int64_t currentDepth = depth[index];
    maxDepth = std::max(maxDepth, currentDepth);
    const auto &inst = fn.instructions[index];
    int64_t nextDepth = currentDepth + stackDelta(inst.op);
    if (nextDepth < 0) {
      error = "native backend detected invalid stack usage at instruction " + std::to_string(index) + " (" +
              opcodeName(inst.op) + ")";
      return false;
    }
    maxDepth = std::max(maxDepth, nextDepth);

    auto pushSuccessor = [&](size_t nextIndex) -> bool {
      if (nextIndex >= fn.instructions.size()) {
        return true;
      }
      if (depth[nextIndex] == kUnset) {
        depth[nextIndex] = nextDepth;
        worklist.push_back(nextIndex);
        return true;
      }
      if (depth[nextIndex] != nextDepth) {
        error = "native backend detected inconsistent stack depth at instruction " + std::to_string(nextIndex) + " (" +
                opcodeName(fn.instructions[nextIndex].op) + ")";
        return false;
      }
      return true;
    };

    if (inst.op == IrOpcode::ReturnVoid || inst.op == IrOpcode::ReturnI32 || inst.op == IrOpcode::ReturnI64) {
      continue;
    }
    if (inst.op == IrOpcode::Jump || inst.op == IrOpcode::JumpIfZero) {
      if (static_cast<size_t>(inst.imm) > fn.instructions.size()) {
        error = "native backend detected invalid jump target";
        return false;
      }
      if (!pushSuccessor(static_cast<size_t>(inst.imm))) {
        return false;
      }
      if (inst.op == IrOpcode::JumpIfZero) {
        if (!pushSuccessor(index + 1)) {
          return false;
        }
      }
      continue;
    }
    if (!pushSuccessor(index + 1)) {
      return false;
    }
  }
  return true;
}

bool writeBinaryFile(const std::string &path, const std::vector<uint8_t> &data, std::string &error) {
  std::filesystem::path outputPath(path);
  std::filesystem::path parent = outputPath.parent_path();
  if (parent.empty()) {
    parent = ".";
  }

  // Write to a temporary file and rename into place. This avoids keeping a
  // potentially problematic inode alive across runs (e.g. when tests leave
  // behind executing processes).
  const long long pid =
#if defined(_WIN32)
      static_cast<long long>(::_getpid());
#else
      static_cast<long long>(::getpid());
#endif
  std::filesystem::path tempPath =
      parent / (outputPath.filename().string() + ".tmp." + std::to_string(pid));

  {
    std::error_code cleanupEc;
    std::filesystem::remove(tempPath, cleanupEc);
  }

  std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
  if (!file) {
    error = "failed to open output file";
    return false;
  }
  file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!file.good()) {
    std::error_code cleanupEc;
    std::filesystem::remove(tempPath, cleanupEc);
    error = "failed to write output file";
    return false;
  }
  file.close();

  std::error_code ec;
  std::filesystem::permissions(tempPath,
                               std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::add,
                               ec);
  if (ec) {
    std::filesystem::remove(tempPath, ec);
    error = "failed to set executable permissions";
    return false;
  }

  std::filesystem::remove(outputPath, ec);
  ec.clear();
  std::filesystem::rename(tempPath, outputPath, ec);
  if (ec) {
    std::filesystem::remove(tempPath, ec);
    error = "failed to move output file into place";
    return false;
  }
  return true;
}

#if defined(__APPLE__)
class Sha256 {
 public:
  Sha256() {
    reset();
  }

  void reset() {
    state_[0] = 0x6a09e667u;
    state_[1] = 0xbb67ae85u;
    state_[2] = 0x3c6ef372u;
    state_[3] = 0xa54ff53au;
    state_[4] = 0x510e527fu;
    state_[5] = 0x9b05688cu;
    state_[6] = 0x1f83d9abu;
    state_[7] = 0x5be0cd19u;
    totalLen_ = 0;
    bufferLen_ = 0;
  }

  void update(const uint8_t *data, size_t len) {
    if (len == 0) {
      return;
    }
    totalLen_ += len;
    size_t offset = 0;
    if (bufferLen_ > 0) {
      size_t toCopy = std::min(len, sizeof(buffer_) - bufferLen_);
      std::memcpy(buffer_ + bufferLen_, data, toCopy);
      bufferLen_ += toCopy;
      offset += toCopy;
      if (bufferLen_ == sizeof(buffer_)) {
        processBlock(buffer_);
        bufferLen_ = 0;
      }
    }
    while (offset + sizeof(buffer_) <= len) {
      processBlock(data + offset);
      offset += sizeof(buffer_);
    }
    if (offset < len) {
      bufferLen_ = len - offset;
      std::memcpy(buffer_, data + offset, bufferLen_);
    }
  }

  std::array<uint8_t, 32> finalize() {
    uint64_t totalBits = totalLen_ * 8;
    buffer_[bufferLen_++] = 0x80;
    if (bufferLen_ > 56) {
      while (bufferLen_ < 64) {
        buffer_[bufferLen_++] = 0;
      }
      processBlock(buffer_);
      bufferLen_ = 0;
    }
    while (bufferLen_ < 56) {
      buffer_[bufferLen_++] = 0;
    }
    for (int i = 7; i >= 0; --i) {
      buffer_[bufferLen_++] = static_cast<uint8_t>((totalBits >> (i * 8)) & 0xFF);
    }
    processBlock(buffer_);

    std::array<uint8_t, 32> digest{};
    for (size_t i = 0; i < 8; ++i) {
      digest[i * 4 + 0] = static_cast<uint8_t>((state_[i] >> 24) & 0xFF);
      digest[i * 4 + 1] = static_cast<uint8_t>((state_[i] >> 16) & 0xFF);
      digest[i * 4 + 2] = static_cast<uint8_t>((state_[i] >> 8) & 0xFF);
      digest[i * 4 + 3] = static_cast<uint8_t>(state_[i] & 0xFF);
    }
    return digest;
  }

 private:
  static uint32_t rotr(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32 - bits));
  }

  void processBlock(const uint8_t *block) {
    static constexpr uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

    uint32_t w[64];
    for (size_t i = 0; i < 16; ++i) {
      size_t j = i * 4;
      w[i] = (static_cast<uint32_t>(block[j]) << 24) | (static_cast<uint32_t>(block[j + 1]) << 16) |
             (static_cast<uint32_t>(block[j + 2]) << 8) | static_cast<uint32_t>(block[j + 3]);
    }
    for (size_t i = 16; i < 64; ++i) {
      uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];
    uint32_t f = state_[5];
    uint32_t g = state_[6];
    uint32_t h = state_[7];

    for (size_t i = 0; i < 64; ++i) {
      uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t temp1 = h + s1 + ch + k[i] + w[i];
      uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t temp2 = s0 + maj;

      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  uint32_t state_[8]{};
  uint64_t totalLen_ = 0;
  uint8_t buffer_[64]{};
  size_t bufferLen_ = 0;
};

void appendU32BE(std::vector<uint8_t> &out, uint32_t value) {
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void appendU64BE(std::vector<uint8_t> &out, uint64_t value) {
  appendU32BE(out, static_cast<uint32_t>((value >> 32) & 0xFFFFFFFFu));
  appendU32BE(out, static_cast<uint32_t>(value & 0xFFFFFFFFu));
}

bool buildCodeSignature(const std::vector<uint8_t> &image,
                        uint32_t codeLimit,
                        std::vector<uint8_t> &out,
                        std::string &error) {
  constexpr uint32_t superBlobMagic = 0xFADE0CC0u;
  constexpr uint32_t codeDirMagic = 0xFADE0C02u;
  constexpr uint32_t codeDirVersion = 0x20400u;
  constexpr uint32_t codeDirFlags = 0x00000002u; // adhoc
  constexpr uint8_t hashSize = 32;
  constexpr uint8_t hashType = 2; // SHA-256
  constexpr uint8_t platform = 2;
  constexpr uint8_t pageSizeLog2 = 14; // 16KB pages on arm64 macOS

  if (codeLimit == 0 || codeLimit > image.size()) {
    error = "invalid code signature limit";
    return false;
  }

  const std::string identifier = "primec.native";
  const uint32_t headerSize = 88;
  const uint32_t identOffset = headerSize;
  const uint32_t identSize = static_cast<uint32_t>(identifier.size() + 1);
  const uint32_t hashOffset = identOffset + identSize;
  const uint32_t nCodeSlots = static_cast<uint32_t>((codeLimit + (1u << pageSizeLog2) - 1) >> pageSizeLog2);
  const uint32_t length = hashOffset + nCodeSlots * hashSize;

  std::vector<uint8_t> codeDir;
  codeDir.reserve(length);
  appendU32BE(codeDir, codeDirMagic);
  appendU32BE(codeDir, length);
  appendU32BE(codeDir, codeDirVersion);
  appendU32BE(codeDir, codeDirFlags);
  appendU32BE(codeDir, hashOffset);
  appendU32BE(codeDir, identOffset);
  appendU32BE(codeDir, 0); // nSpecialSlots
  appendU32BE(codeDir, nCodeSlots);
  appendU32BE(codeDir, codeLimit);
  uint32_t hashInfo = (static_cast<uint32_t>(hashSize) << 24) |
                      (static_cast<uint32_t>(hashType) << 16) |
                      (static_cast<uint32_t>(platform) << 8) |
                      static_cast<uint32_t>(pageSizeLog2);
  appendU32BE(codeDir, hashInfo);
  appendU32BE(codeDir, 0); // spare2
  appendU32BE(codeDir, 0); // scatterOffset
  appendU32BE(codeDir, 0); // teamOffset
  appendU32BE(codeDir, 0); // spare3
  appendU64BE(codeDir, 0); // codeLimit64
  appendU64BE(codeDir, 0); // execSegBase
  appendU64BE(codeDir, 0); // execSegLimit
  appendU64BE(codeDir, 0x400000000000ull); // execSegFlags

  codeDir.insert(codeDir.end(), identifier.begin(), identifier.end());
  codeDir.push_back(0);

  const size_t pageSize = static_cast<size_t>(1u << pageSizeLog2);
  for (uint32_t slot = 0; slot < nCodeSlots; ++slot) {
    size_t start = static_cast<size_t>(slot) * pageSize;
    size_t end = std::min(static_cast<size_t>(codeLimit), start + pageSize);
    Sha256 sha;
    if (start < end) {
      sha.update(image.data() + start, end - start);
    }
    if (end < start + pageSize) {
      std::vector<uint8_t> pad(start + pageSize - end, 0);
      sha.update(pad.data(), pad.size());
    }
    auto digest = sha.finalize();
    codeDir.insert(codeDir.end(), digest.begin(), digest.end());
  }

  const uint32_t count = 1;
  const uint32_t superSize = 12 + count * 8 + static_cast<uint32_t>(codeDir.size());
  out.clear();
  out.reserve(superSize);
  appendU32BE(out, superBlobMagic);
  appendU32BE(out, superSize);
  appendU32BE(out, count);
  appendU32BE(out, 0); // type: CodeDirectory
  appendU32BE(out, 12 + count * 8);
  out.insert(out.end(), codeDir.begin(), codeDir.end());
  return true;
}

bool buildMachO(const std::vector<uint8_t> &code, std::vector<uint8_t> &image, std::string &error) {
  if (code.empty()) {
    error = "native backend requires non-empty code";
    return false;
  }

  constexpr uint8_t pageSizeLog2 = 14;
  constexpr uint32_t hashSize = 32;
  const std::string identifier = "primec.native";
  const std::string dyldPath = "/usr/lib/dyld";
  const std::string libSystemPath = "/usr/lib/libSystem.B.dylib";

  const uint32_t headerSize = sizeof(mach_header_64);
  const uint32_t pageZeroCmdSize = sizeof(segment_command_64);
  const uint32_t textCmdSize = sizeof(segment_command_64) + sizeof(section_64);
  const uint32_t linkeditCmdSize = sizeof(segment_command_64);
  const uint32_t dylinkerCmdSize =
      static_cast<uint32_t>(alignTo(sizeof(dylinker_command) + dyldPath.size() + 1, 8));
  const uint32_t dylibCmdSize =
      static_cast<uint32_t>(alignTo(sizeof(dylib_command) + libSystemPath.size() + 1, 8));
  const uint32_t dyldInfoCmdSize = sizeof(dyld_info_command);
  const uint32_t symtabCmdSize = sizeof(symtab_command);
  const uint32_t dysymtabCmdSize = sizeof(dysymtab_command);
  const uint32_t funcStartsCmdSize = sizeof(linkedit_data_command);
  const uint32_t dataInCodeCmdSize = sizeof(linkedit_data_command);
  const uint32_t entryCmdSize = sizeof(entry_point_command);
  const uint32_t codeSigCmdSize = sizeof(linkedit_data_command);
  const uint32_t sizeofcmds =
      pageZeroCmdSize + textCmdSize + linkeditCmdSize + dylinkerCmdSize + dylibCmdSize + dyldInfoCmdSize +
      symtabCmdSize + dysymtabCmdSize + funcStartsCmdSize + dataInCodeCmdSize + entryCmdSize + codeSigCmdSize;

  const uint32_t codeOffset = static_cast<uint32_t>(alignTo(headerSize + sizeofcmds, 16));
  const uint64_t codeSize = code.size();
  const uint64_t textFileSize = codeOffset + codeSize;
  const uint64_t textVmSize = alignTo(textFileSize, PageSize);

  const uint64_t linkeditFileOff = alignTo(textFileSize, PageSize);
  const uint64_t sigOffset = alignTo(linkeditFileOff, 16);
  const uint32_t codeLimit = static_cast<uint32_t>(sigOffset);

  const uint32_t codeDirHeaderSize = 88;
  const uint32_t identSize = static_cast<uint32_t>(identifier.size() + 1);
  const uint32_t hashOffset = codeDirHeaderSize + identSize;
  const uint32_t nCodeSlots = static_cast<uint32_t>((codeLimit + (1u << pageSizeLog2) - 1) >> pageSizeLog2);
  const uint32_t codeDirLength = hashOffset + nCodeSlots * hashSize;
  const uint32_t sigSize = 12 + 8 + codeDirLength;

  const uint64_t linkeditVmAddr = alignTo(TextVmAddr + textVmSize, PageSize);
  const uint64_t linkeditFileSize = (sigOffset - linkeditFileOff) + sigSize;
  const uint64_t linkeditVmSize = alignTo(linkeditFileSize, PageSize);
  const uint64_t fileSize = sigOffset + sigSize;

  image.assign(static_cast<size_t>(fileSize), 0);

  mach_header_64 header{};
  header.magic = MH_MAGIC_64;
  header.cputype = CPU_TYPE_ARM64;
  header.cpusubtype = CPU_SUBTYPE_ARM64_ALL;
  header.filetype = MH_EXECUTE;
  header.ncmds = 12;
  header.sizeofcmds = sizeofcmds;
  header.flags = MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL | MH_PIE;

  segment_command_64 pageZero{};
  pageZero.cmd = LC_SEGMENT_64;
  pageZero.cmdsize = pageZeroCmdSize;
  std::memcpy(pageZero.segname, "__PAGEZERO", sizeof("__PAGEZERO") - 1);
  pageZero.vmaddr = 0;
  pageZero.vmsize = PageZeroSize;
  pageZero.fileoff = 0;
  pageZero.filesize = 0;
  pageZero.maxprot = 0;
  pageZero.initprot = 0;
  pageZero.nsects = 0;
  pageZero.flags = 0;

  segment_command_64 text{};
  text.cmd = LC_SEGMENT_64;
  text.cmdsize = textCmdSize;
  std::memcpy(text.segname, "__TEXT", sizeof("__TEXT") - 1);
  text.vmaddr = TextVmAddr;
  text.vmsize = textVmSize;
  text.fileoff = 0;
  text.filesize = textFileSize;
  text.maxprot = VM_PROT_READ | VM_PROT_EXECUTE;
  text.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
  text.nsects = 1;
  text.flags = 0;

  section_64 textSection{};
  std::memcpy(textSection.sectname, "__text", sizeof("__text") - 1);
  std::memcpy(textSection.segname, "__TEXT", sizeof("__TEXT") - 1);
  textSection.addr = TextVmAddr + codeOffset;
  textSection.size = codeSize;
  textSection.offset = codeOffset;
  textSection.align = 2;
  textSection.reloff = 0;
  textSection.nreloc = 0;
  textSection.flags = S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS;
  textSection.reserved1 = 0;
  textSection.reserved2 = 0;
  textSection.reserved3 = 0;

  segment_command_64 linkedit{};
  linkedit.cmd = LC_SEGMENT_64;
  linkedit.cmdsize = linkeditCmdSize;
  std::memcpy(linkedit.segname, "__LINKEDIT", sizeof("__LINKEDIT") - 1);
  linkedit.vmaddr = linkeditVmAddr;
  linkedit.vmsize = linkeditVmSize;
  linkedit.fileoff = linkeditFileOff;
  linkedit.filesize = linkeditFileSize;
  linkedit.maxprot = VM_PROT_READ;
  linkedit.initprot = VM_PROT_READ;
  linkedit.nsects = 0;
  linkedit.flags = 0;

  linkedit_data_command codeSig{};
  codeSig.cmd = LC_CODE_SIGNATURE;
  codeSig.cmdsize = codeSigCmdSize;
  codeSig.dataoff = static_cast<uint32_t>(sigOffset);
  codeSig.datasize = sigSize;

  dyld_info_command dyldInfo{};
  dyldInfo.cmd = LC_DYLD_INFO_ONLY;
  dyldInfo.cmdsize = dyldInfoCmdSize;

  symtab_command symtab{};
  symtab.cmd = LC_SYMTAB;
  symtab.cmdsize = symtabCmdSize;

  dysymtab_command dysymtab{};
  dysymtab.cmd = LC_DYSYMTAB;
  dysymtab.cmdsize = dysymtabCmdSize;

  linkedit_data_command funcStarts{};
  funcStarts.cmd = LC_FUNCTION_STARTS;
  funcStarts.cmdsize = funcStartsCmdSize;

  linkedit_data_command dataInCode{};
  dataInCode.cmd = LC_DATA_IN_CODE;
  dataInCode.cmdsize = dataInCodeCmdSize;

  size_t offset = 0;
  std::memcpy(image.data() + offset, &header, sizeof(header));
  offset += sizeof(header);
  std::memcpy(image.data() + offset, &pageZero, sizeof(pageZero));
  offset += sizeof(pageZero);
  std::memcpy(image.data() + offset, &text, sizeof(text));
  offset += sizeof(text);
  std::memcpy(image.data() + offset, &textSection, sizeof(textSection));
  offset += sizeof(textSection);
  std::memcpy(image.data() + offset, &linkedit, sizeof(linkedit));
  offset += sizeof(linkedit);
  dylinker_command dyldCmd{};
  dyldCmd.cmd = LC_LOAD_DYLINKER;
  dyldCmd.cmdsize = dylinkerCmdSize;
  dyldCmd.name.offset = sizeof(dylinker_command);
  std::vector<uint8_t> dyldBlob(dylinkerCmdSize, 0);
  std::memcpy(dyldBlob.data(), &dyldCmd, sizeof(dyldCmd));
  std::memcpy(dyldBlob.data() + dyldCmd.name.offset, dyldPath.c_str(), dyldPath.size() + 1);
  std::memcpy(image.data() + offset, dyldBlob.data(), dyldBlob.size());
  offset += dyldBlob.size();
  dylib_command libSystemCmd{};
  libSystemCmd.cmd = LC_LOAD_DYLIB;
  libSystemCmd.cmdsize = dylibCmdSize;
  libSystemCmd.dylib.name.offset = sizeof(dylib_command);
  libSystemCmd.dylib.timestamp = 2;
  libSystemCmd.dylib.current_version = 0x00010000;
  libSystemCmd.dylib.compatibility_version = 0x00010000;
  std::vector<uint8_t> dylibBlob(dylibCmdSize, 0);
  std::memcpy(dylibBlob.data(), &libSystemCmd, sizeof(libSystemCmd));
  std::memcpy(dylibBlob.data() + libSystemCmd.dylib.name.offset,
              libSystemPath.c_str(),
              libSystemPath.size() + 1);
  std::memcpy(image.data() + offset, dylibBlob.data(), dylibBlob.size());
  offset += dylibBlob.size();
  std::memcpy(image.data() + offset, &dyldInfo, sizeof(dyldInfo));
  offset += sizeof(dyldInfo);
  std::memcpy(image.data() + offset, &symtab, sizeof(symtab));
  offset += sizeof(symtab);
  std::memcpy(image.data() + offset, &dysymtab, sizeof(dysymtab));
  offset += sizeof(dysymtab);
  std::memcpy(image.data() + offset, &funcStarts, sizeof(funcStarts));
  offset += sizeof(funcStarts);
  std::memcpy(image.data() + offset, &dataInCode, sizeof(dataInCode));
  offset += sizeof(dataInCode);
  entry_point_command entryCmd{};
  entryCmd.cmd = LC_MAIN;
  entryCmd.cmdsize = entryCmdSize;
  entryCmd.entryoff = codeOffset;
  entryCmd.stacksize = 0;
  std::memcpy(image.data() + offset, &entryCmd, sizeof(entryCmd));
  offset += sizeof(entryCmd);
  std::memcpy(image.data() + offset, &codeSig, sizeof(codeSig));

  std::memcpy(image.data() + codeOffset, code.data(), code.size());
  std::vector<uint8_t> signature;
  if (!buildCodeSignature(image, codeLimit, signature, error)) {
    return false;
  }
  if (signature.size() != sigSize) {
    error = "code signature size mismatch";
    return false;
  }
  std::memcpy(image.data() + sigOffset, signature.data(), signature.size());
  return true;
}
#endif

} // namespace

bool NativeEmitter::emitExecutable(const IrModule &module, const std::string &outputPath, std::string &error) const {
#if !defined(__APPLE__)
  (void)module;
  (void)outputPath;
  error = "native backend is only supported on macOS";
  return false;
#else
#if !defined(__aarch64__) && !defined(__arm64__)
  (void)module;
  (void)outputPath;
  error = "native backend requires arm64";
  return false;
#else
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }

  const IrFunction &fn = module.functions[static_cast<size_t>(module.entryIndex)];
  Arm64Emitter emitter;
  size_t localCount = 0;
  int64_t maxStack = 0;
  bool needsPrintScratch = false;
  bool needsArgc = false;
  bool needsArgv = false;
  for (const auto &inst : fn.instructions) {
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal ||
        inst.op == IrOpcode::AddressOfLocal) {
      localCount = std::max(localCount, static_cast<size_t>(inst.imm) + 1);
    }
    if (inst.op == IrOpcode::PrintI32 || inst.op == IrOpcode::PrintI64 || inst.op == IrOpcode::PrintU64 ||
        inst.op == IrOpcode::PrintString || inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe) {
      needsPrintScratch = true;
    }
    if (inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe) {
      needsArgc = true;
      needsArgv = true;
    }
    if (inst.op == IrOpcode::PushArgc) {
      needsArgc = true;
    }
  }
  if (!computeMaxStackDepth(fn, maxStack, error)) {
    return false;
  }
  uint32_t argcLocalIndex = 0;
  uint32_t argvLocalIndex = 0;
  if (needsArgc) {
    argcLocalIndex = static_cast<uint32_t>(localCount);
    localCount += 1;
  }
  if (needsArgv) {
    argvLocalIndex = static_cast<uint32_t>(localCount);
    localCount += 1;
  }
  uint32_t scratchSlots = needsPrintScratch ? PrintScratchSlots : 0;
  uint32_t scratchBytes = scratchSlots * 16;
  uint32_t scratchOffset = static_cast<uint32_t>(localCount) * 16;
  uint64_t localsSize = static_cast<uint64_t>(localCount + scratchSlots) * 16;
  uint64_t stackSize = static_cast<uint64_t>(maxStack) * 16;
  uint64_t frameSize = alignTo(localsSize + stackSize, 16);
  if (localCount > 2047) {
    error = "native backend supports up to 2048 locals";
    return false;
  }
  if (!emitter.beginFunction(frameSize, error)) {
    return false;
  }
  if (needsArgc) {
    emitter.emitStoreLocalFromReg(argcLocalIndex, 0);
  }
  if (needsArgv) {
    emitter.emitStoreLocalFromReg(argvLocalIndex, 1);
  }

  struct BranchFixup {
    size_t codeIndex = 0;
    size_t targetInst = 0;
    bool isConditional = false;
  };
  std::vector<BranchFixup> fixups;
  struct StringFixup {
    size_t codeIndex = 0;
    uint32_t stringIndex = 0;
  };
  std::vector<StringFixup> stringFixups;
  std::vector<uint64_t> stringOffsets;
  std::vector<uint8_t> stringData;
  if (!module.stringTable.empty()) {
    stringOffsets.reserve(module.stringTable.size());
    for (const auto &text : module.stringTable) {
      stringOffsets.push_back(static_cast<uint64_t>(stringData.size()));
      stringData.insert(stringData.end(), text.begin(), text.end());
    }
  }
  std::vector<size_t> instOffsets(fn.instructions.size() + 1, 0);

  for (size_t index = 0; index < fn.instructions.size(); ++index) {
    const auto &inst = fn.instructions[index];
    instOffsets[index] = emitter.currentWordIndex();
    switch (inst.op) {
      case IrOpcode::PushI32:
        emitter.emitPushI32(static_cast<int32_t>(inst.imm));
        break;
      case IrOpcode::PushI64:
        emitter.emitPushI64(inst.imm);
        break;
      case IrOpcode::PushArgc:
        emitter.emitLoadLocal(argcLocalIndex);
        break;
      case IrOpcode::LoadLocal:
        emitter.emitLoadLocal(static_cast<uint32_t>(inst.imm));
        break;
      case IrOpcode::StoreLocal:
        emitter.emitStoreLocal(static_cast<uint32_t>(inst.imm));
        break;
      case IrOpcode::AddressOfLocal:
        emitter.emitAddressOfLocal(static_cast<uint32_t>(inst.imm));
        break;
      case IrOpcode::LoadIndirect:
        emitter.emitLoadIndirect();
        break;
      case IrOpcode::StoreIndirect:
        emitter.emitStoreIndirect();
        break;
      case IrOpcode::Dup:
        emitter.emitDup();
        break;
      case IrOpcode::Pop:
        emitter.emitPop();
        break;
      case IrOpcode::AddI32:
        emitter.emitAdd();
        break;
      case IrOpcode::SubI32:
        emitter.emitSub();
        break;
      case IrOpcode::MulI32:
        emitter.emitMul();
        break;
      case IrOpcode::DivI32:
        emitter.emitDiv();
        break;
      case IrOpcode::NegI32:
        emitter.emitNeg();
        break;
      case IrOpcode::AddI64:
        emitter.emitAdd();
        break;
      case IrOpcode::SubI64:
        emitter.emitSub();
        break;
      case IrOpcode::MulI64:
        emitter.emitMul();
        break;
      case IrOpcode::DivI64:
        emitter.emitDiv();
        break;
      case IrOpcode::DivU64:
        emitter.emitDivU();
        break;
      case IrOpcode::NegI64:
        emitter.emitNeg();
        break;
      case IrOpcode::CmpEqI32:
        emitter.emitCmpEq();
        break;
      case IrOpcode::CmpNeI32:
        emitter.emitCmpNe();
        break;
      case IrOpcode::CmpLtI32:
        emitter.emitCmpLt();
        break;
      case IrOpcode::CmpLeI32:
        emitter.emitCmpLe();
        break;
      case IrOpcode::CmpGtI32:
        emitter.emitCmpGt();
        break;
      case IrOpcode::CmpGeI32:
        emitter.emitCmpGe();
        break;
      case IrOpcode::CmpEqI64:
        emitter.emitCmpEq();
        break;
      case IrOpcode::CmpNeI64:
        emitter.emitCmpNe();
        break;
      case IrOpcode::CmpLtI64:
        emitter.emitCmpLt();
        break;
      case IrOpcode::CmpLeI64:
        emitter.emitCmpLe();
        break;
      case IrOpcode::CmpGtI64:
        emitter.emitCmpGt();
        break;
      case IrOpcode::CmpGeI64:
        emitter.emitCmpGe();
        break;
      case IrOpcode::CmpLtU64:
        emitter.emitCmpLtU();
        break;
      case IrOpcode::CmpLeU64:
        emitter.emitCmpLeU();
        break;
      case IrOpcode::CmpGtU64:
        emitter.emitCmpGtU();
        break;
      case IrOpcode::CmpGeU64:
        emitter.emitCmpGeU();
        break;
      case IrOpcode::JumpIfZero: {
        BranchFixup fixup;
        fixup.codeIndex = emitter.emitJumpIfZeroPlaceholder();
        fixup.targetInst = static_cast<size_t>(inst.imm);
        fixup.isConditional = true;
        fixups.push_back(fixup);
        break;
      }
      case IrOpcode::Jump: {
        BranchFixup fixup;
        fixup.codeIndex = emitter.emitJumpPlaceholder();
        fixup.targetInst = static_cast<size_t>(inst.imm);
        fixup.isConditional = false;
        fixups.push_back(fixup);
        break;
      }
      case IrOpcode::ReturnVoid:
        emitter.emitReturnVoid();
        break;
      case IrOpcode::ReturnI32:
        emitter.emitReturn();
        break;
      case IrOpcode::ReturnI64:
        emitter.emitReturn();
        break;
      case IrOpcode::PrintI32: {
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        emitter.emitPrintSigned(scratchOffset, scratchBytes, newline, fd);
        break;
      }
      case IrOpcode::PrintI64: {
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        emitter.emitPrintSigned(scratchOffset, scratchBytes, newline, fd);
        break;
      }
      case IrOpcode::PrintU64: {
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        emitter.emitPrintUnsigned(scratchOffset, scratchBytes, newline, fd);
        break;
      }
      case IrOpcode::PrintString: {
        uint64_t stringIndex = decodePrintStringIndex(inst.imm);
        if (stringIndex >= module.stringTable.size()) {
          error = "native backend encountered invalid string index";
          return false;
        }
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        size_t fixupIndex = emitter.emitPrintStringPlaceholder(
            module.stringTable[static_cast<size_t>(stringIndex)].size(), scratchOffset, newline, fd);
        stringFixups.push_back({fixupIndex, static_cast<uint32_t>(stringIndex)});
        break;
      }
      case IrOpcode::PrintArgv: {
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        emitter.emitPrintArgv(argcLocalIndex, argvLocalIndex, scratchOffset, newline, fd);
        break;
      }
      case IrOpcode::PrintArgvUnsafe: {
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        emitter.emitPrintArgv(argcLocalIndex, argvLocalIndex, scratchOffset, newline, fd);
        break;
      }
      case IrOpcode::LoadStringByte: {
        if (inst.imm >= module.stringTable.size()) {
          error = "native backend encountered invalid string index";
          return false;
        }
        size_t fixupIndex = emitter.emitLoadStringBytePlaceholder();
        stringFixups.push_back({fixupIndex, static_cast<uint32_t>(inst.imm)});
        break;
      }
      default:
        error = "unsupported IR opcode for native backend";
        return false;
    }
  }

  instOffsets[fn.instructions.size()] = emitter.currentWordIndex();

  constexpr int64_t kImm19Min = -(1LL << 18);
  constexpr int64_t kImm19Max = (1LL << 18) - 1;
  constexpr int64_t kImm26Min = -(1LL << 25);
  constexpr int64_t kImm26Max = (1LL << 25) - 1;
  for (const auto &fixup : fixups) {
    if (fixup.targetInst > fn.instructions.size()) {
      error = "native backend detected invalid jump target";
      return false;
    }
    int64_t targetOffset = static_cast<int64_t>(instOffsets[fixup.targetInst]);
    int64_t branchOffset = static_cast<int64_t>(fixup.codeIndex);
    int64_t delta = targetOffset - branchOffset;
    if (fixup.isConditional) {
      if (delta < kImm19Min || delta > kImm19Max) {
        error = "native backend jump offset out of range";
        return false;
      }
      emitter.patchJumpIfZero(fixup.codeIndex, static_cast<int32_t>(delta));
    } else {
      if (delta < kImm26Min || delta > kImm26Max) {
        error = "native backend jump offset out of range";
        return false;
      }
      emitter.patchJump(fixup.codeIndex, static_cast<int32_t>(delta));
    }
  }

  if (!stringFixups.empty()) {
    uint64_t codeSizeBytes = static_cast<uint64_t>(emitter.currentWordIndex()) * 4;
    uint64_t stringBaseOffset = codeSizeBytes;
    constexpr int64_t kAdrMin = -(1LL << 20);
    constexpr int64_t kAdrMax = (1LL << 20) - 1;
    for (const auto &fixup : stringFixups) {
      if (fixup.stringIndex >= module.stringTable.size()) {
        error = "native backend encountered invalid string fixup";
        return false;
      }
      int64_t targetOffset = static_cast<int64_t>(stringBaseOffset + stringOffsets[fixup.stringIndex]);
      int64_t instrOffset = static_cast<int64_t>(fixup.codeIndex) * 4;
      int64_t delta = targetOffset - instrOffset;
      if (delta < kAdrMin || delta > kAdrMax) {
        error = "native backend string literal out of range";
        return false;
      }
      emitter.patchAdr(fixup.codeIndex, 1, static_cast<int32_t>(delta));
    }
  }

  std::vector<uint8_t> code = emitter.finalize();
  if (!stringData.empty()) {
    code.insert(code.end(), stringData.begin(), stringData.end());
  }
  std::vector<uint8_t> image;
  if (!buildMachO(code, image, error)) {
    return false;
  }
  return writeBinaryFile(outputPath, image, error);
#endif
#endif
}

} // namespace primec
