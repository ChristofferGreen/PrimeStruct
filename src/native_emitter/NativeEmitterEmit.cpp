#include "primec/NativeEmitter.h"
#include "NativeEmitterInternals.h"

#include <fcntl.h>

namespace primec {
using namespace native_emitter;

bool NativeEmitter::emitExecutable(const IrModule &module, const std::string &outputPath, std::string &error) const {
  return emitExecutable(module, outputPath, error, nullptr);
}

bool NativeEmitter::emitExecutable(const IrModule &module,
                                   const std::string &outputPath,
                                   std::string &error,
                                   NativeEmitterInstrumentation *instrumentation) const {
  if (instrumentation != nullptr) {
    *instrumentation = NativeEmitterInstrumentation{};
  }
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

  constexpr uint64_t ValueStackBytes = 1024ull * 1024ull;
  const size_t entryIndex = static_cast<size_t>(module.entryIndex);

  struct FunctionLayout {
    size_t localCount = 0;
    bool needsPrintScratch = false;
    bool needsArgc = false;
    bool needsArgv = false;
    uint32_t argcLocalIndex = 0;
    uint32_t argvLocalIndex = 0;
    uint32_t framePointerLocalIndex = 0;
    uint32_t linkLocalIndex = 0;
    uint32_t scratchSlots = 0;
    uint32_t scratchBytes = 0;
    uint32_t scratchOffset = 0;
    uint64_t localsSize = 0;
    uint64_t frameSize = 0;
  };
  std::vector<FunctionLayout> layouts(module.functions.size());
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const IrFunction &fn = module.functions[functionIndex];
    FunctionLayout &layout = layouts[functionIndex];
    for (const auto &inst : fn.instructions) {
      if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal ||
          inst.op == IrOpcode::AddressOfLocal) {
        layout.localCount = std::max(layout.localCount, static_cast<size_t>(inst.imm) + 1);
      }
      if (inst.op == IrOpcode::PrintI32 || inst.op == IrOpcode::PrintI64 || inst.op == IrOpcode::PrintU64 ||
          inst.op == IrOpcode::PrintString || inst.op == IrOpcode::PrintStringDynamic ||
          inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe ||
          inst.op == IrOpcode::FileWriteI32 || inst.op == IrOpcode::FileWriteI64 ||
          inst.op == IrOpcode::FileWriteU64 || inst.op == IrOpcode::FileWriteByte ||
          inst.op == IrOpcode::FileWriteNewline) {
        layout.needsPrintScratch = true;
      }
      if (inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe) {
        layout.needsArgc = true;
        layout.needsArgv = true;
      }
      if (inst.op == IrOpcode::PushArgc) {
        layout.needsArgc = true;
      }
    }
    if (functionIndex == entryIndex) {
      int64_t maxStack = 0;
      if (!computeMaxStackDepth(fn, maxStack, error)) {
        return false;
      }
    }
    if (layout.needsArgc) {
      layout.argcLocalIndex = static_cast<uint32_t>(layout.localCount);
      layout.localCount += 1;
    }
    if (layout.needsArgv) {
      layout.argvLocalIndex = static_cast<uint32_t>(layout.localCount);
      layout.localCount += 1;
    }
    layout.framePointerLocalIndex = static_cast<uint32_t>(layout.localCount);
    layout.localCount += 1;
    layout.linkLocalIndex = static_cast<uint32_t>(layout.localCount);
    layout.localCount += 1;
    layout.scratchSlots = layout.needsPrintScratch ? PrintScratchSlots : 0;
    layout.scratchBytes = layout.scratchSlots * 16;
    layout.scratchOffset = static_cast<uint32_t>(layout.localCount) * 16;
    layout.localsSize = static_cast<uint64_t>(layout.localCount + layout.scratchSlots) * 16;
    layout.frameSize = alignTo(layout.localsSize, 16);
  }

  Arm64Emitter emitter;
  struct BranchFixup {
    size_t codeIndex = 0;
    size_t functionIndex = 0;
    size_t targetInst = 0;
    bool isConditional = false;
  };
  std::vector<BranchFixup> branchFixups;
  struct CallFixup {
    size_t codeIndex = 0;
    size_t targetFunctionIndex = 0;
  };
  std::vector<CallFixup> callFixups;
  struct StringFixup {
    size_t codeIndex = 0;
    uint32_t stringIndex = 0;
  };
  std::vector<StringFixup> stringFixups;
  std::vector<size_t> stringTableFixups;
  std::vector<uint64_t> stringOffsets;
  std::vector<uint8_t> stringData;
  if (!module.stringTable.empty()) {
    stringOffsets.reserve(module.stringTable.size());
    for (const auto &text : module.stringTable) {
      stringOffsets.push_back(static_cast<uint64_t>(stringData.size()));
      stringData.insert(stringData.end(), text.begin(), text.end());
      stringData.push_back(0);
    }
  }
  const uint64_t stringDataPadding = alignTo(static_cast<uint64_t>(stringData.size()), 8) -
                                     static_cast<uint64_t>(stringData.size());
  const uint64_t stringTableOffsetDelta = static_cast<uint64_t>(stringData.size()) + stringDataPadding;
  const uint64_t stringOffsetTableSize = static_cast<uint64_t>(module.stringTable.size()) * sizeof(uint64_t);
  std::vector<size_t> functionOffsets(module.functions.size(), 0);
  std::vector<std::vector<size_t>> instOffsets(module.functions.size());
  std::vector<size_t> emitOrder;
  emitOrder.reserve(module.functions.size());
  emitOrder.push_back(entryIndex);
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    if (functionIndex != entryIndex) {
      emitOrder.push_back(functionIndex);
    }
  }
  if (instrumentation != nullptr) {
    instrumentation->perFunction.resize(module.functions.size());
    for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
      auto &functionInstrumentation = instrumentation->perFunction[functionIndex];
      functionInstrumentation.functionName = module.functions[functionIndex].name;
      functionInstrumentation.instructionTotal = module.functions[functionIndex].instructions.size();
    }
  }

  for (size_t functionIndex : emitOrder) {
    const Arm64InstrumentationCounters countersBefore = emitter.instrumentationCounters();
    const IrFunction &fn = module.functions[functionIndex];
    const FunctionLayout &layout = layouts[functionIndex];
    const bool isEntryFunction = functionIndex == entryIndex;
    uint64_t frameSize = layout.frameSize;
    if (isEntryFunction) {
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

    for (size_t index = 0; index < fn.instructions.size(); ++index) {
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
        BranchFixup fixup;
        fixup.codeIndex = emitter.emitJumpIfZeroPlaceholder();
        fixup.functionIndex = functionIndex;
        fixup.targetInst = static_cast<size_t>(inst.imm);
        fixup.isConditional = true;
        branchFixups.push_back(fixup);
        break;
      }
      case IrOpcode::Jump: {
        BranchFixup fixup;
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
  if (instrumentation != nullptr) {
    for (const auto &functionInstrumentation : instrumentation->perFunction) {
      instrumentation->totalInstructionCount += functionInstrumentation.instructionTotal;
      instrumentation->totalValueStackPushCount += functionInstrumentation.valueStackPushCount;
      instrumentation->totalValueStackPopCount += functionInstrumentation.valueStackPopCount;
      instrumentation->totalSpillCount += functionInstrumentation.spillCount;
      instrumentation->totalReloadCount += functionInstrumentation.reloadCount;
    }
  }

  constexpr int64_t kImm26Min = -(1LL << 25);
  constexpr int64_t kImm26Max = (1LL << 25) - 1;
  for (const auto &fixup : branchFixups) {
    const IrFunction &fn = module.functions[fixup.functionIndex];
    if (fixup.targetInst > fn.instructions.size()) {
      error = "native backend detected invalid jump target";
      return false;
    }
    int64_t targetOffset = static_cast<int64_t>(instOffsets[fixup.functionIndex][fixup.targetInst]);
    int64_t branchOffset = static_cast<int64_t>(fixup.codeIndex);
    int64_t delta = targetOffset - branchOffset;
    if (fixup.isConditional) {
      if (delta < kImm26Min || delta > kImm26Max) {
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
  for (const auto &fixup : callFixups) {
    if (fixup.targetFunctionIndex >= module.functions.size()) {
      error = "native backend detected invalid call target";
      return false;
    }
    int64_t targetOffset = static_cast<int64_t>(functionOffsets[fixup.targetFunctionIndex]);
    int64_t branchOffset = static_cast<int64_t>(fixup.codeIndex);
    int64_t delta = targetOffset - branchOffset;
    if (delta < kImm26Min || delta > kImm26Max) {
      error = "native backend call offset out of range";
      return false;
    }
    emitter.patchCall(fixup.codeIndex, static_cast<int32_t>(delta));
  }

  uint32_t codeBaseOffset = 0;
#if defined(__APPLE__)
  codeBaseOffset = computeMachOCodeOffset();
  emitter.setCodeBaseOffset(codeBaseOffset);
#endif

  if (!stringFixups.empty() || !stringTableFixups.empty()) {
    uint64_t codeSizeBytes = static_cast<uint64_t>(emitter.currentWordIndex()) * 4;
    uint64_t stringBaseOffset = codeSizeBytes;
    uint64_t stringTableOffset = stringBaseOffset + stringTableOffsetDelta;
    constexpr int64_t kAdrpMin = -(1LL << 20);
    constexpr int64_t kAdrpMax = (1LL << 20) - 1;
    for (const auto &fixup : stringFixups) {
      if (fixup.stringIndex >= module.stringTable.size()) {
        error = "native backend encountered invalid string fixup";
        return false;
      }
      int64_t targetOffset = static_cast<int64_t>(stringBaseOffset + stringOffsets[fixup.stringIndex]);
      int64_t instrOffset = static_cast<int64_t>(fixup.codeIndex) * 4;
      int64_t instrAddr = static_cast<int64_t>(codeBaseOffset) + instrOffset;
      int64_t targetAddr = static_cast<int64_t>(codeBaseOffset) + targetOffset;
      int64_t delta = targetOffset - instrOffset;
      int64_t instrPage = instrAddr & ~0xFFFLL;
      int64_t targetPage = targetAddr & ~0xFFFLL;
      int64_t pageDelta = (targetPage - instrPage) >> 12;
      if (pageDelta < kAdrpMin || pageDelta > kAdrpMax) {
        error = "native backend string literal out of range";
        return false;
      }
      emitter.patchAdr(fixup.codeIndex, 1, static_cast<int32_t>(delta));
    }
    for (size_t fixupIndex : stringTableFixups) {
      int64_t targetOffset = static_cast<int64_t>(stringTableOffset);
      int64_t instrOffset = static_cast<int64_t>(fixupIndex) * 4;
      int64_t instrAddr = static_cast<int64_t>(codeBaseOffset) + instrOffset;
      int64_t targetAddr = static_cast<int64_t>(codeBaseOffset) + targetOffset;
      int64_t delta = targetOffset - instrOffset;
      int64_t instrPage = instrAddr & ~0xFFFLL;
      int64_t targetPage = targetAddr & ~0xFFFLL;
      int64_t pageDelta = (targetPage - instrPage) >> 12;
      if (pageDelta < kAdrpMin || pageDelta > kAdrpMax) {
        error = "native backend string table out of range";
        return false;
      }
      emitter.patchAdr(fixupIndex, 1, static_cast<int32_t>(delta));
    }
  }

  std::vector<uint8_t> code = emitter.finalize();
  if (!stringData.empty()) {
    code.insert(code.end(), stringData.begin(), stringData.end());
    if (stringDataPadding > 0) {
      code.insert(code.end(), stringDataPadding, 0);
    }
    if (!module.stringTable.empty()) {
      for (uint64_t offset : stringOffsets) {
        for (size_t i = 0; i < sizeof(uint64_t); ++i) {
          code.push_back(static_cast<uint8_t>((offset >> (i * 8)) & 0xffu));
        }
      }
      for (const auto &text : module.stringTable) {
        uint64_t length = static_cast<uint64_t>(text.size());
        for (size_t i = 0; i < sizeof(uint64_t); ++i) {
          code.push_back(static_cast<uint8_t>((length >> (i * 8)) & 0xffu));
        }
      }
    }
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
