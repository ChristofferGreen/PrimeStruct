#include "WasmEmitterInternal.h"

#include <limits>
#include <utility>

namespace primec {

namespace {

constexpr uint8_t WasmOpEnd = 0x0b;

WasmLocalLayout computeLocalLayout(const IrFunction &function,
                                   const std::vector<uint8_t> &irLocalTypes,
                                   std::string &error) {
  WasmLocalLayout layout;
  uint64_t maxLocalIndex = 0;
  bool hasLocal = false;
  for (const IrInstruction &inst : function.instructions) {
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal || inst.op == IrOpcode::FileReadByte) {
      maxLocalIndex = std::max(maxLocalIndex, inst.imm);
      hasLocal = true;
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
  layout.parameterCount = function.parameterCount;
  // The parameters and IR locals take the inferred types; the helper locals below are i32 except the i64 ones.
  layout.localTypes.assign(static_cast<size_t>(totalCount), WasmTypeI32);
  for (size_t i = 0; i < baseCount; ++i) {
    layout.localTypes[i] = i < irLocalTypes.size() ? irLocalTypes[i] : WasmTypeUnknown;
  }
  if (layout.hasFileNumericHelpers) {
    layout.fileDigitsValueLocal = static_cast<uint32_t>(totalCount);
    layout.fileDigitsRemLocal = static_cast<uint32_t>(totalCount + 1);
    totalCount += 2;
    layout.localTypes.push_back(WasmTypeI64);
    layout.localTypes.push_back(WasmTypeI64);
  }
  if (totalCount > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
    error = "wasm emitter local count exceeds 32-bit limit in function: " + function.name;
    return WasmLocalLayout{};
  }
  layout.totalLocals = static_cast<uint32_t>(totalCount);
  return layout;
}

// The local declarations of a function body: runs of equal types, for every local after the parameters (the signature
// declares those) and the scratch locals the emitter allocated while it emitted.
void appendLocalDecls(const WasmLocalLayout &layout, std::vector<uint8_t> &out) {
  std::vector<uint8_t> types(layout.localTypes.begin() + layout.parameterCount,
                             layout.localTypes.end());
  for (const WasmScratchSlot &slot : layout.typeState.scratch) {
    types.push_back(slot.type);
  }
  std::vector<std::pair<uint32_t, uint8_t>> runs;
  for (uint8_t type : types) {
    if (type == WasmTypeUnknown) {
      type = WasmTypeI32;
    }
    if (!runs.empty() && runs.back().second == type) {
      runs.back().first += 1;
    } else {
      runs.emplace_back(1u, type);
    }
  }
  appendU32Leb(static_cast<uint32_t>(runs.size()), out);
  for (const auto &run : runs) {
    appendU32Leb(run.first, out);
    out.push_back(run.second);
  }
}

} // namespace

bool lowerFunctionCode(const IrFunction &function,
                       const std::vector<WasmFunctionType> &functionTypes,
                       const WasmRuntimeContext &runtime,
                       const std::vector<uint8_t> &irLocalTypes,
                       WasmCodeBody &outBody,
                       std::string &error) {
  WasmLocalLayout localLayout = computeLocalLayout(function, irLocalTypes, error);
  if (!error.empty()) {
    return false;
  }

  outBody.localDecls.clear();
  outBody.instructions.clear();

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
  // The scratch locals are known only after the body is emitted.
  appendLocalDecls(localLayout, outBody.localDecls);
  return true;
}

bool inferWasmLocalTypes(const IrModule &module,
                         std::vector<WasmFunctionType> &functionTypes,
                         const WasmRuntimeContext &runtime,
                         std::vector<std::vector<uint8_t>> &irLocalTypes,
                         std::string &error) {
  const size_t functionCount = module.functions.size();
  irLocalTypes.assign(functionCount, {});
  std::vector<std::vector<uint8_t>> observed(functionCount);
  for (size_t f = 0; f < functionCount; ++f) {
    observed[f].assign(module.functions[f].parameterCount, WasmTypeUnknown);
    functionTypes[f].params.assign(module.functions[f].parameterCount, WasmTypeUnknown);
  }
  // Types only widen (unknown, i32, i64), so the dry runs reach a fixed point; the bound keeps a defect from looping.
  constexpr size_t MaxRounds = 64;
  for (size_t round = 0; round < MaxRounds; ++round) {
    bool changed = false;
    for (size_t f = 0; f < functionCount; ++f) {
      const IrFunction &function = module.functions[f];
      WasmLocalLayout layout = computeLocalLayout(function, irLocalTypes[f], error);
      if (!error.empty()) {
        return false;
      }
      // The parameters hold the widest type the calls pass.
      for (size_t k = 0; k < function.parameterCount && k < layout.localTypes.size(); ++k) {
        uint8_t merged = WasmTypeUnknown;
        if (!mergeWasmTypes(layout.localTypes[k], observed[f][k], merged)) {
          error =
              "wasm emitter call passes values of different kinds in function: " + function.name;
          return false;
        }
        layout.localTypes[k] = merged;
      }
      layout.typeState.inferring = true;
      layout.typeState.observedParams = &observed;
      std::vector<uint8_t> discarded;
      const size_t start = function.parameterCount;
      if (function.instructions.size() < start) {
        error = "wasm emitter: function " + function.name +
                " has too few instructions for its parameters";
        return false;
      }
      if (!emitInstructionRange(function,
                                start,
                                function.instructions.size(),
                                layout,
                                functionTypes,
                                runtime,
                                discarded,
                                error)) {
        return false;
      }
      std::vector<uint8_t> next(layout.localTypes.begin(),
                                layout.localTypes.begin() + layout.irLocalCount);
      if (next != irLocalTypes[f]) {
        irLocalTypes[f] = std::move(next);
        changed = true;
      }
      for (size_t k = 0; k < function.parameterCount && k < observed[f].size(); ++k) {
        if (observed[f][k] != layout.localTypes[k]) {
          observed[f][k] = layout.localTypes[k];
          changed = true;
        }
      }
      changed = changed || layout.typeState.changed;
    }
    if (!changed) {
      for (size_t f = 0; f < functionCount; ++f) {
        for (uint8_t &type : irLocalTypes[f]) {
          if (type == WasmTypeUnknown) {
            type = WasmTypeI32;
          }
        }
        for (size_t k = 0; k < module.functions[f].parameterCount; ++k) {
          functionTypes[f].params[k] =
              k < irLocalTypes[f].size() ? irLocalTypes[f][k] : WasmTypeI32;
        }
      }
      return true;
    }
  }
  error = "wasm emitter could not infer local types: the type inference did not converge";
  return false;
}

} // namespace primec
