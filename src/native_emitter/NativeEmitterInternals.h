#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "primec/ir/Ir.h"
#include "NativeEmitterRegAlloc.h"

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
constexpr uint64_t SysExit = SYS_exit;
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
constexpr uint64_t SysExit = 0;
constexpr uint64_t MmapProtReadWrite = 0;
constexpr uint64_t MmapFlagsPrivateAnon = 0;
#endif
constexpr uint64_t HeapHeaderMagic = 0x5053484541503031ull;
// The header magic of a byte region (HeapAllocBytes): the same block layout as a slot allocation, but
// HeapRealloc counts its size in bytes.
constexpr uint64_t HeapHeaderMagicBytes = 0x5053484541503032ull;
// arm64 (NativeEmitterInternalsArm64Heap.h): the magics of pooled blocks of the two kinds (a mapped block's magic
// plus 2) and of a pooled block on a free list, the largest pooled block (slots, header included), the pool's
// chunk size and its size classes, and the allocator state in the data segment (chunk cursor and end, then one
// free-list head per class).
constexpr uint64_t HeapHeaderMagicPooled = HeapHeaderMagic + 2;
constexpr uint64_t HeapHeaderMagicPooledBytes = HeapHeaderMagicBytes + 2;
constexpr uint64_t HeapHeaderMagicFree = 0x5053484541503046ull;
constexpr uint64_t HeapPoolMaxSlots = 512;
constexpr uint64_t HeapPoolChunkBytes = 256 * 1024;
constexpr uint64_t HeapPoolClassCount = 31;
constexpr uint64_t HeapStateBytes = 16 + 8 * HeapPoolClassCount;

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

// The word buffer of Arm64Emitter. A value-stack pop may erase the push before it
// (Arm64Emitter::emitReloadReg), which shifts every word after the push, so the push may only be
// erased while nobody has read a word index since it was written. Every read of size() (labels,
// branch sites, checkpoints, instruction offsets, whichever accessor a template uses) and every
// resize() ends that window; only the fusion itself goes through the non-observing accessors.
class Arm64CodeWords {
public:
  size_t size() const {
    fusionEnd_ = SIZE_MAX;
    return words_.size();
  }
  void resize(size_t count) {
    fusionEnd_ = SIZE_MAX;
    words_.resize(count);
  }
  void push_back(uint32_t word) {
    words_.push_back(word);
  }
  // Writes a word at an index read earlier (a branch patch); no index moves.
  void patch(size_t index, uint32_t word) {
    if (index < words_.size()) {
      words_[index] = word;
    }
  }
  std::vector<uint32_t>::const_iterator begin() const {
    return words_.begin();
  }
  std::vector<uint32_t>::const_iterator end() const {
    return words_.end();
  }

