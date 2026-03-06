#include "primec/IrToGlslEmitter.h"

#include <bit>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace primec {
namespace {

bool emitInstruction(const IrInstruction &instruction,
                     size_t index,
                     size_t nextIndex,
                     size_t instructionCount,
                     size_t functionCount,
                     const std::vector<std::string> &stringTable,
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
  const auto emitCompareU64 = [&](const char *op) {
    out << "        uint right = uint(stack[--sp]);\n";
    out << "        uint left = uint(stack[--sp]);\n";
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
    case IrOpcode::PushI64: {
      const int64_t value = static_cast<int64_t>(instruction.imm);
      if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
        error = "IrToGlslEmitter i64 literal out of i32 range at instruction " + std::to_string(index);
        return false;
      }
      out << "        stack[sp++] = " << static_cast<int32_t>(value) << ";\n";
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
    case IrOpcode::PushF64: {
      const double value = std::bit_cast<double>(instruction.imm);
      const float narrowed = static_cast<float>(value);
      const uint32_t bits = std::bit_cast<uint32_t>(narrowed);
      out << "        // Narrowed GLSL path lowers f64 literals through f32 payloads.\n";
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
    case IrOpcode::AddI64:
      emitBinary("+");
      return true;
    case IrOpcode::SubI64:
      emitBinary("-");
      return true;
    case IrOpcode::MulI64:
      emitBinary("*");
      return true;
    case IrOpcode::DivI64:
      emitBinary("/");
      return true;
    case IrOpcode::DivU64:
      out << "        uint right = uint(stack[--sp]);\n";
      out << "        uint left = uint(stack[--sp]);\n";
      out << "        stack[sp++] = int(left / right);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::NegI64:
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
    case IrOpcode::AddF64:
      out << "        // Narrowed GLSL path lowers f64 add through f32 payloads.\n";
      out << "        float right = intBitsToFloat(stack[--sp]);\n";
      out << "        float left = intBitsToFloat(stack[--sp]);\n";
      out << "        stack[sp++] = floatBitsToInt(left + right);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::SubF64:
      out << "        // Narrowed GLSL path lowers f64 sub through f32 payloads.\n";
      out << "        float right = intBitsToFloat(stack[--sp]);\n";
      out << "        float left = intBitsToFloat(stack[--sp]);\n";
      out << "        stack[sp++] = floatBitsToInt(left - right);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::MulF64:
      out << "        // Narrowed GLSL path lowers f64 mul through f32 payloads.\n";
      out << "        float right = intBitsToFloat(stack[--sp]);\n";
      out << "        float left = intBitsToFloat(stack[--sp]);\n";
      out << "        stack[sp++] = floatBitsToInt(left * right);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::DivF64:
      out << "        // Narrowed GLSL path lowers f64 div through f32 payloads.\n";
      out << "        float right = intBitsToFloat(stack[--sp]);\n";
      out << "        float left = intBitsToFloat(stack[--sp]);\n";
      out << "        stack[sp++] = floatBitsToInt(left / right);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::NegF64:
      out << "        // Narrowed GLSL path lowers f64 neg through f32 payloads.\n";
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
    case IrOpcode::CmpEqI64:
      emitCompare("==");
      return true;
    case IrOpcode::CmpNeI64:
      emitCompare("!=");
      return true;
    case IrOpcode::CmpLtI64:
      emitCompare("<");
      return true;
    case IrOpcode::CmpLeI64:
      emitCompare("<=");
      return true;
    case IrOpcode::CmpGtI64:
      emitCompare(">");
      return true;
    case IrOpcode::CmpGeI64:
      emitCompare(">=");
      return true;
    case IrOpcode::CmpLtU64:
      emitCompareU64("<");
      return true;
    case IrOpcode::CmpLeU64:
      emitCompareU64("<=");
      return true;
    case IrOpcode::CmpGtU64:
      emitCompareU64(">");
      return true;
    case IrOpcode::CmpGeU64:
      emitCompareU64(">=");
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
    case IrOpcode::CmpEqF64:
      out << "        // Narrowed GLSL path lowers f64 equality compare through f32 payloads.\n";
      out << "        float right = intBitsToFloat(stack[--sp]);\n";
      out << "        float left = intBitsToFloat(stack[--sp]);\n";
      out << "        stack[sp++] = (left == right) ? 1 : 0;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::CmpNeF64:
      out << "        // Narrowed GLSL path lowers f64 inequality compare through f32 payloads.\n";
      out << "        float right = intBitsToFloat(stack[--sp]);\n";
      out << "        float left = intBitsToFloat(stack[--sp]);\n";
      out << "        stack[sp++] = (left != right) ? 1 : 0;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::CmpLtF64:
      out << "        // Narrowed GLSL path lowers f64 less-than compare through f32 payloads.\n";
      out << "        float right = intBitsToFloat(stack[--sp]);\n";
      out << "        float left = intBitsToFloat(stack[--sp]);\n";
      out << "        stack[sp++] = (left < right) ? 1 : 0;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::CmpLeF64:
      out << "        // Narrowed GLSL path lowers f64 less-or-equal compare through f32 payloads.\n";
      out << "        float right = intBitsToFloat(stack[--sp]);\n";
      out << "        float left = intBitsToFloat(stack[--sp]);\n";
      out << "        stack[sp++] = (left <= right) ? 1 : 0;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::CmpGtF64:
      out << "        // Narrowed GLSL path lowers f64 greater-than compare through f32 payloads.\n";
      out << "        float right = intBitsToFloat(stack[--sp]);\n";
      out << "        float left = intBitsToFloat(stack[--sp]);\n";
      out << "        stack[sp++] = (left > right) ? 1 : 0;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::CmpGeF64:
      out << "        // Narrowed GLSL path lowers f64 greater-or-equal compare through f32 payloads.\n";
      out << "        float right = intBitsToFloat(stack[--sp]);\n";
      out << "        float left = intBitsToFloat(stack[--sp]);\n";
      out << "        stack[sp++] = (left >= right) ? 1 : 0;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToI32:
      out << "        float value = intBitsToFloat(stack[--sp]);\n";
      out << "        int converted = 0;\n";
      out << "        if (isnan(value)) {\n";
      out << "          converted = 0;\n";
      out << "        } else if (value >= 2147483647.0) {\n";
      out << "          converted = 2147483647;\n";
      out << "        } else if (value <= -2147483648.0) {\n";
      out << "          converted = -2147483647 - 1;\n";
      out << "        } else {\n";
      out << "          converted = int(value);\n";
      out << "        }\n";
      out << "        stack[sp++] = converted;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToI64:
      out << "        float value = intBitsToFloat(stack[--sp]);\n";
      out << "        int converted = 0;\n";
      out << "        if (isnan(value)) {\n";
      out << "          converted = 0;\n";
      out << "        } else if (value >= 2147483647.0) {\n";
      out << "          converted = 2147483647;\n";
      out << "        } else if (value <= -2147483648.0) {\n";
      out << "          converted = -2147483647 - 1;\n";
      out << "        } else {\n";
      out << "          converted = int(value);\n";
      out << "        }\n";
      out << "        stack[sp++] = converted;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToU64:
      out << "        float value = intBitsToFloat(stack[--sp]);\n";
      out << "        uint converted = 0u;\n";
      out << "        if (isnan(value) || value <= 0.0) {\n";
      out << "          converted = 0u;\n";
      out << "        } else if (value >= 2147483647.0) {\n";
      out << "          converted = 2147483647u;\n";
      out << "        } else {\n";
      out << "          converted = uint(int(value));\n";
      out << "        }\n";
      out << "        stack[sp++] = int(converted);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI32ToF32:
      out << "        stack[sp++] = floatBitsToInt(float(stack[--sp]));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI64ToF32:
      out << "        stack[sp++] = floatBitsToInt(float(stack[--sp]));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertU64ToF32:
      out << "        stack[sp++] = floatBitsToInt(float(uint(stack[--sp])));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI64ToF64:
      out << "        // Narrowed GLSL path lowers i64/f64 conversion through f32 payloads.\n";
      out << "        stack[sp++] = floatBitsToInt(float(stack[--sp]));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertU64ToF64:
      out << "        // Narrowed GLSL path lowers u64/f64 conversion through f32 payloads.\n";
      out << "        stack[sp++] = floatBitsToInt(float(uint(stack[--sp])));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI32ToF64:
      out << "        // Narrowed GLSL path lowers i32/f64 conversion through f32 payloads.\n";
      out << "        stack[sp++] = floatBitsToInt(float(stack[--sp]));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToI32:
      out << "        // Narrowed GLSL path lowers f64/i32 conversion through f32 payloads.\n";
      out << "        float value = intBitsToFloat(stack[--sp]);\n";
      out << "        int converted = 0;\n";
      out << "        if (isnan(value)) {\n";
      out << "          converted = 0;\n";
      out << "        } else if (value >= 2147483647.0) {\n";
      out << "          converted = 2147483647;\n";
      out << "        } else if (value <= -2147483648.0) {\n";
      out << "          converted = -2147483647 - 1;\n";
      out << "        } else {\n";
      out << "          converted = int(value);\n";
      out << "        }\n";
      out << "        stack[sp++] = converted;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToI64:
      out << "        // Narrowed GLSL path lowers f64/i64 conversion through f32 payloads.\n";
      out << "        float value = intBitsToFloat(stack[--sp]);\n";
      out << "        int converted = 0;\n";
      out << "        if (isnan(value)) {\n";
      out << "          converted = 0;\n";
      out << "        } else if (value >= 2147483647.0) {\n";
      out << "          converted = 2147483647;\n";
      out << "        } else if (value <= -2147483648.0) {\n";
      out << "          converted = -2147483647 - 1;\n";
      out << "        } else {\n";
      out << "          converted = int(value);\n";
      out << "        }\n";
      out << "        stack[sp++] = converted;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToU64:
      out << "        // Narrowed GLSL path lowers f64/u64 conversion through f32 payloads.\n";
      out << "        float value = intBitsToFloat(stack[--sp]);\n";
      out << "        uint converted = 0u;\n";
      out << "        if (isnan(value) || value <= 0.0) {\n";
      out << "          converted = 0u;\n";
      out << "        } else if (value >= 2147483647.0) {\n";
      out << "          converted = 2147483647u;\n";
      out << "        } else {\n";
      out << "          converted = uint(int(value));\n";
      out << "        }\n";
      out << "        stack[sp++] = int(converted);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToF64:
      out << "        // Narrowed GLSL path keeps f32/f64 conversion as bit-preserving passthrough.\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToF32:
      out << "        // Narrowed GLSL path keeps f64/f32 conversion as bit-preserving passthrough.\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::PrintString: {
      const uint64_t stringIndex = decodePrintStringIndex(instruction.imm);
      if (stringIndex >= stringTable.size()) {
        error = "IrToGlslEmitter string index out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        // GLSL backend ignores print-string side effects.\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    }
    case IrOpcode::PrintI32:
    case IrOpcode::PrintI64:
    case IrOpcode::PrintU64:
    case IrOpcode::PrintStringDynamic:
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe:
      out << "        --sp; // GLSL backend ignores print side effects.\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileOpenRead:
      if (instruction.imm >= stringTable.size()) {
        error = "IrToGlslEmitter string index out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        // GLSL backend cannot open files; push deterministic invalid file handle.\n";
      out << "        stack[sp++] = -1;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileOpenWrite:
      if (instruction.imm >= stringTable.size()) {
        error = "IrToGlslEmitter string index out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        // GLSL backend cannot open files; push deterministic invalid file handle.\n";
      out << "        stack[sp++] = -1;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::LoadStringByte:
      if (instruction.imm >= stringTable.size()) {
        error = "IrToGlslEmitter string index out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        uint stringByteIndex = uint(stack[--sp]);\n";
      out << "        int stringByte = 0;\n";
      out << "        switch (stringByteIndex) {\n";
      for (size_t byteIndex = 0; byteIndex < stringTable[static_cast<size_t>(instruction.imm)].size(); ++byteIndex) {
        const uint32_t byteValue =
            static_cast<uint32_t>(static_cast<unsigned char>(stringTable[static_cast<size_t>(instruction.imm)][byteIndex]));
        out << "          case " << byteIndex << "u: stringByte = " << byteValue << "; break;\n";
      }
      out << "          default: stringByte = 0; break;\n";
      out << "        }\n";
      out << "        stack[sp++] = stringByte;\n";
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
    case IrOpcode::Call:
      if (instruction.imm >= functionCount) {
        error = "IrToGlslEmitter call target out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        stack[sp++] = ps_entry_" << instruction.imm << "(stack, sp);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::CallVoid:
      if (instruction.imm >= functionCount) {
        error = "IrToGlslEmitter call target out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        ps_entry_" << instruction.imm << "(stack, sp);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::PushArgc:
      out << "        stack[sp++] = 0; // GLSL backend has no argv\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ReturnF32:
      out << "        return stack[--sp];\n";
      return true;
    case IrOpcode::ReturnI32:
      out << "        return stack[--sp];\n";
      return true;
    case IrOpcode::ReturnI64:
      out << "        return stack[--sp];\n";
      return true;
    case IrOpcode::ReturnF64:
      out << "        // Narrowed GLSL path returns f64 values through f32 payloads.\n";
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

bool emitFunction(const IrFunction &function,
                  size_t functionIndex,
                  size_t functionCount,
                  const std::vector<std::string> &stringTable,
                  std::ostringstream &out,
                  std::string &error) {
  if (function.instructions.empty()) {
    error = "IrToGlslEmitter function has no instructions at index " + std::to_string(functionIndex);
    return false;
  }

  out << "int ps_entry_" << functionIndex << "(inout int stack[1024], inout int sp) {\n";
  out << "  int locals[1024];\n";
  out << "  for (int i = 0; i < 1024; ++i) {\n";
  out << "    locals[i] = 0;\n";
  out << "  }\n";
  out << "  int pc = 0;\n";
  out << "  while (true) {\n";
  out << "    switch (pc) {\n";

  const size_t instructionCount = function.instructions.size();
  for (size_t i = 0; i < instructionCount; ++i) {
    const size_t next = i + 1;
    out << "      case " << i << ": {\n";
    if (!emitInstruction(function.instructions[i], i, next, instructionCount, functionCount, stringTable, out,
                         error)) {
      return false;
    }
    out << "      }\n";
  }

  out << "      default: {\n";
  out << "        return 0;\n";
  out << "      }\n";
  out << "    }\n";
  out << "  }\n";
  out << "}\n";
  return true;
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
  for (size_t i = 0; i < module.functions.size(); ++i) {
    body << "int ps_entry_" << i << "(inout int stack[1024], inout int sp);\n";
  }
  for (size_t i = 0; i < module.functions.size(); ++i) {
    if (!emitFunction(module.functions[i], i, module.functions.size(), module.stringTable, body, error)) {
      return false;
    }
  }
  body << "void main() {\n";
  body << "  int stack[1024];\n";
  body << "  int sp = 0;\n";
  body << "  ps_output.value = ps_entry_" << static_cast<size_t>(module.entryIndex) << "(stack, sp);\n";
  body << "}\n";

  out = body.str();
  return true;
}

} // namespace primec
