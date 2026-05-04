#include "NativeEmitterEmitInternal.h"

#include <fcntl.h>

namespace primec::native_emitter {

bool emitNativeFunctions(const IrModule &module,
                         size_t entryIndex,
                         const std::vector<NativeEmitterFunctionLayout> &layouts,
                         const std::vector<size_t> &emitOrder,
                         Arm64Emitter &emitter,
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
  for (size_t functionIndex : emitOrder) {
    const Arm64InstrumentationCounters countersBefore = emitter.instrumentationCounters();
    const IrFunction &fn = module.functions[functionIndex];
    const NativeEmitterFunctionLayout &layout = layouts[functionIndex];
    const bool isEntryFunction = functionIndex == entryIndex;
    uint64_t frameSize = layout.frameSize;
    if (isEntryFunction) {
      constexpr uint64_t ValueStackBytes = 1024ull * 1024ull;
      frameSize = alignTo(layout.localsSize + ValueStackBytes, 16);
    }
    functionOffsets[functionIndex] = emitter.currentWordIndex();
    emitter.emitMovRegPublic(21, 27);
    if (!emitter.beginFunction(frameSize, isEntryFunction, error)) {
      return false;
    }
    if (isEntryFunction) {
      emitter.emitCaptureEntryArgs();
    }
    if (layout.needsArgc) {
      emitter.emitStoreLocalFromReg(layout.argcLocalIndex, 19);
    }
    if (layout.needsArgv) {
      emitter.emitStoreLocalFromReg(layout.argvLocalIndex, 20);
    }
    emitter.emitStoreLocalFromReg(layout.framePointerLocalIndex, 21);
    emitter.emitStoreLocalFromReg(layout.linkLocalIndex, 30);
    instOffsets[functionIndex].assign(fn.instructions.size() + 1, 0);
    std::vector<bool> branchTargets(fn.instructions.size() + 1, false);
    for (const auto &inst : fn.instructions) {
      if ((inst.op == IrOpcode::Jump || inst.op == IrOpcode::JumpIfZero) &&
          inst.imm <= fn.instructions.size()) {
        branchTargets[static_cast<size_t>(inst.imm)] = true;
      }
    }

    for (size_t index = 0; index < fn.instructions.size(); ++index) {
      if (branchTargets[index]) {
        emitter.flushValueStackCachePublic();
      }
      const auto &inst = fn.instructions[index];
      instOffsets[functionIndex][index] = emitter.currentWordIndex();
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
        callFixups.push_back({emitter.emitCallPlaceholder(), static_cast<size_t>(inst.imm)});
        emitter.emitPushReg0();
        break;
      }
      case IrOpcode::CallVoid: {
        if (inst.imm >= module.functions.size()) {
          error = "native backend detected invalid call target";
          return false;
        }
        callFixups.push_back({emitter.emitCallPlaceholder(), static_cast<size_t>(inst.imm)});
        break;
      }
      case IrOpcode::ReturnVoid:
        emitter.emitReturnVoidWithFrameAndLink(layout.framePointerLocalIndex, layout.linkLocalIndex);
        break;
      case IrOpcode::ReturnI32:
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
    }

    instOffsets[functionIndex][fn.instructions.size()] = emitter.currentWordIndex();
    if (instrumentation != nullptr) {
      const Arm64InstrumentationCounters countersAfter = emitter.instrumentationCounters();
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

} // namespace primec::native_emitter
