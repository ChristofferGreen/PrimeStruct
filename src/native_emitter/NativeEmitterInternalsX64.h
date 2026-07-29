#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ir.h"

#if defined(__linux__)
#include <sys/mman.h>
#include <sys/syscall.h>
#endif

namespace primec::native_emitter {

// `alignTo`, `PageSize` (already correctly 0x1000 on any non-arm64
// platform, including Linux x86_64), and `HeapHeaderMagic` are shared,
// arch-agnostic definitions from NativeEmitterInternals.h - reused as-is
// rather than redeclared here, since NativeEmitterEmitInternal.h always
// includes both headers together in the same translation unit.

#if defined(__linux__)
constexpr uint64_t ElfLoadAddress = 0x400000ull;
constexpr uint32_t PrintScratchBytes = 32;
constexpr uint32_t PrintScratchSlots = (PrintScratchBytes + 15) / 16;

constexpr uint64_t LinuxSysRead = SYS_read;
constexpr uint64_t LinuxSysWrite = SYS_write;
constexpr uint64_t LinuxSysOpen = SYS_open;
constexpr uint64_t LinuxSysClose = SYS_close;
constexpr uint64_t LinuxSysFsync = SYS_fsync;
constexpr uint64_t LinuxSysMmap = SYS_mmap;
constexpr uint64_t LinuxSysMunmap = SYS_munmap;
constexpr uint64_t LinuxSysExit = SYS_exit;
constexpr uint64_t LinuxSysExitGroup = SYS_exit_group;
constexpr uint64_t LinuxMmapProtReadWrite = static_cast<uint64_t>(PROT_READ | PROT_WRITE);
constexpr uint64_t LinuxMmapFlagsPrivateAnon = static_cast<uint64_t>(MAP_PRIVATE | MAP_ANONYMOUS);
#endif

#if defined(__linux__)

struct X64InstrumentationCounters {
  uint64_t valueStackPushCount = 0;
  uint64_t valueStackPopCount = 0;
  uint64_t spillCount = 0;
  uint64_t reloadCount = 0;
};

// x86_64/Linux counterpart to Arm64Emitter (NativeEmitterInternals.h).
// Exposes the same public method names/signatures so
// NativeEmitterFunctionEmit.cpp's IR-dispatch loop can be templated over
// either emitter type - see docs/todo.md's TODO-4747 x86_64 backend entry
// for the full register-role mapping this mirrors (rsp/rbp=frame,
// r15=value-stack pointer, r14=value-stack cache register,
// r12/r13=argc/argv).
//
// Unlike Arm64Emitter's word-addressed `code_` (every ARM64 instruction is
// a fixed 4 bytes), x86_64 instructions are variable-length, so `code_`
// here is byte-addressed. `currentWordIndex()` is kept as the method name
// for interface parity with Arm64Emitter even though it returns a byte
// offset on this backend - callers only use it as an opaque position for
// offset math, never interpret the unit directly.
class X64Emitter {
 public:
  void setValueStackCacheEnabled(bool enabled) {
    valueStackCacheEnabled_ = enabled;
    if (!valueStackCacheEnabled_) {
      hasValueStackCache_ = false;
    }
  }

  void flushValueStackCachePublic() {
    flushValueStackCache();
  }

  bool beginFunction(uint64_t frameSize, bool resetValueStack, std::string &error);
  void emitCaptureEntryArgs();
  void emitMovRegPublic(uint8_t rd, uint8_t rn);

  void emitPushI32(int32_t value);
  void emitPushI64(uint64_t value);
  void emitPushF32(uint32_t bits);
  void emitPushF64(uint64_t bits);
  void emitLoadLocal(uint32_t index);
  void emitLoadLocalToReg(uint8_t reg, uint32_t index);
  void emitAddressOfLocal(uint32_t index);
  void emitStoreLocal(uint32_t index);
  void emitStoreLocalFromReg(uint32_t index, uint8_t reg);
  void emitLoadIndirect();
  void emitStoreIndirect();

  void emitHeapAlloc();
  void emitHeapFree();
  void emitHeapRealloc();

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

  void emitReturn();
  void emitReturnVoid();
  void emitReturnWithLink(uint32_t linkLocalIndex);
  void emitReturnVoidWithLink(uint32_t linkLocalIndex);
  void emitReturnWithFrameAndLink(uint32_t frameLocalIndex, uint32_t linkLocalIndex);
  void emitReturnVoidWithFrameAndLink(uint32_t frameLocalIndex, uint32_t linkLocalIndex);

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

  void patchAdr(size_t index, uint8_t rd, int32_t deltaBytes);

  void setCodeBaseOffset(uint32_t offsetBytes) {
    codeBaseOffset_ = offsetBytes;
  }

  std::vector<uint8_t> finalize() const {
    return code_;
  }

