#include "primec/ir/IrValidation.h"

#include "primec/ir/IrOpcodeTable.h"

#include <cstdint>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace primec {
namespace {

constexpr uint8_t MinOpcode = IrOpcodeMin;
constexpr uint8_t MaxOpcode = IrOpcodeMax;
constexpr uint64_t MaxGlslLocalIndex = 1023;
constexpr uint32_t MaxCallParameterCount = 4096;
constexpr uint64_t KnownEffectMask = EffectIoOut | EffectIoErr | EffectHeapAlloc | EffectPathSpaceNotify |
                                     EffectPathSpaceInsert | EffectPathSpaceTake | EffectFileWrite |
                                     EffectGpuDispatch | EffectPathSpaceBind | EffectPathSpaceSchedule |
                                     EffectFileRead | EffectTask;
constexpr uint64_t KnownWasmWasiEffectMask =
    EffectIoOut | EffectIoErr | EffectHeapAlloc | EffectFileWrite | EffectFileRead;
constexpr uint64_t KnownWasmBrowserEffectMask = 0ull;

bool isWasmTarget(IrValidationTarget target) {
  return target == IrValidationTarget::Wasm || target == IrValidationTarget::WasmBrowser;
}

bool isGlslTarget(IrValidationTarget target) {
  return target == IrValidationTarget::Glsl;
}

const char *wasmTargetName(IrValidationTarget target) {
  if (target == IrValidationTarget::WasmBrowser) {
    return "wasm-browser";
  }
  return "wasm";
}

// Both predicates read the per-target flags of the opcode table (TODO-5361).
bool isGlslOpcodeAllowed(IrOpcode op) {
  const IrOpcodeInfo *info = irOpcodeInfo(op);
  return info != nullptr && info->glsl;
}

bool isWasmOpcodeAllowedForTarget(IrOpcode op, IrValidationTarget target) {
  const IrOpcodeInfo *info = irOpcodeInfo(op);
  if (info == nullptr) {
    return false;
  }
  return target == IrValidationTarget::WasmBrowser ? info->wasmBrowser : info->wasm;
}

bool failFunction(size_t functionIndex,
                  const std::string &functionName,
                  const std::string &detail,
                  std::string &error) {
  std::ostringstream out;
  out << "invalid IR function";
  if (!functionName.empty()) {
    out << " " << functionName;
  } else {
    out << " #" << functionIndex;
  }
  out << ": " << detail;
  error = out.str();
  return false;
}

bool failInstruction(size_t functionIndex,
                     const std::string &functionName,
                     size_t instructionIndex,
                     const std::string &detail,
                     std::string &error) {
  std::ostringstream out;
  out << "invalid IR instruction " << instructionIndex << " in ";
  if (!functionName.empty()) {
    out << functionName;
  } else {
    out << "function #" << functionIndex;
  }
  out << ": " << detail;
  error = out.str();
  return false;
}

bool validatePrintFlags(size_t functionIndex,
                        const std::string &functionName,
                        size_t instructionIndex,
                        uint64_t imm,
                        std::string &error) {
  if ((imm & ~PrintFlagMask) != 0) {
    return failInstruction(functionIndex, functionName, instructionIndex, "invalid print flags", error);
  }
  return true;
}

bool validateStringIndex(size_t functionIndex,
                         const std::string &functionName,
                         size_t instructionIndex,
                         uint64_t stringIndex,
                         size_t stringCount,
                         IrValidationTarget target,
                         std::string &error) {
  if (stringIndex >= stringCount) {
    return failInstruction(functionIndex, functionName, instructionIndex, "invalid string index", error);
  }
  if (target == IrValidationTarget::Native &&
      stringIndex > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
    return failInstruction(functionIndex, functionName, instructionIndex, "native string index exceeds 32-bit limit", error);
  }
  return true;
}

bool validateFunction(const IrModule &module,
                      size_t functionIndex,
                      const IrFunction &function,
                      IrValidationTarget target,
                      std::string &error) {
  if (function.name.empty()) {
    return failFunction(functionIndex, function.name, "empty function name", error);
  }
  if (function.instructions.empty()) {
    return failFunction(functionIndex, function.name, "no instructions", error);
  }
  if (function.metadata.schedulingScope != IrSchedulingScope::Default) {
    return failFunction(functionIndex, function.name, "unsupported scheduling scope", error);
  }
  if ((function.metadata.instrumentationFlags & ~InstrumentationTailExecution) != 0u) {
    return failFunction(functionIndex, function.name, "unsupported instrumentation flags", error);
  }
  if ((function.metadata.effectMask & ~KnownEffectMask) != 0u) {
    return failFunction(functionIndex, function.name, "unsupported effect mask bits", error);
  }
  if ((function.metadata.capabilityMask & ~KnownEffectMask) != 0u) {
    return failFunction(functionIndex, function.name, "unsupported capability mask bits", error);
  }
  if (isWasmTarget(target)) {
    const uint64_t allowedMask =
        target == IrValidationTarget::WasmBrowser ? KnownWasmBrowserEffectMask : KnownWasmWasiEffectMask;
    if ((function.metadata.effectMask & ~allowedMask) != 0u) {
      return failFunction(functionIndex,
                          function.name,
                          "unsupported effect mask bits for " + std::string(wasmTargetName(target)) + " target",
                          error);
    }
    if ((function.metadata.capabilityMask & ~allowedMask) != 0u) {
      return failFunction(functionIndex,
                          function.name,
                          "unsupported capability mask bits for " + std::string(wasmTargetName(target)) + " target",
                          error);
    }
  }

  for (size_t instructionIndex = 0; instructionIndex < function.instructions.size(); ++instructionIndex) {
    const IrInstruction &inst = function.instructions[instructionIndex];
    const uint8_t opcodeValue = static_cast<uint8_t>(inst.op);
    if (opcodeValue < MinOpcode || opcodeValue > MaxOpcode) {
      return failInstruction(functionIndex, function.name, instructionIndex, "unsupported opcode", error);
    }
    if (inst.op == IrOpcode::CallHost) {
      if (target != IrValidationTarget::Vm && target != IrValidationTarget::Serialized) {
        return failInstruction(functionIndex,
                               function.name,
                               instructionIndex,
                               "host calls are only supported by the vm target and serialized bytecode",
                               error);
      }
      if (inst.imm >= module.hostImports.size()) {
        return failInstruction(functionIndex, function.name, instructionIndex, "invalid host import index", error);
      }
      if (module.hostImports[static_cast<size_t>(inst.imm)].parameters.size() > MaxCallParameterCount) {
        return failInstruction(
            functionIndex, function.name, instructionIndex, "host import parameter count exceeds supported limit", error);
      }
    }
    if (isWasmTarget(target) && !isWasmOpcodeAllowedForTarget(inst.op, target)) {
      return failInstruction(functionIndex,
                             function.name,
                             instructionIndex,
                             "unsupported opcode for " + std::string(wasmTargetName(target)) + " target",
                             error);
    }
    if (isGlslTarget(target) && !isGlslOpcodeAllowed(inst.op)) {
      return failInstruction(functionIndex, function.name, instructionIndex, "unsupported opcode for glsl target", error);
    }
    switch (inst.op) {
      case IrOpcode::PushI64: {
        const int64_t value = static_cast<int64_t>(inst.imm);
        if (target == IrValidationTarget::Glsl &&
            (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max())) {
          return failInstruction(functionIndex, function.name, instructionIndex, "glsl i64 literal out of i32 range", error);
        }
        break;
      }
      case IrOpcode::JumpIfZero:
      case IrOpcode::Jump:
        if (inst.imm > function.instructions.size()) {
          return failInstruction(functionIndex, function.name, instructionIndex, "invalid jump target", error);
        }
        break;
      case IrOpcode::Call:
      case IrOpcode::CallVoid:
        if (inst.imm >= module.functions.size()) {
          return failInstruction(functionIndex, function.name, instructionIndex, "invalid call target", error);
        }
        if (module.functions[static_cast<size_t>(inst.imm)].parameterCount > MaxCallParameterCount) {
          return failInstruction(
              functionIndex, function.name, instructionIndex, "call target parameter count exceeds supported limit", error);
        }
        break;
      case IrOpcode::LoadLocal:
      case IrOpcode::StoreLocal:
      case IrOpcode::AddressOfLocal:
      case IrOpcode::FileReadByte:
        if (inst.imm > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
          return failInstruction(functionIndex, function.name, instructionIndex, "local index exceeds 32-bit limit", error);
        }
        if (target == IrValidationTarget::Glsl && inst.imm > MaxGlslLocalIndex) {
          return failInstruction(functionIndex, function.name, instructionIndex, "local index exceeds glsl local-slot limit", error);
        }
        break;
      case IrOpcode::PrintI32:
      case IrOpcode::PrintI64:
      case IrOpcode::PrintU64:
      case IrOpcode::PrintStringDynamic:
      case IrOpcode::PrintArgv:
      case IrOpcode::PrintArgvUnsafe:
        if (!validatePrintFlags(functionIndex, function.name, instructionIndex, inst.imm, error)) {
          return false;
        }
        break;
      case IrOpcode::PrintString:
        if (!validateStringIndex(functionIndex,
                                 function.name,
                                 instructionIndex,
                                 decodePrintStringIndex(inst.imm),
                                 module.stringTable.size(),
                                 target,
                                 error)) {
          return false;
        }
        break;
      case IrOpcode::FileOpenRead:
      case IrOpcode::FileOpenWrite:
      case IrOpcode::FileOpenAppend:
      case IrOpcode::FileWriteString:
      case IrOpcode::LoadStringByte:
        if (!validateStringIndex(functionIndex,
                                 function.name,
                                 instructionIndex,
                                 inst.imm,
                                 module.stringTable.size(),
                                 target,
                                 error)) {
          return false;
        }
        break;
      case IrOpcode::LoadStringLength:
        break;
      default:
        break;
    }
  }
  return true;
}

// GLSL/shader targets fundamentally forbid recursive function calls (direct
// or mutual) - unlike the VM/native/C++/wasm backends, there is no call-stack
// mechanism to bound recursion depth. Detects a cycle in the Call/CallVoid
// graph iteratively (no native recursion, consistent with this codebase's
// stack-overflow-safety convention for compiler-internal graph walks).
bool findGlslRecursionCycle(const IrModule &module, std::string &error) {
  std::vector<std::vector<size_t>> calleesOf(module.functions.size());
  for (size_t caller = 0; caller < module.functions.size(); ++caller) {
    for (const IrInstruction &inst : module.functions[caller].instructions) {
      if ((inst.op == IrOpcode::Call || inst.op == IrOpcode::CallVoid) &&
          inst.imm < module.functions.size()) {
        calleesOf[caller].push_back(static_cast<size_t>(inst.imm));
      }
    }
  }

  enum class VisitState : uint8_t { Unvisited, OnStack, Done };
  std::vector<VisitState> state(module.functions.size(), VisitState::Unvisited);
  for (size_t start = 0; start < module.functions.size(); ++start) {
    if (state[start] != VisitState::Unvisited) {
      continue;
    }
    std::vector<std::pair<size_t, size_t>> frames;
    frames.push_back({start, 0});
    state[start] = VisitState::OnStack;
    while (!frames.empty()) {
      const size_t node = frames.back().first;
      size_t &nextEdge = frames.back().second;
      if (nextEdge < calleesOf[node].size()) {
        const size_t callee = calleesOf[node][nextEdge];
        ++nextEdge;
        if (state[callee] == VisitState::OnStack) {
          failFunction(node,
                       module.functions[node].name,
                       "glsl target does not support recursive function calls (calls " +
                           module.functions[callee].name + ")",
                       error);
          return true;
        }
        if (state[callee] == VisitState::Unvisited) {
          state[callee] = VisitState::OnStack;
          frames.push_back({callee, 0});
        }
      } else {
        state[node] = VisitState::Done;
        frames.pop_back();
      }
    }
  }
  return false;
}

} // namespace

bool validateIrModule(const IrModule &module, IrValidationTarget target, std::string &error) {
  error.clear();

  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  if (target == IrValidationTarget::Native &&
      module.stringTable.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = "native string table exceeds 32-bit limit";
    return false;
  }

  std::unordered_set<std::string> importNames;
  for (const IrHostImport &import : module.hostImports) {
    if (import.name.empty()) {
      error = "empty IR host import name";
      return false;
    }
    if (!importNames.insert(import.name).second) {
      error = "duplicate IR host import name: " + import.name;
      return false;
    }
  }

  std::unordered_set<std::string> functionNames;
  functionNames.reserve(module.functions.size());
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const IrFunction &function = module.functions[functionIndex];
    if (!functionNames.insert(function.name).second) {
      error = "duplicate IR function name: " + function.name;
      return false;
    }
    if (!validateFunction(module, functionIndex, function, target, error)) {
      return false;
    }
  }

  if (isGlslTarget(target) && findGlslRecursionCycle(module, error)) {
    return false;
  }

  return true;
}

} // namespace primec
