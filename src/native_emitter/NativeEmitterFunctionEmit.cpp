#include "NativeEmitterEmitInternal.h"
#include "NativeEmitterPromotion.h"

#include <fcntl.h>
#include <type_traits>

namespace primec::native_emitter {

template <typename EmitterT>
bool emitNativeFunctions(const IrModule &module,
                         size_t entryIndex,
                         const std::vector<NativeEmitterFunctionLayout> &layouts,
                         const std::vector<size_t> &emitOrder,
                         EmitterT &emitter,
                         std::vector<NativeEmitterBranchFixup> &branchFixups,
                         std::vector<NativeEmitterCallFixup> &callFixups,
                         std::vector<NativeEmitterStringFixup> &stringFixups,
                         std::vector<size_t> &stringTableFixups,
                         uint64_t stringTableOffsetDelta,
                         uint64_t stringOffsetTableSize,
                         std::vector<size_t> &functionOffsets,
                         std::vector<std::vector<size_t>> &instOffsets,
                         NativeEmitterInstrumentation *instrumentation,
                         std::string &error) {
  constexpr bool kIsArm64 = std::is_same_v<EmitterT, Arm64Emitter>;
  // x86_64 keeps argc/argv in r12/r13 for the whole program, but only functions
  // whose layout asks for them read them; without any, the registers are free.
  bool argRegsFree = true;
  for (const NativeEmitterFunctionLayout &layout : layouts) {
    if (layout.needsArgc || layout.needsArgv) {
      argRegsFree = false;
      break;
    }
  }
  // Register allocation (x86_64, NativeEmitterRegAlloc.h) plans every function before any is
  // emitted, so calls know how their callee takes its arguments.
  std::vector<RegAllocFunctionPlan> regAllocPlans(module.functions.size());
  std::vector<bool> registerAllocated(module.functions.size(), false);
  // Functions whose first three arguments arrive in rax, rcx and rdx (the others still on the
  // operand stack): not the entry, and every caller register-allocated (the template callers
  // pass arguments on the operand stack).
  std::vector<bool> registerArguments(module.functions.size(), false);
  if constexpr (!kIsArm64) {
    if (emitter.registerAllocationEnabled()) {
      for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
        bool stringsValid = true;
        for (const IrInstruction &inst : module.functions[functionIndex].instructions) {
          if (inst.op == IrOpcode::LoadStringByte && inst.imm >= module.stringTable.size()) {
            stringsValid = false;
          }
        }
        std::string planError = "string index out of range";
        registerAllocated[functionIndex] =
            stringsValid &&
            planNativeRegisterAllocation(module,
                                         functionIndex,
                                         argRegsFree ? X64RegAllocPoolWithArgRegs : X64RegAllocPool,
                                         regAllocPlans[functionIndex],
                                         planError);
        if (instrumentation != nullptr) {
          auto &functionInstrumentation = instrumentation->perFunction[functionIndex];
          functionInstrumentation.registerAllocated = registerAllocated[functionIndex];
          functionInstrumentation.registerAllocationSpillSlots =
              registerAllocated[functionIndex] ? regAllocPlans[functionIndex].spillSlotCount : 0;
          functionInstrumentation.registerAllocationFallback =
              registerAllocated[functionIndex] ? std::string() : planError;
        }
      }
      for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
        const RegAllocFunctionPlan &plan = regAllocPlans[functionIndex];
        registerArguments[functionIndex] =
            registerAllocated[functionIndex] && functionIndex != entryIndex && !plan.blocks.empty();
      }
      for (size_t caller = 0; caller < module.functions.size(); ++caller) {
        if (registerAllocated[caller]) {
          continue;
        }
        for (const IrInstruction &inst : module.functions[caller].instructions) {
          if ((inst.op == IrOpcode::Call || inst.op == IrOpcode::CallVoid) &&
              inst.imm < module.functions.size()) {
            registerArguments[static_cast<size_t>(inst.imm)] = false;
          }
        }
      }
    }
  }
  for (size_t functionIndex : emitOrder) {
    const auto countersBefore = emitter.instrumentationCounters();
    const IrFunction &fn = module.functions[functionIndex];
    const NativeEmitterFunctionLayout &layout = layouts[functionIndex];
    const bool isEntryFunction = functionIndex == entryIndex;
    // The register allocator's spill slots and the save slots of the 16 general and 16 xmm
    // registers extend the frame past the locals and the print scratch area.
    const RegAllocFunctionPlan &regAllocPlan = regAllocPlans[functionIndex];
    const bool useRegisterAllocation = registerAllocated[functionIndex];
    if constexpr (!kIsArm64) {
      // The JIT's fault checks live in the register-allocated emitter only.
      if (emitter.jitMode() && !useRegisterAllocation) {
        error = "native JIT needs every function register-allocated";
        return false;
      }
    }
    const uint32_t spillBaseLocal = static_cast<uint32_t>(layout.localCount + layout.scratchSlots);
    const uint64_t regAllocBytes =
        useRegisterAllocation ? (static_cast<uint64_t>(regAllocPlan.spillSlotCount) + 32) * 16 : 0;
    uint64_t frameSize = alignTo(layout.frameSize + regAllocBytes, 16);
    if (isEntryFunction) {
      constexpr uint64_t ValueStackBytes = 1024ull * 1024ull;
      frameSize = alignTo(layout.localsSize + regAllocBytes + ValueStackBytes, 16);
    }
    functionOffsets[functionIndex] = emitter.currentWordIndex();
    if constexpr (kIsArm64) {
      // Save the caller's frame pointer (x27) into x21 before
      // beginFunction overwrites x27 with this function's own frame base
      // - later stashed into layout.framePointerLocalIndex below and
      // restored from there at return time. x86_64 needs none of this:
      // its call/ret plus push-rbp/pop-rbp already save/restore the
      // caller's frame pointer and return address on the real stack (see
      // X64Emitter's class-level design comment).
      emitter.emitMovRegPublic(21, 27);
    }
    if (!emitter.beginFunction(frameSize, isEntryFunction, error)) {
      return false;
    }
    // In JIT mode the trampoline passes argc and argv in r12 and r13 itself.
    bool captureEntryArgs = isEntryFunction && (kIsArm64 || !argRegsFree);
    if constexpr (!kIsArm64) {
      captureEntryArgs = captureEntryArgs && !emitter.jitMode();
    }
    if (captureEntryArgs) {
      emitter.emitCaptureEntryArgs();
    }
    if constexpr (kIsArm64) {
      if (layout.needsArgc) {
        emitter.emitStoreLocalFromReg(layout.argcLocalIndex, 19);
      }
      if (layout.needsArgv) {
        emitter.emitStoreLocalFromReg(layout.argvLocalIndex, 20);
      }
      emitter.emitStoreLocalFromReg(layout.framePointerLocalIndex, 21);
      emitter.emitStoreLocalFromReg(layout.linkLocalIndex, 30);
    } else {
      // X64Emitter::emitCaptureEntryArgs() already placed argc/argv in
      // r12/r13; framePointerLocalIndex/linkLocalIndex have no x86_64
      // equivalent to store (see above).
      if (layout.needsArgc) {
        emitter.emitStoreLocalFromReg(layout.argcLocalIndex, 12);
      }
      if (layout.needsArgv) {
        emitter.emitStoreLocalFromReg(layout.argvLocalIndex, 13);
      }
    }
    // Keep the hottest unaddressed locals in registers (x86_64 only). Everything
    // else about the function is emitted exactly as without promotion.
    bool hasPromotedLocals = false;
    if constexpr (!kIsArm64) {
      emitter.clearPromotedLocals();
      if (emitter.localPromotionEnabled() && !useRegisterAllocation) {
        std::vector<X64Emitter::PromotedLocalSlot> promoted;
        const uint8_t *pool = argRegsFree ? X64PromotionPoolWithArgRegs : X64PromotionPool;
        const size_t poolSize =
            argRegsFree ? sizeof(X64PromotionPoolWithArgRegs) : sizeof(X64PromotionPool);
        for (const PromotedLocal &local : planPromotedLocals(fn, pool, poolSize)) {
          promoted.push_back({local.index, local.reg});
        }
        emitter.setPromotedLocals(promoted);
        emitter.emitInitPromotedLocals();
        hasPromotedLocals = emitter.hasPromotedLocals();
      }
    }
    instOffsets[functionIndex].assign(fn.instructions.size() + 1, 0);
    std::vector<bool> branchTargets(fn.instructions.size() + 1, false);
    std::vector<bool> loopHeaders(fn.instructions.size() + 1, false);
    for (size_t jumpIndex = 0; jumpIndex < fn.instructions.size(); ++jumpIndex) {
      const auto &inst = fn.instructions[jumpIndex];
      if ((inst.op == IrOpcode::Jump || inst.op == IrOpcode::JumpIfZero) &&
          inst.imm <= fn.instructions.size()) {
        branchTargets[static_cast<size_t>(inst.imm)] = true;
        if (inst.imm <= jumpIndex) {
          loopHeaders[static_cast<size_t>(inst.imm)] = true;
        }
      }
    }

    // One instruction through its template; also the register allocator's fallback, which
    // calls it with the operands already on the memory operand stack.
    const auto emitTemplateInstruction = [&](size_t index) -> bool {
      const auto &inst = fn.instructions[index];
      switch (inst.op) {
      case IrOpcode::PushI32:
        emitter.emitPushI32(static_cast<int32_t>(inst.imm));
        break;
      case IrOpcode::PushI64:
        emitter.emitPushI64(inst.imm);
        break;
      case IrOpcode::PushF32:
        emitter.emitPushF32(static_cast<uint32_t>(inst.imm));
        break;
      case IrOpcode::PushF64:
        emitter.emitPushF64(inst.imm);
        break;
      case IrOpcode::PushArgc:
        emitter.emitLoadLocal(layout.argcLocalIndex);
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
      case IrOpcode::HeapAlloc:
        emitter.emitHeapAlloc();
        break;
      case IrOpcode::HeapFree:
        emitter.emitHeapFree();
        break;
      case IrOpcode::HeapRealloc:
        emitter.emitHeapRealloc();
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
      case IrOpcode::SextI32:
        emitter.emitSextI32();
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
      case IrOpcode::AddF32:
        emitter.emitAddF32();
        break;
      case IrOpcode::SubF32:
        emitter.emitSubF32();
        break;
      case IrOpcode::MulF32:
        emitter.emitMulF32();
        break;
      case IrOpcode::DivF32:
        emitter.emitDivF32();
        break;
      case IrOpcode::NegF32:
        emitter.emitNegF32();
        break;
      case IrOpcode::AddF64:
        emitter.emitAddF64();
        break;
      case IrOpcode::SubF64:
        emitter.emitSubF64();
        break;
      case IrOpcode::MulF64:
        emitter.emitMulF64();
        break;
      case IrOpcode::DivF64:
        emitter.emitDivF64();
        break;
      case IrOpcode::NegF64:
        emitter.emitNegF64();
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
      case IrOpcode::CmpEqF32:
        emitter.emitCmpEqF32();
        break;
      case IrOpcode::CmpNeF32:
        emitter.emitCmpNeF32();
        break;
      case IrOpcode::CmpLtF32:
        emitter.emitCmpLtF32();
        break;
      case IrOpcode::CmpLeF32:
        emitter.emitCmpLeF32();
        break;
      case IrOpcode::CmpGtF32:
        emitter.emitCmpGtF32();
        break;
      case IrOpcode::CmpGeF32:
        emitter.emitCmpGeF32();
        break;
      case IrOpcode::CmpEqF64:
        emitter.emitCmpEqF64();
        break;
      case IrOpcode::CmpNeF64:
        emitter.emitCmpNeF64();
        break;
      case IrOpcode::CmpLtF64:
        emitter.emitCmpLtF64();
        break;
      case IrOpcode::CmpLeF64:
        emitter.emitCmpLeF64();
        break;
      case IrOpcode::CmpGtF64:
        emitter.emitCmpGtF64();
        break;
      case IrOpcode::CmpGeF64:
        emitter.emitCmpGeF64();
        break;
      case IrOpcode::ConvertI32ToF32:
        emitter.emitConvertI32ToF32();
        break;
      case IrOpcode::ConvertI32ToF64:
        emitter.emitConvertI32ToF64();
        break;
      case IrOpcode::ConvertI64ToF32:
        emitter.emitConvertI64ToF32();
        break;
      case IrOpcode::ConvertI64ToF64:
        emitter.emitConvertI64ToF64();
        break;
      case IrOpcode::ConvertU64ToF32:
        emitter.emitConvertU64ToF32();
        break;
      case IrOpcode::ConvertU64ToF64:
        emitter.emitConvertU64ToF64();
        break;
      case IrOpcode::ConvertF32ToI32:
        emitter.emitConvertF32ToI32();
        break;
      case IrOpcode::ConvertF32ToI64:
        emitter.emitConvertF32ToI64();
        break;
      case IrOpcode::ConvertF32ToU64:
        emitter.emitConvertF32ToU64();
        break;
      case IrOpcode::ConvertF64ToI32:
        emitter.emitConvertF64ToI32();
        break;
      case IrOpcode::ConvertF64ToI64:
        emitter.emitConvertF64ToI64();
        break;
      case IrOpcode::ConvertF64ToU64:
        emitter.emitConvertF64ToU64();
        break;
      case IrOpcode::ConvertF32ToF64:
        emitter.emitConvertF32ToF64();
        break;
      case IrOpcode::ConvertF64ToF32:
        emitter.emitConvertF64ToF32();
        break;
      case IrOpcode::JumpIfZero: {
        NativeEmitterBranchFixup fixup;
        fixup.codeIndex = emitter.emitJumpIfZeroPlaceholder();
        fixup.functionIndex = functionIndex;
        fixup.targetInst = static_cast<size_t>(inst.imm);
        fixup.isConditional = true;
        branchFixups.push_back(fixup);
        break;
      }
      case IrOpcode::Jump: {
        emitter.flushValueStackCachePublic();
        NativeEmitterBranchFixup fixup;
        fixup.codeIndex = emitter.emitJumpPlaceholder();
        fixup.functionIndex = functionIndex;
        fixup.targetInst = static_cast<size_t>(inst.imm);
        fixup.isConditional = false;
        branchFixups.push_back(fixup);
        break;
      }
      case IrOpcode::Call: {
        if (inst.imm >= module.functions.size()) {
          error = "native backend detected invalid call target";
          return false;
        }
        // The value-stack cache register is a single global slot with no
        // save/restore around calls, so anything left cached here (e.g.
        // an operand pushed just before this call, still awaiting a
        // later pop) would be silently clobbered by the callee's own use
        // of the same register - flush it to the real, memory-backed
        // stack first so it survives the call. This is the TODO-4747
        // Step 3 finding, confirmed by actually running compiled output:
        // a function that used a call's result in further arithmetic
        // segfaulted/produced wrong results without this flush.
        emitter.flushValueStackCachePublic();
        callFixups.push_back({emitter.emitCallPlaceholder(), static_cast<size_t>(inst.imm)});
        emitter.emitPushReg0();
        break;
      }
      case IrOpcode::CallVoid: {
        if (inst.imm >= module.functions.size()) {
          error = "native backend detected invalid call target";
          return false;
        }
        emitter.flushValueStackCachePublic();
        callFixups.push_back({emitter.emitCallPlaceholder(), static_cast<size_t>(inst.imm)});
        break;
      }
      case IrOpcode::ReturnVoid:
        emitter.emitReturnVoidWithFrameAndLink(layout.framePointerLocalIndex, layout.linkLocalIndex);
        break;
      case IrOpcode::ReturnI32:
        if constexpr (!kIsArm64) {
          emitter.emitSignExtendTop32();
        }
        emitter.emitReturnWithFrameAndLink(layout.framePointerLocalIndex, layout.linkLocalIndex);
        break;
      case IrOpcode::ReturnI64:
        emitter.emitReturnWithFrameAndLink(layout.framePointerLocalIndex, layout.linkLocalIndex);
        break;
      case IrOpcode::ReturnF32:
        emitter.emitReturnWithFrameAndLink(layout.framePointerLocalIndex, layout.linkLocalIndex);
        break;
      case IrOpcode::ReturnF64:
        emitter.emitReturnWithFrameAndLink(layout.framePointerLocalIndex, layout.linkLocalIndex);
        break;
      case IrOpcode::PrintI32: {
        if constexpr (!kIsArm64) {
          emitter.emitSignExtendTop32();
        }
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        emitter.emitPrintSigned(layout.scratchOffset, layout.scratchBytes, newline, fd);
        break;
      }
      case IrOpcode::PrintI64: {
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        emitter.emitPrintSigned(layout.scratchOffset, layout.scratchBytes, newline, fd);
        break;
      }
      case IrOpcode::PrintU64: {
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        emitter.emitPrintUnsigned(layout.scratchOffset, layout.scratchBytes, newline, fd);
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
            module.stringTable[static_cast<size_t>(stringIndex)].size(), layout.scratchOffset, newline, fd);
        stringFixups.push_back({fixupIndex, static_cast<uint32_t>(stringIndex)});
        break;
      }
      case IrOpcode::PrintStringDynamic: {
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        size_t fixupIndex =
            emitter.emitPrintStringDynamicPlaceholder(stringTableOffsetDelta, stringOffsetTableSize,
                                                      layout.scratchOffset, newline, fd);
        stringTableFixups.push_back(fixupIndex);
        break;
      }
      case IrOpcode::FileOpenRead: {
        if (inst.imm >= module.stringTable.size()) {
          error = "native backend encountered invalid string index";
          return false;
        }
        size_t fixupIndex = emitter.emitFileOpenPlaceholder(O_RDONLY, 0);
        stringFixups.push_back({fixupIndex, static_cast<uint32_t>(inst.imm)});
        break;
      }
      case IrOpcode::FileOpenWrite: {
        if (inst.imm >= module.stringTable.size()) {
          error = "native backend encountered invalid string index";
          return false;
        }
        size_t fixupIndex = emitter.emitFileOpenPlaceholder(O_WRONLY | O_CREAT | O_TRUNC, 0644);
        stringFixups.push_back({fixupIndex, static_cast<uint32_t>(inst.imm)});
        break;
      }
      case IrOpcode::FileOpenAppend: {
        if (inst.imm >= module.stringTable.size()) {
          error = "native backend encountered invalid string index";
          return false;
        }
        size_t fixupIndex = emitter.emitFileOpenPlaceholder(O_WRONLY | O_CREAT | O_APPEND, 0644);
        stringFixups.push_back({fixupIndex, static_cast<uint32_t>(inst.imm)});
        break;
      }
      case IrOpcode::FileOpenReadDynamic: {
        size_t fixupIndex = emitter.emitFileOpenDynamicPlaceholder(stringTableOffsetDelta, O_RDONLY, 0);
        stringTableFixups.push_back(fixupIndex);
        break;
      }
      case IrOpcode::FileOpenWriteDynamic: {
        size_t fixupIndex =
            emitter.emitFileOpenDynamicPlaceholder(stringTableOffsetDelta, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        stringTableFixups.push_back(fixupIndex);
        break;
      }
      case IrOpcode::FileOpenAppendDynamic: {
        size_t fixupIndex =
            emitter.emitFileOpenDynamicPlaceholder(stringTableOffsetDelta, O_WRONLY | O_CREAT | O_APPEND, 0644);
        stringTableFixups.push_back(fixupIndex);
        break;
      }
      case IrOpcode::FileReadByte:
        emitter.emitFileReadByte(static_cast<uint32_t>(inst.imm), layout.scratchOffset);
        break;
      case IrOpcode::FileClose:
        emitter.emitFileClose();
        break;
      case IrOpcode::FileFlush:
        emitter.emitFileFlush();
        break;
      case IrOpcode::FileWriteI32:
        if constexpr (!kIsArm64) {
          emitter.emitSignExtendTop32();
        }
        emitter.emitFileWriteI32(layout.scratchOffset, layout.scratchBytes);
        break;
      case IrOpcode::FileWriteI64:
        emitter.emitFileWriteI64(layout.scratchOffset, layout.scratchBytes);
        break;
      case IrOpcode::FileWriteU64:
        emitter.emitFileWriteU64(layout.scratchOffset, layout.scratchBytes);
        break;
      case IrOpcode::FileWriteString: {
        if (inst.imm >= module.stringTable.size()) {
          error = "native backend encountered invalid string index";
          return false;
        }
        size_t fixupIndex = emitter.emitFileWriteStringPlaceholder(
            module.stringTable[static_cast<size_t>(inst.imm)].size(), layout.scratchOffset);
        stringFixups.push_back({fixupIndex, static_cast<uint32_t>(inst.imm)});
        break;
      }
      case IrOpcode::FileWriteStringDynamic: {
        size_t fixupIndex = emitter.emitFileWriteStringDynamicPlaceholder(
            stringTableOffsetDelta, stringOffsetTableSize);
        stringTableFixups.push_back(fixupIndex);
        break;
      }
      case IrOpcode::FileWriteByte:
        emitter.emitFileWriteByte(layout.scratchOffset);
        break;
      case IrOpcode::FileWriteNewline:
        emitter.emitFileWriteNewline(layout.scratchOffset);
        break;
      case IrOpcode::PrintArgv: {
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        emitter.emitPrintArgv(layout.argcLocalIndex, layout.argvLocalIndex, layout.scratchOffset, newline, fd);
        break;
      }
      case IrOpcode::PrintArgvUnsafe: {
        uint64_t flags = decodePrintFlags(inst.imm);
        bool newline = (flags & PrintFlagNewline) != 0;
        uint64_t fd = (flags & PrintFlagStderr) ? 2 : 1;
        emitter.emitPrintArgv(layout.argcLocalIndex, layout.argvLocalIndex, layout.scratchOffset, newline, fd);
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
      case IrOpcode::LoadStringLength: {
        size_t fixupIndex = emitter.emitLoadStringLengthPlaceholder(stringOffsetTableSize);
        stringTableFixups.push_back(fixupIndex);
        break;
      }
      default:
        error = "unsupported IR opcode for native backend";
        return false;
      }
      return true;
    };
    bool deferOperands = false;
    if constexpr (!kIsArm64) {
      deferOperands = emitter.operandDeferralEnabled();
    }
    if (useRegisterAllocation) {
      if constexpr (!kIsArm64) {
        X64Emitter::RegAllocHooks hooks;
        hooks.emitTemplate = emitTemplateInstruction;
        hooks.recordStringFixup = [&](size_t fixupIndex, uint32_t stringIndex) {
          stringFixups.push_back({fixupIndex, stringIndex});
        };
        hooks.argumentsInRegisters = registerArguments[functionIndex];
        hooks.functionIndex = static_cast<uint32_t>(functionIndex);
        hooks.stringCount = module.stringTable.size();
        hooks.stringLength = [&](uint64_t index) -> uint64_t {
          return index < module.stringTable.size() ? module.stringTable[index].size() : 0;
        };
        hooks.calleeTakesRegisterArguments = [&](uint64_t target) {
          return target < registerArguments.size() && registerArguments[target];
        };
        hooks.recordCallFixup = [&](size_t fixupIndex, uint64_t target) {
          if (target >= module.functions.size()) {
            error = "native backend detected invalid call target";
            return false;
          }
          callFixups.push_back({fixupIndex, static_cast<size_t>(target)});
          return true;
        };
        if (!emitter.emitRegisterAllocatedFunction(
                fn, regAllocPlan, spillBaseLocal, instOffsets[functionIndex], hooks, error)) {
          return false;
        }
      }
    }
    // Template expansion, unless the register allocator already emitted the body.
    const size_t templateInstructionCount = useRegisterAllocation ? 0 : fn.instructions.size();
    for (size_t index = 0; index < templateInstructionCount; ++index) {
      if (branchTargets[index]) {
        emitter.flushValueStackCachePublic();
        if constexpr (!kIsArm64) {
          if (loopHeaders[index]) {
            emitter.alignLoopHeader();
          }
        }
      }
      const auto &inst = fn.instructions[index];
      instOffsets[functionIndex][index] = emitter.currentWordIndex();
      if constexpr (!kIsArm64) {
        if (deferOperands) {
          // `if (compare) { local = local +/- constant }` is branchless: the guarded
          // statement is exactly LoadLocal, constant, AddI64/SubI64 and StoreLocal of a
          // register local, and nothing else enters or leaves it.
          if (index + 1 < fn.instructions.size() &&
              fn.instructions[index + 1].op == IrOpcode::JumpIfZero && !branchTargets[index + 1]) {
            const size_t armStart = index + 2;
            const size_t armEnd = static_cast<size_t>(fn.instructions[index + 1].imm);
            if (armEnd == armStart + 4 && armEnd <= fn.instructions.size() &&
                !branchTargets[armStart + 1] && !branchTargets[armStart + 2] &&
                !branchTargets[armStart + 3] &&
                fn.instructions[armStart].op == IrOpcode::LoadLocal &&
                (fn.instructions[armStart + 1].op == IrOpcode::PushI64 ||
                 fn.instructions[armStart + 1].op == IrOpcode::PushI32) &&
                (fn.instructions[armStart + 2].op == IrOpcode::AddI64 ||
                 fn.instructions[armStart + 2].op == IrOpcode::SubI64) &&
                fn.instructions[armStart + 3].op == IrOpcode::StoreLocal &&
                fn.instructions[armStart + 3].imm == fn.instructions[armStart].imm) {
              const IrInstruction &operand = fn.instructions[armStart + 1];
              const uint64_t constant = operand.op == IrOpcode::PushI32
                                            ? static_cast<uint64_t>(static_cast<int64_t>(
                                                  static_cast<int32_t>(operand.imm)))
                                            : operand.imm;
              if (emitter.tryEmitCompareConditionalAdd(
                      inst.op,
                      static_cast<uint32_t>(fn.instructions[armStart].imm),
                      fn.instructions[armStart + 2].op == IrOpcode::SubI64,
                      constant)) {
                for (size_t skipped = 1; skipped < armEnd - index; ++skipped) {
                  instOffsets[functionIndex][index + skipped] = emitter.currentWordIndex();
                }
                index = armEnd - 1;
                continue;
              }
            }
          }
          // A comparison feeding a branch becomes one compare-and-branch.
          if (index + 1 < fn.instructions.size() &&
              fn.instructions[index + 1].op == IrOpcode::JumpIfZero && !branchTargets[index + 1]) {
            size_t fixupIndex = 0;
            if (emitter.tryEmitCompareBranch(inst.op, fixupIndex)) {
              NativeEmitterBranchFixup fixup;
              fixup.codeIndex = fixupIndex;
              fixup.functionIndex = functionIndex;
              fixup.targetInst = static_cast<size_t>(fn.instructions[index + 1].imm);
              fixup.isConditional = true;
              branchFixups.push_back(fixup);
              ++index;
              instOffsets[functionIndex][index] = emitter.currentWordIndex();
              continue;
            }
          }
          // `local = local OP (local | constant)` on a register-resident local
          // becomes one instruction on that register.
          if (inst.op == IrOpcode::LoadLocal && index + 3 < fn.instructions.size() &&
              emitter.isLocalPromoted(static_cast<uint32_t>(inst.imm)) &&
              !branchTargets[index + 1] && !branchTargets[index + 2] && !branchTargets[index + 3]) {
            const IrInstruction &operand = fn.instructions[index + 1];
            const IrInstruction &arithmetic = fn.instructions[index + 2];
            // An i32 operation is followed by SextI32 before the store.
            const bool sext = fn.instructions[index + 3].op == IrOpcode::SextI32 &&
                              index + 4 < fn.instructions.size() && !branchTargets[index + 4];
            const IrInstruction &store = fn.instructions[index + (sext ? 4 : 3)];
            int kind = -1;
            if (arithmetic.op == IrOpcode::AddI32 || arithmetic.op == IrOpcode::AddI64) {
              kind = 0;
            } else if (arithmetic.op == IrOpcode::SubI32 || arithmetic.op == IrOpcode::SubI64) {
              kind = 1;
            } else if (arithmetic.op == IrOpcode::MulI32 || arithmetic.op == IrOpcode::MulI64) {
              kind = 2;
            }
            const bool operandIsConstant =
                operand.op == IrOpcode::PushI32 || operand.op == IrOpcode::PushI64;
            if (kind >= 0 && store.op == IrOpcode::StoreLocal && store.imm == inst.imm &&
                (operandIsConstant || operand.op == IrOpcode::LoadLocal)) {
              const uint64_t constant = operand.op == IrOpcode::PushI32
                                            ? static_cast<uint64_t>(static_cast<int64_t>(
                                                  static_cast<int32_t>(operand.imm)))
                                            : operand.imm;
              emitter.emitPromotedLocalUpdate(static_cast<uint32_t>(inst.imm),
                                              kind,
                                              operandIsConstant,
                                              constant,
                                              static_cast<uint32_t>(operand.imm),
                                              sext);
              const size_t skippedCount = sext ? 4 : 3;
              for (size_t skipped = 1; skipped <= skippedCount; ++skipped) {
                instOffsets[functionIndex][index + skipped] = emitter.currentWordIndex();
              }
              index += skippedCount;
              continue;
            }
          }
        }
      }
      // Instructions whose templates clobber the promoted or cache registers get
      // the promoted locals written to their frame slots (and the deferred
      // operands to the memory stack) first, and the locals reloaded afterwards.
      const bool bracketComplex =
          (hasPromotedLocals || deferOperands) && !opcodeKeepsPromotedRegisters(inst.op);
      if constexpr (!kIsArm64) {
        if (bracketComplex) {
          emitter.beginComplexOp();
        }
      }
      if (!emitTemplateInstruction(index)) {
        return false;
      }
      if constexpr (!kIsArm64) {
        if (bracketComplex) {
          emitter.endComplexOp();
        }
      }
    }
    if constexpr (!kIsArm64) {
      emitter.clearPromotedLocals();
    }

    instOffsets[functionIndex][fn.instructions.size()] = emitter.currentWordIndex();
    if (instrumentation != nullptr) {
      const auto countersAfter = emitter.instrumentationCounters();
      auto &functionInstrumentation = instrumentation->perFunction[functionIndex];
      functionInstrumentation.valueStackPushCount =
          countersAfter.valueStackPushCount - countersBefore.valueStackPushCount;
      functionInstrumentation.valueStackPopCount =
          countersAfter.valueStackPopCount - countersBefore.valueStackPopCount;
      functionInstrumentation.spillCount = countersAfter.spillCount - countersBefore.spillCount;
      functionInstrumentation.reloadCount = countersAfter.reloadCount - countersBefore.reloadCount;
    }
  }
  return true;
}