  const X64InstrumentationCounters &instrumentationCounters() const {
    return counters_;
  }

 private:
  enum class CondCode : uint8_t {
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    Below,
    BelowEq,
    Above,
    AboveEq,
  };

  static CondCode invertCond(CondCode cond);

  void emitByte(uint8_t byte);
  void emitU32(uint32_t value);
  void emitU64(uint64_t value);
  void patchByte(size_t index, uint8_t byte);
  void patchU32(size_t index, uint32_t value);

  // REX prefix: 0100WRXB. `w`=64-bit operand, `regExt`/`rmExt` are the
  // high bit of a 4-bit register index used in the ModRM.reg / ModRM.rm
  // (or opcode-embedded) field respectively.
  void emitRex(bool w, uint8_t reg, uint8_t rmOrBase);
  void emitModRmReg(uint8_t reg, uint8_t rm);        // mod=11 (register-direct)
  void emitModRmBaseDisp32(uint8_t reg, uint8_t base, int32_t disp);
  size_t emitModRmRipRelPlaceholder(uint8_t reg);    // mod=00 rm=101, returns disp32 fixup index

  void emitMovRegImm64(uint8_t rd, uint64_t imm);
  void emitMovRegReg(uint8_t rd, uint8_t rs);
  void emitLoadMem(uint8_t rd, uint8_t base, int32_t disp);
  void emitStoreMem(uint8_t base, int32_t disp, uint8_t rs);
  void emitAddRegImm32(uint8_t rd, int32_t imm);
  void emitSubRegImm32(uint8_t rd, int32_t imm);
  void emitPushReg64(uint8_t reg);
  void emitPopReg64(uint8_t reg);
  void emitSyscall();
  void emitRet();

  // Integer arithmetic/compare primitives (all operate on 64-bit GPRs -
  // this backend mirrors Arm64Emitter's existing simplification of using
  // the same op for both I32 and I64 IR opcodes, see emitAdd()'s doc).
  void emitAddRegReg(uint8_t rd, uint8_t rs);
  void emitSubRegReg(uint8_t rd, uint8_t rs);
  void emitImulRegReg(uint8_t rd, uint8_t rs); // rd *= rs (RM form: reg=dst)
  void emitXorRegReg(uint8_t rd, uint8_t rs);
  void emitCqo();                    // sign-extend rax into rdx:rax
  void emitIdivReg(uint8_t reg);     // signed divide rdx:rax by reg
  void emitDivReg(uint8_t reg);      // unsigned divide rdx:rax by reg
  void emitNegReg(uint8_t rd);
  void emitCmpRegReg(uint8_t a, uint8_t b); // flags = a - b
  void emitSetccReg(uint8_t rd, CondCode cc);
  void emitMovzxReg8(uint8_t rd, uint8_t rs); // rd = zero-extend(low byte of rs)
  static uint8_t condCodeValue(CondCode cc);

  // Raw conditional-jump placeholder/patch for control flow *internal* to
  // a single emitted routine (e.g. the unsigned int64<->float conversion
  // sequences below), independent of the IR-level Jump/JumpIfZero fixup
  // lists the dispatch loop manages - resolved immediately within the
  // same method that emits the placeholder, never left pending.
  size_t emitCondJumpPlaceholder(CondCode cc);
  void patchCondJumpHere(size_t fixupIndex);
  size_t emitJumpPlaceholderRaw();
  void patchJumpHere(size_t fixupIndex);

  // SSE2 float primitives (xmm registers 0-15, same numbering scheme as
  // GPRs). `isF64` selects the F2 (double) vs F3 (single) SSE prefix.
  void emitMovqXmmFromReg(uint8_t xmm, uint8_t reg);
  void emitMovqRegFromXmm(uint8_t reg, uint8_t xmm);
  void emitSseBinaryOp(bool isF64, uint8_t opcode, uint8_t dstXmm, uint8_t srcXmm);
  void emitXorpsXmm(uint8_t dstXmm, uint8_t srcXmm);
  void emitComiss(bool isF64, uint8_t a, uint8_t b);
  void emitCvtsi2s(bool isF64, uint8_t dstXmm, uint8_t srcReg);   // int64 -> float
  void emitCvtts2si(bool isF64, uint8_t dstReg, uint8_t srcXmm);  // float -> int64 (truncate)
  void emitCvtss2sd(uint8_t dstXmm, uint8_t srcXmm);
  void emitCvtsd2ss(uint8_t dstXmm, uint8_t srcXmm);
  void emitLoadXmmImm64(uint8_t xmm, uint64_t bits, uint8_t scratchReg);
  void emitConvertUnsignedToFloat(bool isF64);
  void emitConvertFloatToUnsigned(bool isF64);

