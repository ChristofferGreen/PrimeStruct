#include "primec/NativeEmitter.h"
#include "NativeEmitterInternals.h"

#include <fcntl.h>

namespace primec {
using namespace native_emitter;
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
        inst.op == IrOpcode::PrintString || inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe ||
        inst.op == IrOpcode::FileWriteI32 || inst.op == IrOpcode::FileWriteI64 || inst.op == IrOpcode::FileWriteU64 ||
        inst.op == IrOpcode::FileWriteByte || inst.op == IrOpcode::FileWriteNewline) {
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
      stringData.push_back(0);
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
      case IrOpcode::PushF32:
        emitter.emitPushF32(static_cast<uint32_t>(inst.imm));
        break;
      case IrOpcode::PushF64:
        emitter.emitPushF64(inst.imm);
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
      case IrOpcode::ReturnF32:
        emitter.emitReturn();
        break;
      case IrOpcode::ReturnF64:
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
        emitter.emitFileWriteI32(scratchOffset, scratchBytes);
        break;
      case IrOpcode::FileWriteI64:
        emitter.emitFileWriteI64(scratchOffset, scratchBytes);
        break;
      case IrOpcode::FileWriteU64:
        emitter.emitFileWriteU64(scratchOffset, scratchBytes);
        break;
      case IrOpcode::FileWriteString: {
        if (inst.imm >= module.stringTable.size()) {
          error = "native backend encountered invalid string index";
          return false;
        }
        size_t fixupIndex = emitter.emitFileWriteStringPlaceholder(
            module.stringTable[static_cast<size_t>(inst.imm)].size(), scratchOffset);
        stringFixups.push_back({fixupIndex, static_cast<uint32_t>(inst.imm)});
        break;
      }
      case IrOpcode::FileWriteByte:
        emitter.emitFileWriteByte(scratchOffset);
        break;
      case IrOpcode::FileWriteNewline:
        emitter.emitFileWriteNewline(scratchOffset);
        break;
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