#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
template bool emitNativeFunctions<Arm64Emitter>(const IrModule &,
                                                size_t,
                                                const std::vector<NativeEmitterFunctionLayout> &,
                                                const std::vector<size_t> &,
                                                Arm64Emitter &,
                                                std::vector<NativeEmitterBranchFixup> &,
                                                std::vector<NativeEmitterCallFixup> &,
                                                std::vector<NativeEmitterStringFixup> &,
                                                std::vector<size_t> &,
                                                uint64_t,
                                                uint64_t,
                                                std::vector<size_t> &,
                                                std::vector<std::vector<size_t>> &,
                                                NativeEmitterInstrumentation *,
                                                std::string &);
#elif defined(__linux__) && defined(__x86_64__)
template bool emitNativeFunctions<X64Emitter>(const IrModule &,
                                              size_t,
                                              const std::vector<NativeEmitterFunctionLayout> &,
                                              const std::vector<size_t> &,
                                              X64Emitter &,
                                              std::vector<NativeEmitterBranchFixup> &,
                                              std::vector<NativeEmitterCallFixup> &,
                                              std::vector<NativeEmitterStringFixup> &,
                                              std::vector<size_t> &,
                                              uint64_t,
                                              uint64_t,
                                              std::vector<size_t> &,
                                              std::vector<std::vector<size_t>> &,
                                              NativeEmitterInstrumentation *,
                                              std::string &);
#endif

} // namespace primec::native_emitter