  void emitPushReg(uint8_t reg);
  void emitPopReg(uint8_t reg);
  void emitSpillReg(uint8_t reg);
  void emitReloadReg(uint8_t reg);
  void flushValueStackCache();
  static uint64_t localOffset(uint32_t index);

  void emitExitSyscall();
  void emitCompareAndPush(CondCode cc);
  void emitFloatBinaryOp(bool isF64, uint8_t opcode);
  void emitFloatNegate(bool isF64);
  void emitFloatCompareAndPush(bool isF64, CondCode cc);
  void emitConvertIntToFloatImpl(bool isF64);
  void emitConvertFloatToIntImpl(bool isF64);

  void emitHeapAllocFromSlotCountReg(uint8_t slotCountReg, uint8_t resultReg);
  void emitHeapFreeFromAddressReg(uint8_t addressReg);

  // Byte-granularity memory access (MOVZX load / MOV r/m8,r8 store) - not
  // needed by the compute core, but required for scratch-buffer digit
  // writing, string-byte access, and single-byte file I/O below.
  void emitLoadMemByte(uint8_t rd, uint8_t base, int32_t disp);
  void emitStoreMemByte(uint8_t base, int32_t disp, uint8_t rs);
  void emitCmpRegImm32(uint8_t reg, int32_t imm);

  // rd = rbp - offsetBytes: the same "offset measured away from the frame
  // pointer" coordinate space localOffset() already uses for locals (see
  // Core.h's class-comment header), extended to address the scratch
  // region that NativeEmitterEmit.cpp lays out immediately after the
  // locals (scratchOffset = localCount*16, see NativeEmitterFunctionLayout).
  void emitLoadFrameOffset(uint8_t rd, uint32_t offsetBytes);

  void emitWriteSyscall(uint64_t fd, uint8_t bufferReg, uint8_t lengthReg);
  void emitWriteSyscallReg(uint8_t fdReg, uint8_t bufferReg, uint8_t lengthReg);
  void emitReadSyscallReg(uint8_t fdReg, uint8_t bufferReg, uint8_t lengthReg);
  void emitWriteNewline(uint64_t fd, uint32_t scratchOffset);
  void emitWriteNewlineReg(uint8_t fdReg, uint32_t scratchOffset);
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

  // RIP-relative LEA placeholder (x86_64 counterpart to Arm64Emitter's
  // emitAdrPlaceholder): returns the fixup index of the disp32 field,
  // resolved later via patchAdr once the string table's final position
  // is known.
  size_t emitLeaRipPlaceholder(uint8_t rd);

  // Pops an index off the value stack and resolves it through the string
  // offset table into an absolute address, left in `resultReg`. Mirrors
  // the address-only half of Arm64Emitter's inlined
  // emitFileOpenDynamicPlaceholder sequence.
  size_t emitResolveDynamicStringAddress(uint64_t offsetTableDelta, uint8_t resultReg);

  // Resolves `indexReg` (already holding an index, NOT popped - callers
  // that need the index off the value stack pop it themselves first)
  // through both the offset table (-> address, in addrReg) and the
  // parallel length table (-> byte length, in lengthReg). Mirrors
  // Arm64Emitter's inlined emitPrintStringDynamicPlaceholder /
  // emitFileWriteStringDynamicPlaceholder sequence. Clobbers indexReg.
  size_t emitResolveDynamicStringAddressAndLength(uint64_t offsetTableDelta,
                                                  uint64_t offsetTableSize,
                                                  uint8_t indexReg,
                                                  uint8_t addrReg,
                                                  uint8_t lengthReg);

  std::vector<uint8_t> code_;
  uint64_t frameSize_ = 0;
  uint64_t codeBaseOffset_ = 0;
  X64InstrumentationCounters counters_;
  static constexpr uint8_t valueStackCacheReg_ = 14; // r14
  bool hasValueStackCache_ = false;
  bool valueStackCacheEnabled_ = true;
  // Set by beginFunction's `resetValueStack` parameter (which the shared
  // NativeEmitterFunctionEmit.cpp dispatch loop already passes as
  // `isEntryFunction` - see NativeEmitterInternalsX64Core.h's comment on
  // beginFunction). A raw Linux ELF entry point has no caller and no
  // return address on the stack (unlike Mach-O's LC_MAIN, which dyld
  // calls properly), so `ret` there would jump to garbage - the entry
  // function's Return* opcodes must exit_group(value) instead.
  bool isEntryFunction_ = false;
};

#include "NativeEmitterInternalsX64Core.h"
#include "NativeEmitterInternalsX64Arithmetic.h"
#include "NativeEmitterInternalsX64Io.h"

uint32_t computeElfCodeOffset();
bool buildElf(const std::vector<uint8_t> &code, std::vector<uint8_t> &image, std::string &error);

#endif // defined(__linux__)

} // namespace primec::native_emitter
