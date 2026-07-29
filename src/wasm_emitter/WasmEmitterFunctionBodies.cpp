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
  // wasm's function signature already provides locals 0..parameterCount-1
  // (see inferFunctionType in WasmEmitterModule.cpp) - the real-call
  // prologue that references them is skipped during translation (see
  // lowerFunctionCode below) but still counted into maxLocalIndex above,
  // so totalCount here is always >= parameterCount whenever
  // parameterCount > 0. Declaring that same range again in this
  // function's own local-declarations section wouldn't miscompile
  // anything (StoreLocal/LoadLocal address locals by raw IR index either
  // way, and appended declared locals still land at the index that range
  // implies), but it's dead weight - shrink the *declared* count by the
  // range wasm already gives us for free. layout.irLocalCount above (used
  // as the LoadLocal/StoreLocal bounds check) intentionally still covers
  // the full range, since indices 0..parameterCount-1 remain valid IR
  // local references even though nothing needs to declare them again.
  if (totalCount < function.parameterCount) {
    error = "internal error: wasm emitter local count inconsistent with parameterCount in function: " +
            function.name;
    return WasmLocalLayout{};
  }
  layout.i32LocalCount = static_cast<uint32_t>(totalCount - function.parameterCount);
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

  // wasm's own `call` instruction auto-binds a callee's arguments as its
  // first N locals per the wasm spec, but the real-call body-lowering loop
  // (IrLowererLowerStatementsCallsStage.cpp) unconditionally emits an
  // explicit `StoreLocal (N-1) ... StoreLocal 0` prologue for every
  // real-call-eligible body, matching the VM/native/C++ backends'
  // caller-pushes/callee-pops operand-stack convention instead. Translating
  // that prologue as-is would try to `local.set` values wasm never pushed
  // (they're already bound), so skip exactly that leading pattern here.
  // Fail loudly rather than mis-skip if a function claims parameters but
  // its instructions don't start with the expected shape - this is the
  // only place that shape is generated, so a mismatch means either this
  // function's assumptions or the lowering pipeline's have drifted.
  size_t startIndex = 0;
  if (function.parameterCount > 0) {
    if (function.instructions.size() < function.parameterCount) {
      error = "wasm emitter: function " + function.name + " declares " +
              std::to_string(function.parameterCount) +
              " parameters but has too few instructions for the expected prologue";
      return false;
    }
    for (uint32_t i = 0; i < function.parameterCount; ++i) {
      const IrInstruction &inst = function.instructions[i];
      const uint64_t expectedLocal = function.parameterCount - 1 - i;
      if (inst.op != IrOpcode::StoreLocal || inst.imm != expectedLocal) {
        error = "wasm emitter: function " + function.name +
                " declares parameters but its leading instructions do not match the expected "
                "StoreLocal prologue";
        return false;
      }
    }
    startIndex = function.parameterCount;
  }

  if (!emitInstructionRange(
          function,
          startIndex,
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
