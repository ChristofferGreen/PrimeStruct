#include "primec/NativeEmitter.h"
#include "NativeEmitterEmitInternal.h"
#include "NativeEmitterInternals.h"

#include <algorithm>
#include <fcntl.h>
#include <sstream>

namespace primec {
using namespace native_emitter;

std::string formatNativeEmitterDebugDump(
    const NativeEmitterInstrumentation &instrumentation,
    const NativeEmitterOptimizationInstrumentation &optimization) {
  std::vector<NativeEmitterFunctionInstrumentation> orderedFunctions = instrumentation.perFunction;
  std::stable_sort(orderedFunctions.begin(), orderedFunctions.end(),
                   [](const NativeEmitterFunctionInstrumentation &lhs,
                      const NativeEmitterFunctionInstrumentation &rhs) {
                     if (lhs.functionIndex != rhs.functionIndex) {
                       return lhs.functionIndex < rhs.functionIndex;
                     }
                     return lhs.functionName < rhs.functionName;
                   });

  std::ostringstream out;
  out << "native_emitter_debug_v1\n";
  out << "[instrumentation]\n";
  out << "total_instruction_count=" << instrumentation.totalInstructionCount << '\n';
  out << "total_value_stack_push_count=" << instrumentation.totalValueStackPushCount << '\n';
  out << "total_value_stack_pop_count=" << instrumentation.totalValueStackPopCount << '\n';
  out << "total_spill_count=" << instrumentation.totalSpillCount << '\n';
  out << "total_reload_count=" << instrumentation.totalReloadCount << '\n';
  out << "function_count=" << orderedFunctions.size() << '\n';
  for (size_t i = 0; i < orderedFunctions.size(); ++i) {
    const auto &functionStats = orderedFunctions[i];
    out << "function[" << i << "].index=" << functionStats.functionIndex << '\n';
    out << "function[" << i << "].name=" << functionStats.functionName << '\n';
    out << "function[" << i << "].instruction_total=" << functionStats.instructionTotal << '\n';
    out << "function[" << i << "].value_stack_push_count=" << functionStats.valueStackPushCount << '\n';
    out << "function[" << i << "].value_stack_pop_count=" << functionStats.valueStackPopCount << '\n';
    out << "function[" << i << "].spill_count=" << functionStats.spillCount << '\n';
    out << "function[" << i << "].reload_count=" << functionStats.reloadCount << '\n';
  }
  out << "[optimization]\n";
  out << "applied=" << (optimization.applied ? 1 : 0) << '\n';
  out << "instruction_total_before=" << optimization.instructionTotalBefore << '\n';
  out << "instruction_total_after=" << optimization.instructionTotalAfter << '\n';
  out << "value_stack_push_count_before=" << optimization.valueStackPushCountBefore << '\n';
  out << "value_stack_push_count_after=" << optimization.valueStackPushCountAfter << '\n';
  out << "value_stack_pop_count_before=" << optimization.valueStackPopCountBefore << '\n';
  out << "value_stack_pop_count_after=" << optimization.valueStackPopCountAfter << '\n';
  out << "spill_count_before=" << optimization.spillCountBefore << '\n';
  out << "spill_count_after=" << optimization.spillCountAfter << '\n';
  out << "reload_count_before=" << optimization.reloadCountBefore << '\n';
  out << "reload_count_after=" << optimization.reloadCountAfter << '\n';
  return out.str();
}

bool NativeEmitter::emitExecutable(const IrModule &module, const std::string &outputPath, std::string &error) const {
  return emitExecutable(module, outputPath, error, nullptr, NativeEmitterOptions{});
}

bool NativeEmitter::emitExecutable(const IrModule &module,
                                   const std::string &outputPath,
                                   std::string &error,
                                   NativeEmitterInstrumentation *instrumentation) const {
  return emitExecutable(module, outputPath, error, instrumentation, NativeEmitterOptions{});
}

bool NativeEmitter::emitExecutable(const IrModule &module,
                                   const std::string &outputPath,
                                   std::string &error,
                                   NativeEmitterInstrumentation *instrumentation,
                                   const NativeEmitterOptions &options) const {
  if (instrumentation != nullptr) {
    *instrumentation = NativeEmitterInstrumentation{};
  }
#if !defined(__APPLE__)
  (void)module;
  (void)outputPath;
  (void)options;
  error = "native backend is only supported on macOS";
  return false;
#else
#if !defined(__aarch64__) && !defined(__arm64__)
  (void)module;
  (void)outputPath;
  (void)options;
  error = "native backend requires arm64";
  return false;
#else
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }

  const size_t entryIndex = static_cast<size_t>(module.entryIndex);
  std::vector<NativeEmitterFunctionLayout> layouts(module.functions.size());
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const IrFunction &fn = module.functions[functionIndex];
    NativeEmitterFunctionLayout &layout = layouts[functionIndex];
    for (const auto &inst : fn.instructions) {
      if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal ||
          inst.op == IrOpcode::AddressOfLocal) {
        layout.localCount = std::max(layout.localCount, static_cast<size_t>(inst.imm) + 1);
      }
      if (inst.op == IrOpcode::PrintI32 || inst.op == IrOpcode::PrintI64 || inst.op == IrOpcode::PrintU64 ||
          inst.op == IrOpcode::PrintString || inst.op == IrOpcode::PrintStringDynamic ||
          inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe ||
          inst.op == IrOpcode::FileWriteI32 || inst.op == IrOpcode::FileWriteI64 ||
          inst.op == IrOpcode::FileWriteU64 || inst.op == IrOpcode::FileWriteStringDynamic ||
          inst.op == IrOpcode::FileWriteByte ||
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
  emitter.setValueStackCacheEnabled(options.enableRegisterCache);
  std::vector<NativeEmitterBranchFixup> branchFixups;
  std::vector<NativeEmitterCallFixup> callFixups;
  std::vector<NativeEmitterStringFixup> stringFixups;
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
      functionInstrumentation.functionIndex = functionIndex;
      functionInstrumentation.functionName = module.functions[functionIndex].name;
      functionInstrumentation.instructionTotal = module.functions[functionIndex].instructions.size();
    }
  }

  if (!emitNativeFunctions(module,
                           entryIndex,
                           layouts,
                           emitOrder,
                           emitter,
                           branchFixups,
                           callFixups,
                           stringFixups,
                           stringTableFixups,
                           stringTableOffsetDelta,
                           stringOffsetTableSize,
                           functionOffsets,
                           instOffsets,
                           instrumentation,
                           error)) {
    return false;
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