  // The fusion window: opened at the current end after a push or a fused pop, open while no
  // word was added and no index read since.
  void openFusionWindow() {
    fusionEnd_ = words_.size();
  }
  void closeFusionWindow() {
    fusionEnd_ = SIZE_MAX;
  }
  bool fusionWindowOpen() const {
    return fusionEnd_ == words_.size();
  }
  // The index the next word gets, without closing the window: only for the fusion's own record
  // of the push it may erase.
  size_t nextIndexForFusion() const {
    return words_.size();
  }
  void eraseFusedPush(size_t index) {
    words_.erase(words_.begin() + static_cast<std::ptrdiff_t>(index));
  }

private:
  std::vector<uint32_t> words_;
  mutable size_t fusionEnd_ = SIZE_MAX;
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
    emitPushImm64(static_cast<uint64_t>(static_cast<int64_t>(value)));
  }

  void emitPushI64(uint64_t value) {
    emitPushImm64(value);
  }

  void emitPushF32(uint32_t bits) {
    emitPushImm64(bits);
  }

  void emitPushF64(uint64_t bits) {
    emitPushImm64(bits);
  }

  // A pushed integer constant feeding AddI*/SubI* or an integer comparison, as the instruction's
  // immediate operand: `add`/`sub` of a 12-bit immediate (or `#imm, lsl #12`, or the
  // negated constant through the other instruction), `cmp`/`cmn` for the comparisons. A constant 0
  // added or subtracted emits nothing. False, with nothing emitted, for any other opcode or a
  // constant without such an encoding.
  bool tryEmitImmediateOperand(IrOpcode op, uint64_t constant);

  // For encoder tests (primec/testing/NativeEmitterEncodings.h).
  void emitMovImm64Public(uint8_t rd, uint64_t value) {
    emitMovImm64(rd, value);
  }
  void emitAdjustSpPublic(uint64_t amount, bool add) {
    emitAdjustSp(amount, add);
  }
  void emitPushRegPublic(uint8_t reg) {
    emitPushReg(reg);
  }
  void emitPopRegPublic(uint8_t reg) {
    emitPopReg(reg);
  }
  // Reads the buffer size directly, as a template might instead of currentWordIndex(): pins that
  // such a read also keeps a pending push in place.
  size_t codeWordCountPublic() const {
    return code_.size();
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

  // A frame slot past the reach of `str` is addressed through `tmp` (x1, or x2 when storing
  // x1, unless given): the prologue of a function taking register arguments passes x17.
  void emitStoreLocalFromReg(uint32_t index, uint8_t reg, uint8_t tmp = 0xFF) {
    uint64_t offset = localOffset(index);
    if (offset <= MaxLdrStrOffsetBytes) {
      emit(encodeStrRegBase(reg, 27, static_cast<uint16_t>(offset)));
    } else {
      if (tmp == 0xFF) {
        tmp = (reg == 1) ? 2 : 1;
      }
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

  void emitHeapAlloc();
  void emitHeapFree();
  void emitHeapRealloc();
  // Byte memory: a zeroed region of the popped byte count (0 for a count of 0), a byte load that
  // zero-extends, a byte store that leaves the stored byte, and the u8 wrap.
  void emitHeapAllocBytes();
  // The heap routines every heap instruction calls (NativeEmitterInternalsArm64Heap.h), emitted once after the
  // functions when any heap instruction was; the ADRP/ADD pairs that address the allocator state need the data
  // segment's address (heapStateFixups).
  void emitHeapRuntime();
  bool usesHeapRuntime() const {
    return !heapAllocCalls_.empty() || !heapFreeCalls_.empty() || !heapReallocCalls_.empty();
  }
  const std::vector<size_t> &heapStateFixups() const {
    return heapStateFixups_;
  }
  // An imported C function (CallHost): see NativeEmitterInternalsArm64Host.h.
  struct HostCallSite {
    const std::vector<IrHostValueKind> *parameters = nullptr;
    IrHostValueKind returnKind = IrHostValueKind::Void;
    uint64_t stringTableDelta = 0;          // the string table's offset from the string data
    std::vector<size_t> *stringTableFixups = nullptr;
    size_t *slotFixup = nullptr;            // out: the word index of the slot load's ADRP
  };
  bool emitHostCall(const HostCallSite &site, std::string &error);
  size_t emitHostSlotLoad(uint8_t reg);
  void emitHostTextResult();
  void emitTextCopyOfReg13();
  void emitTextArgument(uint32_t argcLocalIndex, uint32_t argvLocalIndex);
  // The string heap functions (`__pstext_*`, primec/ir/IrNativeTextHost.h): a text block is a byte
  // region sized by the VM's size classes, with its capacity in the block header.
  void emitTextAlloc();
  void emitTextFree() { emitHeapFree(); }
  void emitTextCapacity();
  void emitTextUsable();
  void emitTextLength();
  void emitTextBlockSize();
  // The bulk byte operations (`__pstext_move`, `_fill`, `_compare`, `_find`) as byte loops over raw addresses.
  void emitTextMove();
  void emitTextFill();
  void emitTextCompare();
  void emitTextFind();
  void emitLoadU8() {
    emitPopReg(0);
    emit(encodeLdrbRegBase(1, 0, 0));
    emitPushReg(1);
  }
  void emitStoreU8() {
    emitPopReg(0);
    emitPopReg(1);
    emit(encodeStrbRegBase(0, 1, 0));
    emit(0x53001C00u); // UXTB w0, w0
    emitPushReg(0);
  }
  void emitZextU8() {
    emitPopReg(0);
    emit(0x53001C00u); // UXTB w0, w0
    emitPushReg(0);
  }

  void emitDup();
  void emitPop();
  void emitAdd();
  void emitSub();
  void emitMul();
  void emitDiv();
  void emitDivU();
  void emitNeg();
  void emitSextI32(); // low 32 bits of the top operand, sign-extended (SXTW)
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

  // Every label, branch site and instruction offset is read here; like any read of the buffer
  // size it ends the window in which a push may be fused with the pop that follows it
  // (Arm64CodeWords, emitReloadReg).
  size_t currentWordIndex() const {
    return code_.size();
  }

  void emitReturn();
  void emitReturnVoid();
  void emitReturnWithLink(uint32_t linkLocalIndex);
  void emitReturnVoidWithLink(uint32_t linkLocalIndex);
  void emitReturnWithFrameAndLink(uint32_t frameLocalIndex, uint32_t linkLocalIndex);
  void emitReturnVoidWithFrameAndLink(uint32_t frameLocalIndex, uint32_t linkLocalIndex);
  // `Trap`: ends the program with `code` from any frame (the exit syscall; output is already written).
  void emitTrap(int32_t code);

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
  void emitOpenResultFromSyscall();
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

  // Register-allocated function bodies (NativeEmitterRegAlloc.h,
  // NativeEmitterInternalsArm64RegAlloc.h).
  void setRegisterAllocationEnabled(bool enabled) {
    registerAllocationEnabled_ = enabled;
  }
  bool registerAllocationEnabled() const {
    return registerAllocationEnabled_;
  }
  void setRegisterAllocationFarBranches(bool enabled) {
    registerAllocationFarBranches_ = enabled;
  }
  bool registerAllocationFarBranches() const {
    return registerAllocationFarBranches_;
  }
  struct RegAllocHooks {
    // Emits instruction `index` with its ordinary template on the memory value stack.
    std::function<bool(size_t index)> emitTemplate;
    std::function<void(size_t fixupIndex, uint32_t stringIndex)> recordStringFixup;
    // Conditional branches take two words (b.cond over a b) and reach the whole image instead of
    // +-1 MB. The emitter sets `branchOutOfRange` and fails when a one-word branch cannot reach
    // its target; the caller rolls the function back (checkpoint) and emits it again this way.
    bool farBranches = false;
    bool *branchOutOfRange = nullptr;
    // The first Arm64RegAllocArgumentRegisters arguments arrive in x0-x7 instead of on the
    // value stack (every caller is register-allocated).
    bool argumentsInRegisters = false;
    std::function<bool(uint64_t target)> calleeTakesRegisterArguments;
    std::function<bool(size_t fixupIndex, uint64_t target)> recordCallFixup;
  };
  // Emits `fn`'s body from `plan` (after beginFunction and the prologue's local stores). Spill
  // slot k and the save slot of register r are the frame slots of pseudo-locals
  // spillBaseLocal + k and spillBaseLocal + plan.spillSlotCount + r.
  bool emitRegisterAllocatedFunction(const IrFunction &fn,
                                     const RegAllocFunctionPlan &plan,
                                     uint32_t spillBaseLocal,
                                     uint32_t frameLocalIndex,
                                     uint32_t linkLocalIndex,
                                     std::vector<size_t> &instOffsets,
                                     const RegAllocHooks &hooks,
                                     std::string &error);
  // The emitter state a register-allocated function can change, to emit it again.
  struct Checkpoint {
    size_t codeSize = 0;
    size_t heapAllocCalls = 0;
    size_t heapFreeCalls = 0;
    size_t heapReallocCalls = 0;
    size_t heapStateFixups = 0;
    Arm64InstrumentationCounters counters;
  };
  Checkpoint checkpoint() const {
    return {code_.size(),
            heapAllocCalls_.size(),
            heapFreeCalls_.size(),
            heapReallocCalls_.size(),
            heapStateFixups_.size(),
            counters_};
  }
  void rollBack(const Checkpoint &point) {
    code_.resize(point.codeSize);
    heapAllocCalls_.resize(point.heapAllocCalls);
    heapFreeCalls_.resize(point.heapFreeCalls);
    heapReallocCalls_.resize(point.heapReallocCalls);
    heapStateFixups_.resize(point.heapStateFixups);
    counters_ = point.counters;
  }

 private:
   enum class CondCode : uint8_t {
     Eq = 0x0,
     Ne = 0x1,
     Hs = 0x2,
     Lo = 0x3,
     Mi = 0x4, // negative: a float "less than" that is false when unordered
     Pl = 0x5,
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
     case CondCode::Mi:
       return CondCode::Pl;
     case CondCode::Pl:
       return CondCode::Mi;
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

  void emit(uint32_t word);
  void patchWord(size_t index, uint32_t word);

  void emitBinaryOp(uint32_t opWord);
  void emitFloatBinaryOp(bool isF64, uint32_t opD, uint32_t opS);
  void emitFloatUnaryOp(bool isF64, uint32_t opD, uint32_t opS);
  void emitCompareAndPushFloat(bool isF64, CondCode cond);
  void emitConvertIntToFloat(bool toF64, bool isUnsigned);
  void emitConvertFloatToInt(bool fromF64, bool isUnsigned);
  void emitConvertFloatToFloat(bool fromF64);
  void emitCompareRegZero(uint8_t reg);
  void emitCompareReg(uint8_t left, uint8_t right);
  // A template's own conditional branch: one `b.cond` word, patched to a target inside the same
  // template (always within its +-1 MB reach).
  size_t emitCondBranchPlaceholder(CondCode cond);
  void patchCondBranch(size_t index, int32_t offsetWords, CondCode cond);
  // A conditional branch to an IR target, which may lie past that reach: `b.cond` and a `b`,
  // patched to `b.cond target; b next` or, when far, `b.!cond next; b target`.
  size_t emitFarCondBranchPlaceholder(CondCode cond);
  void patchFarCondBranch(size_t index, int32_t offsetWords, CondCode cond);
  size_t emitCbzPlaceholder(uint8_t reg);
  void patchCbz(size_t index, uint8_t reg, int32_t offsetWords);
  void emitCompareAndPush(CondCode cond);

  void emitPushReg(uint8_t reg);
  void emitPopReg(uint8_t reg);
  void emitMovImm64(uint8_t rd, uint64_t value);
  void emitPushImm64(uint64_t value);
  void emitCompareFlagsAndPush(CondCode cond);
  void emitAdjustSp(uint64_t amount, bool add);
  static bool addSubImmediate(uint64_t value, uint16_t &imm12, bool &shifted);
  static bool encodeLogicalImmediate64(uint64_t value, uint32_t &field);
  size_t emitAdrPlaceholder(uint8_t rd);
  void emitMovReg(uint8_t rd, uint8_t rn);
  void emitAddRegImm(uint8_t rd, uint8_t rn, uint16_t imm);
  void emitSubRegImm(uint8_t rd, uint8_t rn, uint16_t imm);
  void emitAddReg(uint8_t rd, uint8_t rn, uint8_t rm);
  void emitSubReg(uint8_t rd, uint8_t rn, uint8_t rm);
  void emitMulReg(uint8_t rd, uint8_t rn, uint8_t rm);
  void emitUdivReg(uint8_t rd, uint8_t rn, uint8_t rm);
  void emitHeapPoolClass();
  void emitHeapStateAddress();

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

  static uint32_t encodeAddSpImm(uint16_t imm);
  static uint32_t encodeSubSpImm(uint16_t imm);
  static uint32_t encodeAddSpImmShifted(uint16_t imm);
  static uint32_t encodeSubSpImmShifted(uint16_t imm);
  static constexpr uint32_t AddImmBase = 0x91000000;
  static constexpr uint32_t AddsImmBase = 0xB1000000;
  static constexpr uint32_t SubImmBase = 0xD1000000;
  static constexpr uint32_t SubsImmBase = 0xF1000000;
  static uint32_t
  encodeAddSubImm(uint32_t base, uint8_t rd, uint8_t rn, uint16_t imm, bool shifted);
  static uint32_t encodeAddRegImm(uint8_t rd, uint8_t rn, uint16_t imm);
  static uint32_t encodeSubRegImm(uint8_t rd, uint8_t rn, uint16_t imm);
  static uint32_t encodeStrRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes);
  static uint32_t encodeLdrRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes);
  static uint32_t encodeAddReg(uint8_t rd, uint8_t rn, uint8_t rm);
  static uint32_t encodeSubReg(uint8_t rd, uint8_t rn, uint8_t rm);
  static uint32_t encodeSubsReg(uint8_t rd, uint8_t rn, uint8_t rm);
  static uint32_t encodeMulReg(uint8_t rd, uint8_t rn, uint8_t rm);
  static uint32_t encodeSdivReg(uint8_t rd, uint8_t rn, uint8_t rm);
  static uint32_t encodeUdivReg(uint8_t rd, uint8_t rn, uint8_t rm);
  static uint32_t encodeMovz(uint8_t rd, uint16_t imm, uint8_t shift);
  static uint32_t encodeMovk(uint8_t rd, uint16_t imm, uint8_t shift);
  static uint32_t encodeMovn(uint8_t rd, uint16_t imm, uint8_t shift);
  static uint32_t encodeMovnW(uint8_t rd, uint16_t imm, uint8_t shift);
  static uint32_t encodeOrrImm64(uint8_t rd, uint8_t rn, uint32_t logicalField);
  static uint32_t encodeFmovDX(uint8_t d, uint8_t x);
  static uint32_t encodeFmovXD(uint8_t x, uint8_t d);
  static uint32_t encodeFmovSW(uint8_t s, uint8_t w);
  static uint32_t encodeFmovWS(uint8_t w, uint8_t s);
  static uint32_t encodeAdr(uint8_t rd, int32_t imm21);
  static uint32_t encodeAdrp(uint8_t rd, int32_t imm21);
  static uint32_t encodeB(int32_t imm26);
  static uint32_t encodeBl(int32_t imm26);
  static uint32_t encodeBCond(int32_t imm19, uint8_t cond);
  static uint32_t encodeCbz(uint8_t rt, int32_t imm19);
  static uint32_t encodeStrbRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes);
  static uint32_t encodeLdrbRegBase(uint8_t rt, uint8_t rn, uint16_t offsetBytes);
  static uint32_t encodeSvc();
  static uint32_t encodeRet();
  static uint64_t localOffset(uint32_t index);
  void emitSpillReg(uint8_t reg);
  void emitReloadReg(uint8_t reg);
  void flushValueStackCache();

  Arm64CodeWords code_;
  uint64_t frameSize_ = 0;
  uint64_t codeBaseOffset_ = 0;
  Arm64InstrumentationCounters counters_;
  static constexpr uint8_t valueStackCacheReg_ = 26;
  // Pushes a pop may still turn into a move (emitReloadReg): while code_'s fusion window is open,
  // the words since the run began are only these pushes (`str reg, [x28, #-16]!`, by word index)
  // and the moves earlier pops became. Any other word or any read of a word index closes it.
  struct PendingSpill {
    size_t word = 0;
    uint8_t reg = 0;
  };
  std::vector<PendingSpill> pendingSpills_;
  std::vector<uint8_t> pendingMoveTargets_;
  bool hasValueStackCache_ = false;
  bool valueStackCacheEnabled_ = true;
  // The BL sites of the heap instructions, patched to the routines by emitHeapRuntime, and the state fixups.
  std::vector<size_t> heapAllocCalls_;
  std::vector<size_t> heapFreeCalls_;
  std::vector<size_t> heapReallocCalls_;
  std::vector<size_t> heapStateFixups_;
  bool registerAllocationEnabled_ = false;
  bool registerAllocationFarBranches_ = false;
};

