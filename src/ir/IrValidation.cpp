#include "primec/ir/IrValidation.h"

#include "primec/ir/IrCfg.h"
#include "primec/ir/IrNativeTextHost.h"
#include "primec/ir/IrOpcodeTable.h"

#include <algorithm>
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
constexpr uint64_t KnownEffectMask =
    EffectIoOut | EffectIoErr | EffectHeapAlloc | EffectPathSpaceNotify | EffectPathSpaceInsert |
    EffectPathSpaceTake | EffectFileWrite | EffectGpuDispatch | EffectPathSpaceBind |
    EffectPathSpaceSchedule | EffectFileRead | EffectTask | EffectNetIo | EffectAtomic |
    EffectLock | EffectChannel;
constexpr uint64_t KnownWasmWasiEffectMask =
    EffectIoOut | EffectIoErr | EffectHeapAlloc | EffectFileWrite | EffectFileRead;
constexpr uint64_t KnownWasmBrowserEffectMask = 0ull;
// Effects a shader can never have: GLSL has no sockets.
constexpr uint64_t GlslRejectedEffectMask = EffectNetIo;

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

// The atomic opcodes (docs/spec/vm-design.md, Atomics): the VM, serialized bytecode and the C++ targets (a module
// that uses them is emitted by the optimized emitter) run them.
bool isAtomicOpcode(IrOpcode op) {
  return op == IrOpcode::AtomicLoad || op == IrOpcode::AtomicStore || op == IrOpcode::AtomicAdd ||
         op == IrOpcode::AtomicExchange || op == IrOpcode::AtomicCompareExchange;
}

// The mutex opcodes (docs/spec/vm-design.md, Mutexes) run where the atomic ones do.
bool isMutexOpcode(IrOpcode op) {
  return op == IrOpcode::MutexLock || op == IrOpcode::MutexUnlock;
}

// The condition opcodes (docs/spec/vm-design.md, Conditions), which a `/std/sync` Channel waits with.
bool isConditionOpcode(IrOpcode op) {
  return op == IrOpcode::ConditionWait || op == IrOpcode::ConditionNotify;
}

// The backend a module with atomic, mutex or condition opcodes is rejected for, or nullptr for a target that runs them.
const char *atomicRejectingBackendName(IrValidationTarget target) {
  switch (target) {
  case IrValidationTarget::Native:
    return "native";
  case IrValidationTarget::Wasm:
    return "wasm";
  case IrValidationTarget::WasmBrowser:
    return "wasm-browser";
  case IrValidationTarget::Glsl:
    return "glsl";
  default:
    return nullptr;
  }
}

// The byte-memory opcodes and the u8 wrap step: the VM, the C++ emitters (a module that uses them is emitted by the
// optimized emitter) and the arm64 native emitter run them; the other backends reject them.
bool isByteOpcode(IrOpcode op) {
  return op == IrOpcode::HeapAllocBytes || op == IrOpcode::LoadU8 || op == IrOpcode::StoreU8 ||
         op == IrOpcode::ZextU8;
}

// Whether the native emitter of this build emits byte memory: only its arm64 emitter does.
constexpr bool nativeEmitsByteMemory() {
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
  return true;
#else
  return false;
#endif
}

