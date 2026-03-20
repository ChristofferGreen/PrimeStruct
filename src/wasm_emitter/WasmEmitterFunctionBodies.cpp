#include "WasmEmitterInternal.h"

#include <limits>

namespace primec {

namespace {

constexpr uint8_t WasmValueTypeI32 = 0x7f;
constexpr uint8_t WasmValueTypeI64 = 0x7e;
constexpr uint8_t WasmOpEnd = 0x0b;

WasmLocalLayout computeLocalLayout(const IrFunction &function, std::string &error) {
  WasmLocalLayout layout;
  uint64_t maxLocalIndex = 0;
  bool hasLocal = false;
  for (const IrInstruction &inst : function.instructions) {
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal || inst.op == IrOpcode::FileReadByte) {
      maxLocalIndex = std::max(maxLocalIndex, inst.imm);
      hasLocal = true;
    }
    if (inst.op == IrOpcode::Dup) {
      layout.needsDupTempLocal = true;
    }
    if (inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe) {
      layout.hasArgvHelpers = true;
    }
    if (inst.op == IrOpcode::PrintI32 || inst.op == IrOpcode::PrintI64 || inst.op == IrOpcode::PrintU64 ||
        inst.op == IrOpcode::PrintStringDynamic || inst.op == IrOpcode::FileOpenRead ||
        inst.op == IrOpcode::FileOpenWrite || inst.op == IrOpcode::FileOpenAppend ||
        inst.op == IrOpcode::FileOpenReadDynamic || inst.op == IrOpcode::FileOpenWriteDynamic ||
        inst.op == IrOpcode::FileOpenAppendDynamic ||
        inst.op == IrOpcode::FileReadByte || inst.op == IrOpcode::FileClose || inst.op == IrOpcode::FileFlush ||
        inst.op == IrOpcode::FileWriteI32 ||
        inst.op == IrOpcode::FileWriteI64 || inst.op == IrOpcode::FileWriteU64 ||
        inst.op == IrOpcode::FileWriteString || inst.op == IrOpcode::FileWriteStringDynamic ||
        inst.op == IrOpcode::FileWriteByte || inst.op == IrOpcode::FileWriteNewline) {
      layout.hasFileHelpers = true;
    }
    if (inst.op == IrOpcode::PrintI32 || inst.op == IrOpcode::PrintI64 || inst.op == IrOpcode::PrintU64 ||
        inst.op == IrOpcode::PrintStringDynamic || inst.op == IrOpcode::FileWriteI32 ||
        inst.op == IrOpcode::FileWriteI64 || inst.op == IrOpcode::FileWriteU64 ||
        inst.op == IrOpcode::FileWriteStringDynamic) {
      layout.hasFileNumericHelpers = true;
    }
  }
  const uint64_t baseCount = hasLocal ? (maxLocalIndex + 1) : 0;
  layout.irLocalCount = static_cast<uint32_t>(baseCount);
  uint64_t totalCount = baseCount;
  if (layout.needsDupTempLocal) {
    layout.dupTempIndex = static_cast<uint32_t>(totalCount);
    totalCount += 1;
  }
  if (layout.hasArgvHelpers) {
    layout.argvIndexLocal = static_cast<uint32_t>(totalCount);
    layout.argvCountLocal = static_cast<uint32_t>(totalCount + 1);
    layout.argvPtrLocal = static_cast<uint32_t>(totalCount + 2);
    layout.argvLenLocal = static_cast<uint32_t>(totalCount + 3);
    layout.argvNextPtrLocal = static_cast<uint32_t>(totalCount + 4);
    totalCount += 5;
  }
  if (layout.hasFileHelpers) {
    layout.fileHandleLocal = static_cast<uint32_t>(totalCount);
    layout.fileValueLocal = static_cast<uint32_t>(totalCount + 1);
    layout.fileErrLocal = static_cast<uint32_t>(totalCount + 2);
    totalCount += 3;
  }
  if (layout.hasFileNumericHelpers) {
    layout.fileDigitsPtrLocal = static_cast<uint32_t>(totalCount);
    layout.fileDigitsLenLocal = static_cast<uint32_t>(totalCount + 1);
    layout.fileDigitsNegLocal = static_cast<uint32_t>(totalCount + 2);
    totalCount += 3;
  }
  layout.i32LocalCount = static_cast<uint32_t>(totalCount);
  if (layout.hasFileNumericHelpers) {
    layout.fileDigitsValueLocal = static_cast<uint32_t>(totalCount);
    layout.fileDigitsRemLocal = static_cast<uint32_t>(totalCount + 1);
    totalCount += 2;
    layout.i64LocalCount = 2;
  }
  if (totalCount > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
    error = "wasm emitter local count exceeds 32-bit limit in function: " + function.name;
    return WasmLocalLayout{};
  }
  layout.totalLocals = static_cast<uint32_t>(totalCount);
  return layout;
}

void appendLocalDecls(const WasmLocalLayout &layout, std::vector<uint8_t> &out) {
  if (layout.totalLocals == 0) {
    appendU32Leb(0, out);
    return;
  }
  uint32_t groupCount = 0;
  if (layout.i32LocalCount > 0) {
    groupCount += 1;
  }
  if (layout.i64LocalCount > 0) {
    groupCount += 1;
  }
  appendU32Leb(groupCount, out);
  if (layout.i32LocalCount > 0) {
    appendU32Leb(layout.i32LocalCount, out);
    out.push_back(WasmValueTypeI32);
  }
  if (layout.i64LocalCount > 0) {
    appendU32Leb(layout.i64LocalCount, out);
    out.push_back(WasmValueTypeI64);
  }
}

} // namespace

bool lowerFunctionCode(const IrFunction &function,
                       const std::vector<WasmFunctionType> &functionTypes,
                       const WasmRuntimeContext &runtime,
                       WasmCodeBody &outBody,
                       std::string &error) {
  const WasmLocalLayout localLayout = computeLocalLayout(function, error);
  if (!error.empty()) {
    return false;
  }

  outBody.localDecls.clear();
  outBody.instructions.clear();
  appendLocalDecls(localLayout, outBody.localDecls);

  if (!emitInstructionRange(
          function,
          0,
          function.instructions.size(),
          localLayout,
          functionTypes,
          runtime,
          outBody.instructions,
          error)) {
    return false;
  }
  outBody.instructions.push_back(WasmOpEnd);
  return true;
}

} // namespace primec