#include "NativeEmitterInternalsArm64Arithmetic.h"
#include "NativeEmitterInternalsArm64Core.h"
#include "NativeEmitterInternalsArm64Io.h"
#include "NativeEmitterInternalsArm64Host.h"
#include "NativeEmitterInternalsArm64Heap.h"
#include "NativeEmitterInternalsArm64RegAlloc.h"

bool computeMaxStackDepth(const IrFunction &fn, const IrModule &module, int64_t &maxDepth, std::string &error);

bool writeBinaryFile(const std::string &path, const std::vector<uint8_t> &data, std::string &error);

// Writable data of a Mach-O image (the `__DATA` segment): one 8-byte pointer slot per imported function,
// which dyld binds from libSystem at load, followed by `stateBytes` of zeroed runtime state. Slot `i`
// is at the segment's address + 8 * i and the state at + 8 * imports.size().
//
// `dylibs` are further libraries the image loads (LC_LOAD_DYLIB paths, such as
// "@executable_path/libps_ui_appkit.dylib"). With any, the imports bind by flat lookup, so each symbol is
// taken from whichever loaded library has it (libSystem first).
struct MachOData {
  std::vector<std::string> imports; // C symbol names, without the leading underscore
  uint64_t stateBytes = 0;
  std::vector<std::string> dylibs;
};

bool buildMachO(const std::vector<uint8_t> &code,
                std::vector<uint8_t> &image,
                std::string &error,
                const MachOData *data = nullptr);

#if defined(__APPLE__)
uint32_t computeMachOCodeOffset(const MachOData *data = nullptr);
// The VM address of the data segment of an image whose code starts at `codeOffset` and is `codeSize`
// bytes long (code first, then the segment on the next page).
uint64_t machODataVmAddr(uint32_t codeOffset, uint64_t codeSize);
#endif

} // namespace primec::native_emitter