// Both predicates read the per-target flags of the opcode table.
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
  if (isGlslTarget(target)) {
    if ((function.metadata.effectMask & GlslRejectedEffectMask) != 0u) {
      return failFunction(functionIndex, function.name, "unsupported effect mask bits for glsl target", error);
    }
    if ((function.metadata.capabilityMask & GlslRejectedEffectMask) != 0u) {
      return failFunction(functionIndex, function.name, "unsupported capability mask bits for glsl target", error);
    }
  }

  for (size_t instructionIndex = 0; instructionIndex < function.instructions.size(); ++instructionIndex) {
    const IrInstruction &inst = function.instructions[instructionIndex];
    const uint8_t opcodeValue = static_cast<uint8_t>(inst.op);
    if (opcodeValue < MinOpcode || opcodeValue > MaxOpcode) {
      return failInstruction(functionIndex, function.name, instructionIndex, "unsupported opcode", error);
    }
    if (inst.op == IrOpcode::Trap && target != IrValidationTarget::Vm &&
        target != IrValidationTarget::Serialized && target != IrValidationTarget::OptCpp &&
        target != IrValidationTarget::Cpp && target != IrValidationTarget::Native) {
      return failInstruction(functionIndex,
                             function.name,
                             instructionIndex,
                             "traps are only supported by the vm, native and C++ targets "
                             "and serialized bytecode",
                             error);
    }
    if ((inst.op == IrOpcode::TaskSpawn || inst.op == IrOpcode::TaskWait) &&
        target != IrValidationTarget::Vm && target != IrValidationTarget::Serialized &&
        target != IrValidationTarget::OptCpp &&
        !(target == IrValidationTarget::Native && nativeEmitsByteMemory())) {
      return failInstruction(
          functionIndex,
          function.name,
          instructionIndex,
          "task opcodes are only supported by the vm, optimized C++ and arm64 native targets "
          "and serialized bytecode",
          error);
    }
    if (inst.op == IrOpcode::CallHost) {
      // The arm64 native backend answers the string heap functions itself and imports any other host function
      // as a C function by name.
      const bool nativeHost = target == IrValidationTarget::Native && nativeEmitsByteMemory();
      const NativeTextHost nativeText = nativeHost && inst.imm < module.hostImports.size()
                                            ? nativeTextHost(module.hostImports[static_cast<size_t>(inst.imm)].name)
                                            : NativeTextHost::NotText;
      if (nativeText == NativeTextHost::Unsupported) {
        return failInstruction(functionIndex,
                               function.name,
                               instructionIndex,
                               "string heap statistics and limits are not available on the native backend yet (docs/todo.md, TODO-5586)",
                               error);
      }
      if (!nativeHost && target != IrValidationTarget::Vm && target != IrValidationTarget::Serialized &&
          target != IrValidationTarget::Cpp && target != IrValidationTarget::OptCpp) {
        return failInstruction(functionIndex,
                               function.name,
                               instructionIndex,
                               "host calls are only supported by the vm target, the C++ targets and serialized bytecode",
                               error);
      }
      if (inst.imm >= module.hostImports.size()) {
        return failInstruction(functionIndex, function.name, instructionIndex, "invalid host import index", error);
      }
      const IrHostImport &hostImport = module.hostImports[static_cast<size_t>(inst.imm)];
      if (hostImport.parameters.size() > MaxCallParameterCount) {
        return failInstruction(
            functionIndex, function.name, instructionIndex, "host import parameter count exceeds supported limit", error);
      }
    }
    if (target != IrValidationTarget::Any && target != IrValidationTarget::Vm &&
        target != IrValidationTarget::Serialized && target != IrValidationTarget::OptCpp &&
        target != IrValidationTarget::Cpp && !(target == IrValidationTarget::Native && nativeEmitsByteMemory()) && isByteOpcode(inst.op)) {
      return failInstruction(functionIndex,
                             function.name,
                             instructionIndex,
                             "u8 and byte memory opcodes are only supported by the vm, the C++ emitters and the arm64 native backend",
                             error);
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
      case IrOpcode::AtomicLoad:
      case IrOpcode::AtomicStore:
      case IrOpcode::AtomicAdd:
      case IrOpcode::AtomicExchange:
      case IrOpcode::AtomicCompareExchange:
        if (inst.imm != 32u && inst.imm != 64u) {
          return failInstruction(functionIndex,
                                 function.name,
                                 instructionIndex,
                                 "atomic width must be 32 or 64",
                                 error);
        }
        break;
      case IrOpcode::TaskSpawn:
        if (inst.imm >= module.functions.size()) {
          return failInstruction(
              functionIndex, function.name, instructionIndex, "invalid task function", error);
        }
        if (module.functions[static_cast<size_t>(inst.imm)].parameterCount >
            MaxCallParameterCount) {
          return failInstruction(functionIndex,
                                 function.name,
                                 instructionIndex,
                                 "task function parameter count exceeds supported limit",
                                 error);
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

// Every reachable instruction must find the operands it consumes, and paths that
// meet at a block must arrive with the same operand-stack depth. The shared CFG
// (IrCfg.h) propagates the depth from the entry, which starts at the function's
// parameter count; unreachable code is not analyzed.
bool validateStackBalance(const IrModule &module,
                          size_t functionIndex,
                          const IrFunction &function,
                          std::string &error) {
  IrCfg cfg;
  IrCfgError cfgError;
  if (buildIrCfg(function, module, cfg, cfgError)) {
    return true;
  }
  const char *message = "operand stack is inconsistent";
  switch (cfgError.kind) {
  case IrCfgErrorKind::StackUnderflow:
    message = "operand stack underflow";
    break;
  case IrCfgErrorKind::InvalidDup:
    message = "dup with an empty operand stack";
    break;
  case IrCfgErrorKind::InconsistentDepth:
    message = "paths reach this block with different operand stack depths";
    break;
  case IrCfgErrorKind::InvalidJumpTarget:
    message = "invalid jump target";
    break;
  case IrCfgErrorKind::UnsupportedOpcode:
    message = "unsupported opcode";
    break;
  case IrCfgErrorKind::None:
    break;
  }
  return failInstruction(functionIndex, function.name, cfgError.instructionIndex, message, error);
}

// A called function's frame is gone once it returns, so an address of it must not be its result: a struct result goes
// back through the caller's block, whose address came in as a parameter (docs/spec/vm-design.md, Real calls). A
// forward analysis over the CFG marks every operand and local that may hold an address of this frame: an
// AddressOfLocal, copied through locals and Dup, and moved by any amount with AddI*/SubI*. A Return of such a value
// fails. Addresses stored in memory and read back are not followed; the lowering never stores a frame address into a
// struct result. Only functions that take an address are analyzed.
bool validateNoOwnFrameAddressReturn(const IrModule &module,
                                     size_t functionIndex,
                                     const IrFunction &function,
                                     std::string &error) {
  uint64_t frameSlots = 0;
  bool takesAddress = false;
  for (const IrInstruction &inst : function.instructions) {
    if (inst.op == IrOpcode::AddressOfLocal) {
      takesAddress = true;
    }
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal ||
        inst.op == IrOpcode::AddressOfLocal) {
      frameSlots = std::max(frameSlots, inst.imm + 1);
    }
  }
  if (!takesAddress) {
    return true;
  }
  IrCfg cfg;
  IrCfgError cfgError;
  if (!buildIrCfg(function, module, cfg, cfgError)) {
    return true; // validateStackBalance reports it
  }
  struct State {
    std::vector<bool> locals;
    std::vector<bool> stack;
  };
  std::vector<State> entry(cfg.blocks.size());
  std::vector<bool> seen(cfg.blocks.size(), false);
  if (cfg.blocks.empty()) {
    return true;
  }
  entry[0].locals.assign(static_cast<size_t>(frameSlots), false);
  entry[0].stack.assign(function.parameterCount, false);
  seen[0] = true;
  std::vector<size_t> worklist = {0};
  while (!worklist.empty()) {
    const size_t b = worklist.back();
    worklist.pop_back();
    State state = entry[b];
    const IrCfgBlock &block = cfg.blocks[b];
    for (size_t i = block.start; i < block.end; ++i) {
      const IrInstruction &inst = function.instructions[i];
      auto &stack = state.stack;
      switch (inst.op) {
      case IrOpcode::AddressOfLocal:
        stack.push_back(true);
        continue;
      case IrOpcode::LoadLocal:
        stack.push_back(state.locals[static_cast<size_t>(inst.imm)]);
        continue;
      case IrOpcode::StoreLocal:
        state.locals[static_cast<size_t>(inst.imm)] = stack.back();
        stack.pop_back();
        continue;
      case IrOpcode::Dup:
        stack.push_back(stack.back());
        continue;
      case IrOpcode::AddI32:
      case IrOpcode::AddI64:
      case IrOpcode::SubI32:
      case IrOpcode::SubI64: {
        const bool address = stack[stack.size() - 1] || stack[stack.size() - 2];
        stack.pop_back();
        stack.back() = address;
        continue;
      }
      case IrOpcode::ReturnI32:
      case IrOpcode::ReturnI64:
        if (stack.back()) {
          return failInstruction(functionIndex,
                                 function.name,
                                 i,
                                 "a called function returns an address of its own frame",
                                 error);
        }
        break;
      default:
        break;
      }
      IrStackEffect effect;
      if (!computeIrStackEffect(inst, module, effect)) {
        return true;
      }
      stack.resize(stack.size() - effect.pops);
      stack.insert(stack.end(), effect.pushes, false);
    }
    for (const size_t successor : block.successors) {
      if (!seen[successor]) {
        entry[successor] = state;
        seen[successor] = true;
        worklist.push_back(successor);
        continue;
      }
      State &into = entry[successor];
      bool changed = false;
      for (size_t k = 0; k < into.locals.size(); ++k) {
        if (state.locals[k] && !into.locals[k]) {
          into.locals[k] = true;
          changed = true;
        }
      }
      for (size_t k = 0; k < into.stack.size() && k < state.stack.size(); ++k) {
        if (state.stack[k] && !into.stack[k]) {
          into.stack[k] = true;
          changed = true;
        }
      }
      if (changed) {
        worklist.push_back(successor);
      }
    }
  }
  return true;
}

// A TaskWait consumes the handle a TaskSpawn pushed (docs/spec/vm-design.md, Tasks). A forward must-analysis over
// the CFG tracks which operands and locals hold a handle on every path: a TaskSpawn result, copied through locals and
// Dup. Any other value reaching a TaskWait fails. Only functions with a TaskWait are analyzed.
bool validateTaskWaitOperands(const IrModule &module,
                              size_t functionIndex,
                              const IrFunction &function,
                              std::string &error) {
  uint64_t frameSlots = 0;
  bool waits = false;
  for (const IrInstruction &inst : function.instructions) {
    waits = waits || inst.op == IrOpcode::TaskWait;
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal ||
        inst.op == IrOpcode::AddressOfLocal) {
      frameSlots = std::max(frameSlots, inst.imm + 1);
    }
  }
  if (!waits) {
    return true;
  }
  IrCfg cfg;
  IrCfgError cfgError;
  if (!buildIrCfg(function, module, cfg, cfgError) || cfg.blocks.empty()) {
    return true; // validateStackBalance reports it
  }
  struct State {
    std::vector<bool> locals;
    std::vector<bool> stack;
  };
  std::vector<State> entry(cfg.blocks.size());
  std::vector<bool> seen(cfg.blocks.size(), false);
  entry[0].locals.assign(static_cast<size_t>(frameSlots), false);
  entry[0].stack.assign(function.parameterCount, false);
  seen[0] = true;
  std::vector<size_t> worklist = {0};
  while (!worklist.empty()) {
    const size_t b = worklist.back();
    worklist.pop_back();
    State state = entry[b];
    const IrCfgBlock &block = cfg.blocks[b];
    for (size_t i = block.start; i < block.end; ++i) {
      const IrInstruction &inst = function.instructions[i];
      auto &stack = state.stack;
      switch (inst.op) {
      case IrOpcode::LoadLocal:
        stack.push_back(state.locals[static_cast<size_t>(inst.imm)]);
        continue;
      case IrOpcode::StoreLocal:
        state.locals[static_cast<size_t>(inst.imm)] = stack.back();
        stack.pop_back();
        continue;
      case IrOpcode::Dup:
        stack.push_back(stack.back());
        continue;
      case IrOpcode::TaskWait:
        if (!stack.back()) {
          return failInstruction(
              functionIndex, function.name, i, "task wait operand is not a task handle", error);
        }
        stack.back() = false;
        continue;
      default:
        break;
      }
      IrStackEffect effect;
      if (!computeIrStackEffect(inst, module, effect)) {
        return true;
      }
      stack.resize(stack.size() - effect.pops);
      stack.insert(stack.end(), effect.pushes, inst.op == IrOpcode::TaskSpawn);
    }
    for (const size_t successor : block.successors) {
      if (!seen[successor]) {
        entry[successor] = state;
        seen[successor] = true;
        worklist.push_back(successor);
        continue;
      }
      // A value is a handle at a join only when it is one on every path into it.
      State &into = entry[successor];
      bool changed = false;
      for (size_t k = 0; k < into.locals.size(); ++k) {
        if (into.locals[k] && !state.locals[k]) {
          into.locals[k] = false;
          changed = true;
        }
      }
      for (size_t k = 0; k < into.stack.size() && k < state.stack.size(); ++k) {
        if (into.stack[k] && !state.stack[k]) {
          into.stack[k] = false;
          changed = true;
        }
      }
      if (changed) {
        worklist.push_back(successor);
      }
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

  // Checked before anything else about a function, so the message names the backend rather than an effect bit.
  if (const char *backendName = atomicRejectingBackendName(target)) {
    // A channel is built from a lock and conditions: a module with a condition opcode is rejected as one with
    // channels, wherever its first lock operation is.
    for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
      const IrFunction &function = module.functions[functionIndex];
      for (size_t instructionIndex = 0; instructionIndex < function.instructions.size();
           ++instructionIndex) {
        if (isConditionOpcode(function.instructions[instructionIndex].op)) {
          return failInstruction(functionIndex,
                                 function.name,
                                 instructionIndex,
                                 "channel operations are not supported by the " +
                                     std::string(backendName) + " backend",
                                 error);
        }
      }
    }
    for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
      const IrFunction &function = module.functions[functionIndex];
      for (size_t instructionIndex = 0; instructionIndex < function.instructions.size();
           ++instructionIndex) {
        const IrOpcode op = function.instructions[instructionIndex].op;
        if (isAtomicOpcode(op) || isMutexOpcode(op)) {
          return failInstruction(functionIndex,
                                 function.name,
                                 instructionIndex,
                                 std::string(isMutexOpcode(op) ? "mutex" : "atomic") +
                                     " operations are not supported by the " +
                                     std::string(backendName) + " backend",
                                 error);
        }
      }
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
    if (!validateStackBalance(module, functionIndex, function, error)) {
      return false;
    }
    if (!validateTaskWaitOperands(module, functionIndex, function, error)) {
      return false;
    }
  }

  std::vector<bool> called(module.functions.size(), false);
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      if ((inst.op == IrOpcode::Call || inst.op == IrOpcode::CallVoid ||
           inst.op == IrOpcode::TaskSpawn) &&
          inst.imm < module.functions.size()) {
        called[static_cast<size_t>(inst.imm)] = true;
      }
    }
  }
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    if (called[functionIndex] &&
        !validateNoOwnFrameAddressReturn(
            module, functionIndex, module.functions[functionIndex], error)) {
      return false;
    }
  }

  if (isGlslTarget(target) && findGlslRecursionCycle(module, error)) {
    return false;
  }

  return true;
}

} // namespace primec
