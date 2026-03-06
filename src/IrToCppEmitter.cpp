#include "primec/IrToCppEmitter.h"

#include "primec/Ir.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace primec {
namespace {

std::string escapeCppStringLiteral(const std::string &text) {
  std::string escaped;
  escaped.reserve(text.size() + 2);
  for (unsigned char ch : text) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(static_cast<char>(ch));
        break;
    }
  }
  return escaped;
}

struct EmitContext {
  size_t stringCount = 0;
  size_t functionCount = 0;
};

std::string irFunctionSymbol(size_t functionIndex) {
  return "ps_fn_" + std::to_string(functionIndex);
}

bool emitInstruction(const IrInstruction &instruction,
                     size_t index,
                     size_t nextIndex,
                     size_t instructionCount,
                     const EmitContext &context,
                     std::ostringstream &out,
                     std::string &error) {
  constexpr uint64_t MaxLocalIndex = 1023;
  const auto emitBinaryI32 = [&](const char *op) {
    out << "        int32_t right = static_cast<int32_t>(stack[--sp]);\n";
    out << "        int32_t left = static_cast<int32_t>(stack[--sp]);\n";
    out << "        stack[sp++] = left " << op << " right;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitCompareI32 = [&](const char *op) {
    out << "        int32_t right = static_cast<int32_t>(stack[--sp]);\n";
    out << "        int32_t left = static_cast<int32_t>(stack[--sp]);\n";
    out << "        stack[sp++] = (left " << op << " right) ? 1u : 0u;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitBinaryI64 = [&](const char *op) {
    out << "        int64_t right = static_cast<int64_t>(stack[--sp]);\n";
    out << "        int64_t left = static_cast<int64_t>(stack[--sp]);\n";
    out << "        stack[sp++] = static_cast<uint64_t>(left " << op << " right);\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitCompareI64 = [&](const char *op) {
    out << "        int64_t right = static_cast<int64_t>(stack[--sp]);\n";
    out << "        int64_t left = static_cast<int64_t>(stack[--sp]);\n";
    out << "        stack[sp++] = (left " << op << " right) ? 1u : 0u;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitCompareU64 = [&](const char *op) {
    out << "        uint64_t right = stack[--sp];\n";
    out << "        uint64_t left = stack[--sp];\n";
    out << "        stack[sp++] = (left " << op << " right) ? 1 : 0;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitPrintValue = [&](const char *valueExpr, uint64_t flags) {
    const char *stream = (flags & PrintFlagStderr) != 0 ? "std::cerr" : "std::cout";
    const bool newline = (flags & PrintFlagNewline) != 0;
    out << "        " << stream << " << " << valueExpr << ";\n";
    if (newline) {
      out << "        " << stream << " << '\\n';\n";
    }
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
      out << "        stack[sp++] = static_cast<uint64_t>(" << value << "ll);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    }
    case IrOpcode::LoadLocal:
      if (instruction.imm > MaxLocalIndex) {
        error = "IrToCppEmitter local index out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        stack[sp++] = locals[" << instruction.imm << "];\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::StoreLocal:
      if (instruction.imm > MaxLocalIndex) {
        error = "IrToCppEmitter local index out of range at instruction " + std::to_string(index);
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
      emitBinaryI32("+");
      return true;
    case IrOpcode::SubI32:
      emitBinaryI32("-");
      return true;
    case IrOpcode::MulI32:
      emitBinaryI32("*");
      return true;
    case IrOpcode::DivI32:
      emitBinaryI32("/");
      return true;
    case IrOpcode::NegI32:
      out << "        int32_t value = static_cast<int32_t>(stack[sp - 1]);\n";
      out << "        stack[sp - 1] = static_cast<uint64_t>(-value);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::AddI64:
      emitBinaryI64("+");
      return true;
    case IrOpcode::SubI64:
      emitBinaryI64("-");
      return true;
    case IrOpcode::MulI64:
      emitBinaryI64("*");
      return true;
    case IrOpcode::DivI64:
      emitBinaryI64("/");
      return true;
    case IrOpcode::DivU64:
      out << "        uint64_t right = stack[--sp];\n";
      out << "        uint64_t left = stack[--sp];\n";
      out << "        stack[sp++] = left / right;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::NegI64:
      out << "        int64_t value = static_cast<int64_t>(stack[sp - 1]);\n";
      out << "        stack[sp - 1] = static_cast<uint64_t>(-value);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::CmpEqI32:
      emitCompareI32("==");
      return true;
    case IrOpcode::CmpNeI32:
      emitCompareI32("!=");
      return true;
    case IrOpcode::CmpLtI32:
      emitCompareI32("<");
      return true;
    case IrOpcode::CmpLeI32:
      emitCompareI32("<=");
      return true;
    case IrOpcode::CmpGtI32:
      emitCompareI32(">");
      return true;
    case IrOpcode::CmpGeI32:
      emitCompareI32(">=");
      return true;
    case IrOpcode::CmpEqI64:
      emitCompareI64("==");
      return true;
    case IrOpcode::CmpNeI64:
      emitCompareI64("!=");
      return true;
    case IrOpcode::CmpLtI64:
      emitCompareI64("<");
      return true;
    case IrOpcode::CmpLeI64:
      emitCompareI64("<=");
      return true;
    case IrOpcode::CmpGtI64:
      emitCompareI64(">");
      return true;
    case IrOpcode::CmpGeI64:
      emitCompareI64(">=");
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
    case IrOpcode::Jump:
      if (instruction.imm >= instructionCount) {
        error = "IrToCppEmitter jump target out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        pc = " << instruction.imm << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::JumpIfZero:
      if (instruction.imm >= instructionCount) {
        error = "IrToCppEmitter jump target out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        int64_t cond = static_cast<int64_t>(stack[--sp]);\n";
      out << "        pc = (cond == 0) ? " << instruction.imm << " : " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::Call:
    case IrOpcode::CallVoid:
      if (instruction.imm >= context.functionCount) {
        error = "IrToCppEmitter call target out of range at instruction " + std::to_string(index);
        return false;
      }
      if (instruction.op == IrOpcode::Call) {
        out << "        int64_t callValue = " << irFunctionSymbol(static_cast<size_t>(instruction.imm))
            << "(stack, sp, argc, argv);\n";
        out << "        stack[sp++] = static_cast<uint64_t>(callValue);\n";
      } else {
        out << "        " << irFunctionSymbol(static_cast<size_t>(instruction.imm)) << "(stack, sp, argc, argv);\n";
      }
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::PushArgc:
      out << "        stack[sp++] = static_cast<uint64_t>(argc);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::PrintI32:
      emitPrintValue("static_cast<int32_t>(stack[--sp])", decodePrintFlags(instruction.imm));
      return true;
    case IrOpcode::PrintI64:
      emitPrintValue("static_cast<int64_t>(stack[--sp])", decodePrintFlags(instruction.imm));
      return true;
    case IrOpcode::PrintU64:
      emitPrintValue("stack[--sp]", decodePrintFlags(instruction.imm));
      return true;
    case IrOpcode::PrintString: {
      const uint64_t stringIndex = decodePrintStringIndex(instruction.imm);
      if (stringIndex >= context.stringCount) {
        error = "IrToCppEmitter string index out of range at instruction " + std::to_string(index);
        return false;
      }
      std::ostringstream value;
      value << "ps_string_table[" << stringIndex << "]";
      emitPrintValue(value.str().c_str(), decodePrintFlags(instruction.imm));
      return true;
    }
    case IrOpcode::PrintStringDynamic: {
      uint64_t flags = decodePrintFlags(instruction.imm);
      out << "        uint64_t stringIndex = stack[--sp];\n";
      out << "        if (stringIndex >= ps_string_table_count) {\n";
      out << "          std::cerr << \"invalid string index in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      const char *stream = (flags & PrintFlagStderr) != 0 ? "std::cerr" : "std::cout";
      const bool newline = (flags & PrintFlagNewline) != 0;
      out << "        " << stream << " << ps_string_table[stringIndex];\n";
      if (newline) {
        out << "        " << stream << " << '\\n';\n";
      }
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    }
    case IrOpcode::PrintArgv: {
      uint64_t flags = decodePrintFlags(instruction.imm);
      out << "        int64_t indexValue = static_cast<int64_t>(stack[--sp]);\n";
      out << "        if (indexValue < 0 || indexValue >= static_cast<int64_t>(argc)) {\n";
      out << "          std::cerr << \"invalid argv index in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      const char *stream = (flags & PrintFlagStderr) != 0 ? "std::cerr" : "std::cout";
      const bool newline = (flags & PrintFlagNewline) != 0;
      out << "        " << stream << " << argv[indexValue];\n";
      if (newline) {
        out << "        " << stream << " << '\\n';\n";
      }
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    }
    case IrOpcode::PrintArgvUnsafe: {
      uint64_t flags = decodePrintFlags(instruction.imm);
      out << "        int64_t indexValue = static_cast<int64_t>(stack[--sp]);\n";
      out << "        if (indexValue >= 0 && indexValue < static_cast<int64_t>(argc)) {\n";
      const char *stream = (flags & PrintFlagStderr) != 0 ? "std::cerr" : "std::cout";
      const bool newline = (flags & PrintFlagNewline) != 0;
      out << "          " << stream << " << argv[indexValue];\n";
      if (newline) {
        out << "          " << stream << " << '\\n';\n";
      }
      out << "        }\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    }
    case IrOpcode::ReturnI32:
      out << "        return static_cast<int64_t>(static_cast<int32_t>(stack[--sp]));\n";
      return true;
    case IrOpcode::ReturnI64:
      out << "        return static_cast<int64_t>(stack[--sp]);\n";
      return true;
    case IrOpcode::ReturnVoid:
      out << "        return 0;\n";
      return true;
    default:
      error = "IrToCppEmitter unsupported opcode at instruction " + std::to_string(index);
      return false;
  }
}

} // namespace

bool IrToCppEmitter::emitSource(const IrModule &module, std::string &out, std::string &error) const {
  out.clear();
  error.clear();
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "IrToCppEmitter invalid IR entry index";
    return false;
  }
  const IrFunction &entry = module.functions[static_cast<size_t>(module.entryIndex)];
  if (entry.instructions.empty()) {
    error = "IrToCppEmitter entry function has no instructions";
    return false;
  }

  std::ostringstream body;
  body << "#include <cstddef>\n";
  body << "#include <cstdint>\n\n";
  body << "#include <iostream>\n\n";
  body << "static const char *ps_string_table[] = {\n";
  if (module.stringTable.empty()) {
    body << "  \"\",\n";
  } else {
    for (const std::string &text : module.stringTable) {
      body << "  \"" << escapeCppStringLiteral(text) << "\",\n";
    }
  }
  body << "};\n\n";
  body << "static constexpr std::size_t ps_string_table_count = " << module.stringTable.size() << "u;\n\n";

  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    body << "static int64_t " << irFunctionSymbol(functionIndex)
         << "(uint64_t *stack, std::size_t &sp, int argc, char **argv);\n";
  }
  body << "\n";

  const EmitContext context{
      .stringCount = module.stringTable.size(),
      .functionCount = module.functions.size(),
  };
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const IrFunction &function = module.functions[functionIndex];
    body << "static int64_t " << irFunctionSymbol(functionIndex)
         << "(uint64_t *stack, std::size_t &sp, int argc, char **argv) {\n";
    body << "  uint64_t locals[1024] = {};\n";
    body << "  std::size_t pc = 0;\n";
    body << "  while (true) {\n";
    body << "    switch (pc) {\n";

    const size_t instructionCount = function.instructions.size();
    for (size_t i = 0; i < instructionCount; ++i) {
      const size_t next = i + 1;
      body << "      case " << i << ": {\n";
      if (!emitInstruction(function.instructions[i], i, next, instructionCount, context, body, error)) {
        return false;
      }
      body << "      }\n";
    }

    body << "      default:\n";
    body << "        return 0;\n";
    body << "    }\n";
    body << "  }\n";
    body << "}\n\n";
  }

  body << "static int64_t ps_entry_" << static_cast<size_t>(module.entryIndex) << "(int argc, char **argv) {\n";
  body << "  uint64_t stack[1024] = {};\n";
  body << "  std::size_t sp = 0;\n";
  body << "  return " << irFunctionSymbol(static_cast<size_t>(module.entryIndex)) << "(stack, sp, argc, argv);\n";
  body << "}\n\n";
  body << "int main(int argc, char **argv) {\n";
  body << "  return static_cast<int>(ps_entry_" << static_cast<size_t>(module.entryIndex) << "(argc, argv));\n";
  body << "}\n";

  out = body.str();
  return true;
}

} // namespace primec
