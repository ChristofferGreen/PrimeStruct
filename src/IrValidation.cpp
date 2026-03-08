#include "primec/IrValidation.h"

#include <cstdint>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace primec {
namespace {

constexpr uint8_t MinOpcode = static_cast<uint8_t>(IrOpcode::PushI32);
constexpr uint8_t MaxOpcode = static_cast<uint8_t>(IrOpcode::HeapAlloc);
constexpr uint64_t MaxGlslLocalIndex = 1023;
constexpr uint64_t KnownEffectMask = EffectIoOut | EffectIoErr | EffectHeapAlloc | EffectPathSpaceNotify |
                                     EffectPathSpaceInsert | EffectPathSpaceTake | EffectFileWrite |
                                     EffectGpuDispatch | EffectPathSpaceBind | EffectPathSpaceSchedule;
constexpr uint64_t KnownWasmWasiEffectMask = EffectIoOut | EffectIoErr | EffectFileWrite;
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

bool isGlslOpcodeAllowed(IrOpcode op) {
  switch (op) {
    case IrOpcode::PushI32:
    case IrOpcode::PushI64:
    case IrOpcode::LoadLocal:
    case IrOpcode::StoreLocal:
    case IrOpcode::AddressOfLocal:
    case IrOpcode::LoadIndirect:
    case IrOpcode::StoreIndirect:
    case IrOpcode::Dup:
    case IrOpcode::Pop:
    case IrOpcode::AddI32:
    case IrOpcode::SubI32:
    case IrOpcode::MulI32:
    case IrOpcode::DivI32:
    case IrOpcode::NegI32:
    case IrOpcode::AddI64:
    case IrOpcode::SubI64:
    case IrOpcode::MulI64:
    case IrOpcode::DivI64:
    case IrOpcode::DivU64:
    case IrOpcode::NegI64:
    case IrOpcode::CmpEqI32:
    case IrOpcode::CmpNeI32:
    case IrOpcode::CmpLtI32:
    case IrOpcode::CmpLeI32:
    case IrOpcode::CmpGtI32:
    case IrOpcode::CmpGeI32:
    case IrOpcode::CmpEqI64:
    case IrOpcode::CmpNeI64:
    case IrOpcode::CmpLtI64:
    case IrOpcode::CmpLeI64:
    case IrOpcode::CmpGtI64:
    case IrOpcode::CmpGeI64:
    case IrOpcode::CmpLtU64:
    case IrOpcode::CmpLeU64:
    case IrOpcode::CmpGtU64:
    case IrOpcode::CmpGeU64:
    case IrOpcode::JumpIfZero:
    case IrOpcode::Jump:
    case IrOpcode::ReturnVoid:
    case IrOpcode::ReturnI32:
    case IrOpcode::ReturnI64:
    case IrOpcode::PrintI32:
    case IrOpcode::PrintI64:
    case IrOpcode::PrintU64:
    case IrOpcode::PrintString:
    case IrOpcode::PushArgc:
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe:
    case IrOpcode::LoadStringByte:
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
    case IrOpcode::FileClose:
    case IrOpcode::FileFlush:
    case IrOpcode::FileWriteI32:
    case IrOpcode::FileWriteI64:
    case IrOpcode::FileWriteU64:
    case IrOpcode::FileWriteString:
    case IrOpcode::FileWriteByte:
    case IrOpcode::FileWriteNewline:
    case IrOpcode::PushF32:
    case IrOpcode::PushF64:
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32:
    case IrOpcode::NegF32:
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64:
    case IrOpcode::NegF64:
    case IrOpcode::CmpEqF32:
    case IrOpcode::CmpNeF32:
    case IrOpcode::CmpLtF32:
    case IrOpcode::CmpLeF32:
    case IrOpcode::CmpGtF32:
    case IrOpcode::CmpGeF32:
    case IrOpcode::CmpEqF64:
    case IrOpcode::CmpNeF64:
    case IrOpcode::CmpLtF64:
    case IrOpcode::CmpLeF64:
    case IrOpcode::CmpGtF64:
    case IrOpcode::CmpGeF64:
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF32:
    case IrOpcode::ConvertU64ToF64:
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF32ToU64:
    case IrOpcode::ConvertF64ToI32:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF64ToU64:
    case IrOpcode::ConvertF32ToF64:
    case IrOpcode::ConvertF64ToF32:
    case IrOpcode::ReturnF32:
    case IrOpcode::ReturnF64:
    case IrOpcode::PrintStringDynamic:
    case IrOpcode::Call:
    case IrOpcode::CallVoid:
      return true;
    default:
      return false;
  }
}

bool isWasmOpcodeAllowedWasi(IrOpcode op) {
  switch (op) {
    case IrOpcode::PushI32:
    case IrOpcode::LoadLocal:
    case IrOpcode::StoreLocal:
    case IrOpcode::Dup:
    case IrOpcode::Pop:
    case IrOpcode::AddI32:
    case IrOpcode::SubI32:
    case IrOpcode::MulI32:
    case IrOpcode::DivI32:
    case IrOpcode::NegI32:
    case IrOpcode::CmpEqI32:
    case IrOpcode::CmpNeI32:
    case IrOpcode::CmpLtI32:
    case IrOpcode::CmpLeI32:
    case IrOpcode::CmpGtI32:
    case IrOpcode::CmpGeI32:
    case IrOpcode::PushF32:
    case IrOpcode::PushF64:
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32:
    case IrOpcode::NegF32:
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64:
    case IrOpcode::NegF64:
    case IrOpcode::CmpEqF32:
    case IrOpcode::CmpNeF32:
    case IrOpcode::CmpLtF32:
    case IrOpcode::CmpLeF32:
    case IrOpcode::CmpGtF32:
    case IrOpcode::CmpGeF32:
    case IrOpcode::CmpEqF64:
    case IrOpcode::CmpNeF64:
    case IrOpcode::CmpLtF64:
    case IrOpcode::CmpLeF64:
    case IrOpcode::CmpGtF64:
    case IrOpcode::CmpGeF64:
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF32:
    case IrOpcode::ConvertU64ToF64:
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF32ToU64:
    case IrOpcode::ConvertF64ToI32:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF64ToU64:
    case IrOpcode::ConvertF32ToF64:
    case IrOpcode::ConvertF64ToF32:
    case IrOpcode::PushArgc:
    case IrOpcode::PrintString:
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe:
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
    case IrOpcode::FileClose:
    case IrOpcode::FileFlush:
    case IrOpcode::FileWriteI32:
    case IrOpcode::FileWriteI64:
    case IrOpcode::FileWriteU64:
    case IrOpcode::FileWriteString:
    case IrOpcode::FileWriteByte:
    case IrOpcode::FileWriteNewline:
    case IrOpcode::JumpIfZero:
    case IrOpcode::Jump:
    case IrOpcode::Call:
    case IrOpcode::CallVoid:
    case IrOpcode::ReturnVoid:
    case IrOpcode::ReturnI32:
    case IrOpcode::ReturnF32:
    case IrOpcode::ReturnF64:
      return true;
    default:
      return false;
  }
}

bool isWasiOnlyOpcode(IrOpcode op) {
  switch (op) {
    case IrOpcode::PushArgc:
    case IrOpcode::PrintString:
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe:
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
    case IrOpcode::FileClose:
    case IrOpcode::FileFlush:
    case IrOpcode::FileWriteI32:
    case IrOpcode::FileWriteI64:
    case IrOpcode::FileWriteU64:
    case IrOpcode::FileWriteString:
    case IrOpcode::FileWriteByte:
    case IrOpcode::FileWriteNewline:
      return true;
    default:
      return false;
  }
}

bool isWasmOpcodeAllowedForTarget(IrOpcode op, IrValidationTarget target) {
  if (!isWasmOpcodeAllowedWasi(op)) {
    return false;
  }
  if (target == IrValidationTarget::WasmBrowser && isWasiOnlyOpcode(op)) {
    return false;
  }
  return true;
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
        break;
      case IrOpcode::LoadLocal:
      case IrOpcode::StoreLocal:
      case IrOpcode::AddressOfLocal:
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
      default:
        break;
    }
  }
  return true;
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

  return true;
}

} // namespace primec
