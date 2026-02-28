#include "primec/IrValidation.h"

#include <cstdint>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace primec {
namespace {

constexpr uint8_t MinOpcode = static_cast<uint8_t>(IrOpcode::PushI32);
constexpr uint8_t MaxOpcode = static_cast<uint8_t>(IrOpcode::PrintStringDynamic);
constexpr uint64_t KnownEffectMask = EffectIoOut | EffectIoErr | EffectHeapAlloc | EffectPathSpaceNotify |
                                     EffectPathSpaceInsert | EffectPathSpaceTake | EffectFileWrite |
                                     EffectGpuDispatch | EffectPathSpaceBind | EffectPathSpaceSchedule;

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

  for (size_t instructionIndex = 0; instructionIndex < function.instructions.size(); ++instructionIndex) {
    const IrInstruction &inst = function.instructions[instructionIndex];
    const uint8_t opcodeValue = static_cast<uint8_t>(inst.op);
    if (opcodeValue < MinOpcode || opcodeValue > MaxOpcode) {
      return failInstruction(functionIndex, function.name, instructionIndex, "unsupported opcode", error);
    }
    switch (inst.op) {
      case IrOpcode::JumpIfZero:
      case IrOpcode::Jump:
        if (inst.imm > function.instructions.size()) {
          return failInstruction(functionIndex, function.name, instructionIndex, "invalid jump target", error);
        }
        break;
      case IrOpcode::LoadLocal:
      case IrOpcode::StoreLocal:
      case IrOpcode::AddressOfLocal:
        if (inst.imm > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
          return failInstruction(functionIndex, function.name, instructionIndex, "local index exceeds 32-bit limit", error);
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
