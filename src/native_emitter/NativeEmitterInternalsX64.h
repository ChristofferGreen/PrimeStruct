#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"
#include "NativeEmitterRegAlloc.h"
#include "NativeJitOpcodes.h"

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
   // A local held in a register for the whole function; see
   // NativeEmitterPromotion.h. The emitter maps its loads and stores to register
   // moves and can write the registers to the frame slots and back around
   // instructions whose templates clobber them.
   struct PromotedLocalSlot {
     uint32_t index = 0;
     uint8_t reg = 0;
   };

   // Register-allocated function bodies (NativeEmitterRegAlloc.h,
   // NativeEmitterInternalsX64RegAlloc.h).
   struct RegAllocHooks {
     // Emits instruction `index` with its ordinary template on the memory operand stack.
     std::function<bool(size_t index)> emitTemplate;
     std::function<void(size_t fixupIndex, uint32_t stringIndex)> recordStringFixup;
     // Records the call placeholder at `fixupIndex` to function `target`; false (with the error
     // set) when there is no such function.
     std::function<bool(size_t fixupIndex, uint64_t target)> recordCallFixup;
     // The first three arguments travel in rax, rcx and rdx (first argument in rax) when the
     // callee takes them that way, the others on the operand stack; otherwise all on the stack.
     bool argumentsInRegisters = false;
     std::function<bool(uint64_t target)> calleeTakesRegisterArguments;
     // The function's index, which a JIT "missing return" fault reports, and the byte length of
     // module string `index` (JIT bounds checks).
     uint32_t functionIndex = 0;
     std::function<uint64_t(uint64_t index)> stringLength;
     uint64_t stringCount = 0;
     // JIT mode: the VM's local count (indirect addresses are byte offsets below count * 16), and
     // whether the frame's locals start at zero here as in the VM (functions that take an
     // address keep every local in its frame slot).
     uint32_t vmLocalCount = 0;
     bool zeroFrameLocals = false;
     // JIT mode: whether the module allocates heap memory, so an indirect address may name a
     // heap slot (tagged with bit 63) as well as a frame slot.
     bool heapAddresses = false;
   };

   // In-process execution (NativeJit.h): the entry function returns to a trampoline instead of
   // exiting, and every VM fault the code can meet (division by zero, a string index out of
   // bounds, call depth, falling off the end of a function) unwinds to the trampoline with a
   // fault code in the JIT data area instead of trapping. The data area is a page placed after
   // the code and strings and addressed RIP-relative; its fields are below.
   static constexpr uint32_t JitDataSavedStack = 0;
   static constexpr uint32_t JitDataCallDepth = 8;
   static constexpr uint32_t JitDataFaultCode = 16;
   static constexpr uint32_t JitDataFaultArgument = 24;
   // The runtime side (VmNativeJitHost): its context, the bridge function the code calls for
   // the opcodes nativeJitBridgesOpcode lists, and the heap it keeps for the code, which the code
   // reads and writes directly: the VM's slot values, their count, and one byte per slot that is
   // nonzero while the slot's allocation is live. The last field is scratch for the heap path.
   static constexpr uint32_t JitDataHostContext = 32;
   static constexpr uint32_t JitDataHostBridge = 40;
   static constexpr uint32_t JitDataHeapBase = 48;
   static constexpr uint32_t JitDataHeapSlots = 56;
   static constexpr uint32_t JitDataHeapLive = 64;
   static constexpr uint32_t JitDataScratch = 72;
   static constexpr uint64_t JitMaxCallDepth =
       4095; // the VM's limit of 4096 frames, entry included
   enum class JitFault : uint32_t {
     None = 0,
     DivisionByZero = 1,
     StringIndexOutOfBounds = 2,
     CallStackOverflow = 3,
     MissingReturn = 4, // argument: function index
     InvalidDynamicStringIndex = 5,
     InvalidStringIndex = 6,
     UnalignedIndirectAddress = 7, // argument: the address
     InvalidIndirectAddress = 8,   // argument: the address
     HostFault = 9,                // the bridge failed; the runtime holds the message
   };
   void setJitMode(bool enabled) {
     jitMode_ = enabled;
   }
   bool jitMode() const {
     return jitMode_;
   }
   // Around a call: fault when the call would exceed the VM's depth, else count it.
   void emitJitEnterCall();
   void emitJitLeaveCall();
   // After every function: the fault stubs and the trampoline the host calls as
   // uint64_t(uint64_t argc, char **argv, void *stackTop); returns the trampoline's offset.
   size_t emitJitRuntime(size_t entryOffset);
   // Resolves the data-area references once the data area's offset in the image is known.
   void patchJitData(std::vector<uint8_t> &image, size_t dataOffset) const;
   uint64_t maxFrameSize() const {
     return maxFrameSize_;
   }
   uint64_t entryFrameSize() const {
     return entryFrameSize_;
   }
   void setRegisterAllocationEnabled(bool enabled) {
     registerAllocationEnabled_ = enabled;
   }
   bool registerAllocationEnabled() const {
     return registerAllocationEnabled_;
   }
   // Emits `fn`'s body from `plan` (after beginFunction). Spill slot k and the save slot of
   // register r are the frame slots of pseudo-locals spillBaseLocal + k and
   // spillBaseLocal + plan.spillSlotCount + r.
   bool emitRegisterAllocatedFunction(const IrFunction &fn,
                                      const RegAllocFunctionPlan &plan,
                                      uint32_t spillBaseLocal,
                                      std::vector<size_t> &instOffsets,
                                      const RegAllocHooks &hooks,
                                      std::string &error);

   void setLocalPromotionEnabled(bool enabled) {
     localPromotionEnabled_ = enabled;
   }
   bool localPromotionEnabled() const {
     return localPromotionEnabled_;
   }
   // Deferred operands (optimized native code): pushes of constants and promoted
   // locals emit nothing until an instruction consumes them, and arithmetic works
   // on registers and immediates directly. Operands wait in `pending_`; the
   // memory-backed value stack holds everything below them. Off unless the backend
   // enables it for -O1 and above.
   void setOperandDeferralEnabled(bool enabled) {
     deferOperands_ = enabled;
   }
   bool operandDeferralEnabled() const {
     return deferOperands_;
   }
   // Brackets an instruction whose template clobbers the registers deferred
   // operands live in (everything outside the simple opcode set): pending operands
   // go to the memory stack first, and the promoted locals are saved and restored.
   // Starts a loop header on a 16-byte boundary (bounded padding).
   void alignLoopHeader();
   void beginComplexOp();
   void endComplexOp();
   // `a CMP b; JumpIfZero` as one compare and branch. Returns false when
   // `compareOp` is not an integer comparison; otherwise returns the branch fixup.
   bool tryEmitCompareBranch(IrOpcode compareOp, size_t &fixupIndex);
   // `if (compare) { local = local +/- constant }` as setcc and an add on the local's
   // register. Returns false (emitting nothing) when `compareOp` is not an integer
   // comparison or the local is not register-resident.
   bool tryEmitCompareConditionalAdd(IrOpcode compareOp,
                                     uint32_t local,
                                     bool subtract,
                                     uint64_t constant);
   bool isLocalPromoted(uint32_t index) const {
     return promotedRegister(index) >= 0;
   }
   // `local = local OP operand` for a promoted local, where the operand is another
   // local or a constant: add, sub or mul (kind 0, 1, 2).
   // `sext` follows the update with a 32-bit sign extension (an i32 operation).
   void emitPromotedLocalUpdate(uint32_t local,
                                int kind,
                                bool operandIsImm,
                                uint64_t imm,
                                uint32_t operandLocal,
                                bool sext = false);

   void setPromotedLocals(const std::vector<PromotedLocalSlot> &locals);
   void clearPromotedLocals();
   bool hasPromotedLocals() const {
     return !promotedLocals_.empty();
   }
   void emitInitPromotedLocals();
   void emitSpillPromotedLocals();
   void emitReloadPromotedLocals();

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
  // Replaces the top operand by its low 32 bits sign-extended, which is how the VM
  // reads an i32 slot for printing, file writes and returns.
  void emitSignExtendTop32();
  // The SextI32 opcode: the same on the top operand, but it works on the deferred
  // operand list, so wrapped i32 arithmetic keeps its operands in registers.
  void emitSextI32();
  void emitMovsxdRegReg(uint8_t rd, uint8_t rs); // rd = sign-extended low 32 bits of rs
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
     Parity,   // an unordered float comparison (a NaN operand)
     NoParity, // an ordered one
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
   void emitModRmReg(uint8_t reg, uint8_t rm); // mod=11 (register-direct)
   void emitModRmBaseDisp32(uint8_t reg, uint8_t base, int32_t disp);
   size_t emitModRmRipRelPlaceholder(uint8_t reg); // mod=00 rm=101, returns disp32 fixup index

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
   void emitAndRegReg(uint8_t rd, uint8_t rs);
   void emitOrRegReg(uint8_t rd, uint8_t rs);
   // target = (xmm a CC xmm b) as 0 or 1 with IEEE semantics: every comparison but != is false
   // when an operand is NaN. Clobbers rcx; `target` must not be rcx.
   void emitFloatCompareToReg(bool isF64, CondCode cc, uint8_t a, uint8_t b, uint8_t target);
   void emitCqo();                // sign-extend rax into rdx:rax
   void emitIdivReg(uint8_t reg); // signed divide rdx:rax by reg
   void emitDivReg(uint8_t reg);  // unsigned divide rdx:rax by reg
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
   // Keeps a jump (or a fused compare and jump) from crossing or ending on a 32-byte
   // boundary; see the definition.
   void alignBranchSequence(size_t sequenceStart, size_t length);
   static std::vector<uint8_t> makeNopPadding(size_t count);
   size_t compareStart_ = 0;
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
   void emitCvtsi2s(bool isF64, uint8_t dstXmm, uint8_t srcReg);  // int64 -> float
   void emitCvtts2si(bool isF64, uint8_t dstReg, uint8_t srcXmm); // float -> int64 (truncate)
   void emitCvtss2sd(uint8_t dstXmm, uint8_t srcXmm);
   void emitCvtsd2ss(uint8_t dstXmm, uint8_t srcXmm);
   void emitLoadXmmImm64(uint8_t xmm, uint64_t bits, uint8_t scratchReg);
   void emitMovapsXmm(uint8_t dstXmm, uint8_t srcXmm);
   void emitMovqXmmFromMem(uint8_t xmm, uint8_t base, int32_t disp); // low 64 bits, upper zeroed
   void emitMovqMemFromXmm(uint8_t base, int32_t disp, uint8_t xmm);
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
   // movzx rd, byte [base + index]
   void emitLoadMemByteIndexed(uint8_t rd, uint8_t base, uint8_t index);
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
   bool localPromotionEnabled_ = false;

   // An operand waiting to be materialized (see setOperandDeferralEnabled).
   struct PendingOperand {
     enum class Kind : uint8_t { Reg, Imm, Local };
     Kind kind = Kind::Imm;
     uint8_t reg = 0;    // Reg: the cache register; Local: the promoted local's register
     uint32_t local = 0; // Local: the promoted local's index
     uint64_t imm = 0;
   };
  bool deferOperands_ = false;
  bool registerAllocationEnabled_ = false;
  int32_t regAllocSlotDisp(uint32_t pseudoLocal) const;
  bool jitMode_ = false;
  uint64_t maxFrameSize_ = 0;
  uint64_t entryFrameSize_ = 0;
  struct JitDataReference {
    size_t position = 0; // of the rel32
    uint32_t field = 0;
    uint32_t trailing = 0; // immediate bytes after the rel32
  };
  struct JitFaultSite {
    size_t position = 0; // of the rel32
    JitFault fault = JitFault::None;
    uint32_t argument = 0;
    int argumentRegister = -1; // when set, the argument is this register's value
  };
  std::vector<JitDataReference> jitDataReferences_;
  std::vector<JitFaultSite> jitFaultSites_;
  // A RIP-relative ModRM operand naming a data-area field (mod=00, rm=101).
  void emitJitDataOperand(uint8_t reg, uint32_t field, uint32_t trailing = 0);
  // Jumps to the fault exit when `cc` holds (or always).
  void emitJitFaultIf(CondCode cc, JitFault fault, uint32_t argument = 0);
  void emitJitFaultIfWithRegister(CondCode cc, JitFault fault, uint8_t argumentRegister);
  // Zeroes the frame slots of locals [0, count) (register-allocated functions; uses r10, r11).
  void emitJitZeroFrameLocals(uint32_t count);
  void emitJitFault(JitFault fault, uint32_t argument = 0);
  // Runs instruction `irIndex` of function `functionIndex` through the runtime bridge, its
  // operands on the operand stack (left for the caller to drop) and its results written in
  // their place; faults with HostFault when the bridge fails. Clobbers every caller-saved
  // register.
  void emitJitHostCall(uint32_t functionIndex, uint32_t irIndex);
  bool inComplexOp_ = false;
  std::vector<PendingOperand> pending_;
  // Registers that hold pending Reg operands: r14 first (the only one inside a
  // complex template, as in the single-cache design), then rbx and r9, which the
  // simple opcodes' templates never touch.
  static constexpr uint8_t PendingRegs[3] = {14, 3, 9};

  bool pendingRegBusy(uint8_t reg) const;
  uint8_t allocPendingReg(uint32_t excludeMask);
  void spillPendingEntry(const PendingOperand &entry);
  void spillFrontPending();
  void materializePending(const PendingOperand &entry, uint8_t reg);
  PendingOperand popOperand(uint32_t &usedMask);
  void pushPendingOperand(const PendingOperand &entry);
  void flushPendingForAlias(uint32_t local);
  void emitDeferredCompareFlags();
  void emitStoreImm64Mem(uint8_t base, int32_t disp, uint64_t imm);
  void emitTestRegReg(uint8_t reg);
  template <typename Op> void emitBinaryDeferred(Op &&op);
  void emitCompareDeferred(CondCode cc);
  void emitNegDeferred();
  void emitDupDeferred();
  void emitPopDeferred();
  void emitLoadLocalDeferred(uint32_t index);
  void emitStoreLocalDeferred(uint32_t index);
  void emitPushImmDeferred(uint64_t imm);
  size_t emitJumpIfZeroDeferred();
  bool compareCondition(IrOpcode op, CondCode &cc) const;
  std::vector<PromotedLocalSlot> promotedLocals_;
  // Register of each promoted local by index, -1 for locals kept in the frame.
  std::vector<int8_t> promotedRegByLocal_;

  int promotedRegister(uint32_t index) const {
    return index < promotedRegByLocal_.size() ? promotedRegByLocal_[index] : -1;
  }
};

#include "NativeEmitterInternalsX64Core.h"
#include "NativeEmitterInternalsX64Arithmetic.h"
#include "NativeEmitterInternalsX64Io.h"
#include "NativeEmitterInternalsX64Deferred.h"
#include "NativeEmitterInternalsX64RegAlloc.h"
#include "NativeEmitterInternalsX64Jit.h"

uint32_t computeElfCodeOffset();
bool buildElf(const std::vector<uint8_t> &code, std::vector<uint8_t> &image, std::string &error);

#endif // defined(__linux__)

} // namespace primec::native_emitter
