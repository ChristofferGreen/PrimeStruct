#include "primec/IrToGlslEmitter.h"

#include <cstdint>
#include <sstream>
#include <string>

namespace primec {
namespace {

bool emitInstruction(const IrInstruction &instruction,
                     size_t index,
                     size_t nextIndex,
                     size_t instructionCount,
                     std::ostringstream &out,
                     std::string &error) {
  constexpr uint64_t MaxLocalIndex = 1023;
  const auto emitBinary = [&](const char *op) {
    out << "        int right = stack[--sp];\n";
    out << "        int left = stack[--sp];\n";
    out << "        stack[sp++] = left " << op << " right;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitCompare = [&](const char *op) {
    out << "        int right = stack[--sp];\n";
    out << "        int left = stack[--sp];\n";
    out << "        stack[sp++] = (left " << op << " right) ? 1 : 0;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitBinaryF32 = [&](const char *op) {
    out << "        float right = intBitsToFloat(stack[--sp]);\n";
    out << "        float left = intBitsToFloat(stack[--sp]);\n";
    out << "        stack[sp++] = floatBitsToInt(left " << op << " right);\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitCompareF32 = [&](const char *op) {
    out << "        float right = intBitsToFloat(stack[--sp]);\n";
    out << "        float left = intBitsToFloat(stack[--sp]);\n";
    out << "        stack[sp++] = (left " << op << " right) ? 1 : 0;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };

  switch (instruction.op) {
    case IrOpcode::PushI32: {
      const int32_t value = static_cast<int32_t>(instruction.imm);
      out << "        stack[sp++] = " << value << ";\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    }
    case IrOpcode::PushF32: {
      const uint32_t bits = static_cast<uint32_t>(instruction.imm);
      out << "        stack[sp++] = int(uint(" << bits << "u));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    }
    case IrOpcode::LoadLocal:
      if (instruction.imm > MaxLocalIndex) {
        error = "IrToGlslEmitter local index out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        stack[sp++] = locals[" << instruction.imm << "];\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::StoreLocal:
      if (instruction.imm > MaxLocalIndex) {
        error = "IrToGlslEmitter local index out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        locals[" << instruction.imm << "] = stack[--sp];\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::Dup:
      out << "        stack[sp] = stack[sp - 1];\n";
      out << "        ++sp;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::Pop:
      out << "        --sp;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::AddI32:
      emitBinary("+");
      return true;
    case IrOpcode::SubI32:
      emitBinary("-");
      return true;
    case IrOpcode::MulI32:
      emitBinary("*");
      return true;
    case IrOpcode::DivI32:
      emitBinary("/");
      return true;
    case IrOpcode::NegI32:
      out << "        stack[sp - 1] = -stack[sp - 1];\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::AddF32:
      emitBinaryF32("+");
      return true;
    case IrOpcode::SubF32:
      emitBinaryF32("-");
      return true;
    case IrOpcode::MulF32:
      emitBinaryF32("*");
      return true;
    case IrOpcode::DivF32:
      emitBinaryF32("/");
      return true;
    case IrOpcode::NegF32:
      out << "        float value = intBitsToFloat(stack[sp - 1]);\n";
      out << "        stack[sp - 1] = floatBitsToInt(-value);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::CmpEqI32:
      emitCompare("==");
      return true;
    case IrOpcode::CmpNeI32:
      emitCompare("!=");
      return true;
    case IrOpcode::CmpLtI32:
      emitCompare("<");
      return true;
    case IrOpcode::CmpLeI32:
      emitCompare("<=");
      return true;
    case IrOpcode::CmpGtI32:
      emitCompare(">");
      return true;
    case IrOpcode::CmpGeI32:
      emitCompare(">=");
      return true;
    case IrOpcode::CmpEqF32:
      emitCompareF32("==");
      return true;
    case IrOpcode::CmpNeF32:
      emitCompareF32("!=");
      return true;
    case IrOpcode::CmpLtF32:
      emitCompareF32("<");
      return true;
    case IrOpcode::CmpLeF32:
      emitCompareF32("<=");
      return true;
    case IrOpcode::CmpGtF32:
      emitCompareF32(">");
      return true;
    case IrOpcode::CmpGeF32:
      emitCompareF32(">=");
      return true;
    case IrOpcode::ConvertF32ToI32:
      out << "        stack[sp++] = int(intBitsToFloat(stack[--sp]));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::Jump:
      if (instruction.imm >= instructionCount) {
        error = "IrToGlslEmitter jump target out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        pc = " << instruction.imm << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::JumpIfZero:
      if (instruction.imm >= instructionCount) {
        error = "IrToGlslEmitter jump target out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        int cond = stack[--sp];\n";
      out << "        pc = (cond == 0) ? " << instruction.imm << " : " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ReturnI32:
      out << "        return stack[--sp];\n";
      return true;
    case IrOpcode::ReturnVoid:
      out << "        return 0;\n";
      return true;
    default:
      error = "IrToGlslEmitter unsupported opcode in /" + std::to_string(index) + ": " +
              std::to_string(static_cast<int>(instruction.op));
      return false;
  }
}

} // namespace

bool IrToGlslEmitter::emitSource(const IrModule &module, std::string &out, std::string &error) const {
  out.clear();
  error.clear();
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "IrToGlslEmitter invalid IR entry index";
    return false;
  }

  const IrFunction &entry = module.functions[static_cast<size_t>(module.entryIndex)];
  if (entry.instructions.empty()) {
    error = "IrToGlslEmitter entry function has no instructions";
    return false;
  }

  std::ostringstream body;
  body << "#version 450\n";
  body << "layout(std430, binding = 0) buffer PrimeStructOutput {\n";
  body << "  int value;\n";
  body << "} ps_output;\n";
  body << "int ps_entry_" << static_cast<size_t>(module.entryIndex) << "() {\n";
  body << "  int stack[1024];\n";
  body << "  int locals[1024];\n";
  body << "  for (int i = 0; i < 1024; ++i) {\n";
  body << "    locals[i] = 0;\n";
  body << "  }\n";
  body << "  int sp = 0;\n";
  body << "  int pc = 0;\n";
  body << "  while (true) {\n";
  body << "    switch (pc) {\n";

  const size_t instructionCount = entry.instructions.size();
  for (size_t i = 0; i < instructionCount; ++i) {
    const size_t next = i + 1;
    body << "      case " << i << ": {\n";
    if (!emitInstruction(entry.instructions[i], i, next, instructionCount, body, error)) {
      return false;
    }
    body << "      }\n";
  }

  body << "      default: {\n";
  body << "        return 0;\n";
  body << "      }\n";
  body << "    }\n";
  body << "  }\n";
  body << "}\n";
  body << "void main() {\n";
  body << "  ps_output.value = ps_entry_" << static_cast<size_t>(module.entryIndex) << "();\n";
  body << "}\n";

  out = body.str();
  return true;
}

} // namespace primec
