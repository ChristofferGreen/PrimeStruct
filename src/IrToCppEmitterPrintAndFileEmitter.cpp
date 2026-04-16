#include "IrToCppEmitterInternal.h"

#include <iostream>
#include <sstream>
#include <string>

namespace primec {

bool emitPrintAndFileInstruction(const IrInstruction &instruction,
                                 size_t index,
                                 size_t nextIndex,
                                 size_t localCount,
                                 const EmitContext &context,
                                 std::ostringstream &out,
                                 std::string &error) {
  const auto emitStackUnderflowGuard = [&](size_t required, const char *operation) {
    out << "        if (!psEnsureStack(sp, " << required << "ull, \"" << operation << "\")) {\n";
    out << "          return 1;\n";
    out << "        }\n";
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
  const auto stringOperandError = [&](const char *opcodeName, uint64_t stringIndex) {
    error = "IrToCppEmitter string index out of range at instruction " + std::to_string(index) + " (" + opcodeName +
            ", imm=" + std::to_string(instruction.imm) + ", stringIndex=" + std::to_string(stringIndex) + ")";
  };

  switch (instruction.op) {
    case IrOpcode::PushArgc:
      out << "        stack[sp++] = static_cast<uint64_t>(argc);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::PrintI32:
      emitStackUnderflowGuard(1, "print");
      emitPrintValue("static_cast<int32_t>(stack[--sp])", decodePrintFlags(instruction.imm));
      return true;
    case IrOpcode::PrintI64:
      emitStackUnderflowGuard(1, "print");
      emitPrintValue("static_cast<int64_t>(stack[--sp])", decodePrintFlags(instruction.imm));
      return true;
    case IrOpcode::PrintU64:
      emitStackUnderflowGuard(1, "print");
      emitPrintValue("stack[--sp]", decodePrintFlags(instruction.imm));
      return true;
    case IrOpcode::PrintString: {
      const uint64_t stringIndex = decodePrintStringIndex(instruction.imm);
      if (stringIndex >= context.stringCount) {
        stringOperandError("PrintString", stringIndex);
        return false;
      }
      std::ostringstream value;
      value << "ps_string_table[" << stringIndex << "]";
      emitPrintValue(value.str().c_str(), decodePrintFlags(instruction.imm));
      return true;
    }
    case IrOpcode::PrintStringDynamic: {
      uint64_t flags = decodePrintFlags(instruction.imm);
      emitStackUnderflowGuard(1, "print");
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
      emitStackUnderflowGuard(1, "print");
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
      emitStackUnderflowGuard(1, "print");
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
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
      if (instruction.imm >= context.stringCount) {
        stringOperandError(instruction.op == IrOpcode::FileOpenWrite
                               ? "FileOpenWrite"
                               : instruction.op == IrOpcode::FileOpenAppend ? "FileOpenAppend" : "FileOpenRead",
                           instruction.imm);
        return false;
      }
      out << "        int fileOpenFlags = ";
      if (instruction.op == IrOpcode::FileOpenWrite) {
        out << "O_WRONLY | O_CREAT | O_TRUNC";
      } else if (instruction.op == IrOpcode::FileOpenAppend) {
        out << "O_WRONLY | O_CREAT | O_APPEND";
      } else {
        out << "O_RDONLY";
      }
      out << ";\n";
      out << "        int fileFd = ::open(ps_string_table[" << instruction.imm << "], fileOpenFlags, 0644);\n";
      out << "        uint64_t filePacked = 0;\n";
      out << "        if (fileFd < 0) {\n";
      out << "          uint32_t fileErr = errno == 0 ? 1u : static_cast<uint32_t>(errno);\n";
      out << "          filePacked = static_cast<uint64_t>(fileErr) << 32;\n";
      out << "        } else {\n";
      out << "          filePacked = static_cast<uint64_t>(static_cast<uint32_t>(fileFd));\n";
      out << "        }\n";
      out << "        stack[sp++] = filePacked;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileOpenReadDynamic:
    case IrOpcode::FileOpenWriteDynamic:
    case IrOpcode::FileOpenAppendDynamic:
      emitStackUnderflowGuard(1, "file open");
      out << "        uint64_t stringIndex = stack[--sp];\n";
      out << "        if (stringIndex >= ps_string_table_count) {\n";
      out << "          std::cerr << \"invalid string index in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        int fileOpenFlags = ";
      if (instruction.op == IrOpcode::FileOpenWriteDynamic) {
        out << "O_WRONLY | O_CREAT | O_TRUNC";
      } else if (instruction.op == IrOpcode::FileOpenAppendDynamic) {
        out << "O_WRONLY | O_CREAT | O_APPEND";
      } else {
        out << "O_RDONLY";
      }
      out << ";\n";
      out << "        int fileFd = ::open(ps_string_table[stringIndex], fileOpenFlags, 0644);\n";
      out << "        uint64_t filePacked = 0;\n";
      out << "        if (fileFd < 0) {\n";
      out << "          uint32_t fileErr = errno == 0 ? 1u : static_cast<uint32_t>(errno);\n";
      out << "          filePacked = static_cast<uint64_t>(fileErr) << 32;\n";
      out << "        } else {\n";
      out << "          filePacked = static_cast<uint64_t>(static_cast<uint32_t>(fileFd));\n";
      out << "        }\n";
      out << "        stack[sp++] = filePacked;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileClose:
      emitStackUnderflowGuard(1, "file close");
      out << "        uint64_t closeHandle = stack[--sp];\n";
      out << "        int closeFd = static_cast<int>(closeHandle & 0xffffffffu);\n";
      out << "        int closeRc = ::close(closeFd);\n";
      out << "        uint32_t closeErr = (closeRc < 0) ? (errno == 0 ? 1u : static_cast<uint32_t>(errno)) : 0u;\n";
      out << "        stack[sp++] = static_cast<uint64_t>(closeErr);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileFlush:
      emitStackUnderflowGuard(1, "file flush");
      out << "        uint64_t flushHandle = stack[--sp];\n";
      out << "        int flushFd = static_cast<int>(flushHandle & 0xffffffffu);\n";
      out << "        int flushRc = ::fsync(flushFd);\n";
      out << "        uint32_t flushErr = (flushRc < 0) ? (errno == 0 ? 1u : static_cast<uint32_t>(errno)) : 0u;\n";
      out << "        stack[sp++] = static_cast<uint64_t>(flushErr);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileWriteI32:
      emitStackUnderflowGuard(2, "file write");
      out << "        int64_t fileI32Value = static_cast<int64_t>(static_cast<int32_t>(stack[--sp]));\n";
      out << "        uint64_t fileI32Handle = stack[--sp];\n";
      out << "        int fileI32Fd = static_cast<int>(fileI32Handle & 0xffffffffu);\n";
      out << "        std::string fileI32Text = std::to_string(fileI32Value);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileI32Fd, fileI32Text.data(), fileI32Text.size()));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileWriteI64:
      emitStackUnderflowGuard(2, "file write");
      out << "        int64_t fileI64Value = static_cast<int64_t>(stack[--sp]);\n";
      out << "        uint64_t fileI64Handle = stack[--sp];\n";
      out << "        int fileI64Fd = static_cast<int>(fileI64Handle & 0xffffffffu);\n";
      out << "        std::string fileI64Text = std::to_string(fileI64Value);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileI64Fd, fileI64Text.data(), fileI64Text.size()));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileWriteU64:
      emitStackUnderflowGuard(2, "file write");
      out << "        uint64_t fileU64Value = stack[--sp];\n";
      out << "        uint64_t fileU64Handle = stack[--sp];\n";
      out << "        int fileU64Fd = static_cast<int>(fileU64Handle & 0xffffffffu);\n";
      out << "        std::string fileU64Text = std::to_string(static_cast<unsigned long long>(fileU64Value));\n";
      out << "        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileU64Fd, fileU64Text.data(), fileU64Text.size()));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileWriteString:
      if (instruction.imm >= context.stringCount) {
        stringOperandError("FileWriteString", instruction.imm);
        return false;
      }
      emitStackUnderflowGuard(1, "file write");
      out << "        uint64_t fileStringHandle = stack[--sp];\n";
      out << "        int fileStringFd = static_cast<int>(fileStringHandle & 0xffffffffu);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileStringFd, ps_string_table[" << instruction.imm
          << "], " << context.stringLengths[static_cast<size_t>(instruction.imm)] << "ull));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileWriteStringDynamic:
      emitStackUnderflowGuard(2, "file write");
      out << "        uint64_t fileStringDynamicIndex = stack[--sp];\n";
      out << "        if (fileStringDynamicIndex >= ps_string_table_count) {\n";
      out << "          std::cerr << \"invalid string index in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        uint64_t fileStringDynamicHandle = stack[--sp];\n";
      out << "        int fileStringDynamicFd = static_cast<int>(fileStringDynamicHandle & 0xffffffffu);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileStringDynamicFd, "
             "ps_string_table[fileStringDynamicIndex], "
             "std::char_traits<char>::length(ps_string_table[fileStringDynamicIndex])));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileWriteByte:
      emitStackUnderflowGuard(2, "file write");
      out << "        uint8_t fileByteValue = static_cast<uint8_t>(stack[--sp] & 0xffu);\n";
      out << "        uint64_t fileByteHandle = stack[--sp];\n";
      out << "        int fileByteFd = static_cast<int>(fileByteHandle & 0xffffffffu);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileByteFd, &fileByteValue, 1));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileReadByte:
      if (instruction.imm >= localCount) {
        error = "IrToCppEmitter local index out of range at instruction " + std::to_string(index);
        return false;
      }
      emitStackUnderflowGuard(1, "file read");
      out << "        uint64_t fileReadHandle = stack[--sp];\n";
      out << "        int fileReadFd = static_cast<int>(fileReadHandle & 0xffffffffu);\n";
      out << "        uint8_t fileReadByte = 0;\n";
      out << "        ssize_t fileReadRc = ::read(fileReadFd, &fileReadByte, 1);\n";
      out << "        uint32_t fileReadErr = 0;\n";
      out << "        if (fileReadRc < 0) {\n";
      out << "          fileReadErr = errno == 0 ? 1u : static_cast<uint32_t>(errno);\n";
      out << "        } else if (fileReadRc == 0) {\n";
      out << "          fileReadErr = " << FileReadEofCode << "u;\n";
      out << "        } else {\n";
      out << "          locals[" << instruction.imm << "] = static_cast<uint64_t>(fileReadByte);\n";
      out << "        }\n";
      out << "        stack[sp++] = static_cast<uint64_t>(fileReadErr);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::FileWriteNewline:
      emitStackUnderflowGuard(1, "file write");
      out << "        uint64_t fileLineHandle = stack[--sp];\n";
      out << "        int fileLineFd = static_cast<int>(fileLineHandle & 0xffffffffu);\n";
      out << "        char fileLineValue = '\\n';\n";
      out << "        stack[sp++] = static_cast<uint64_t>(psWriteAll(fileLineFd, &fileLineValue, 1));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::LoadStringByte:
      if (instruction.imm >= context.stringCount) {
        stringOperandError("LoadStringByte", instruction.imm);
        return false;
      }
      emitStackUnderflowGuard(1, "string byte");
      out << "        uint64_t stringByteIndex = stack[--sp];\n";
      out << "        if (stringByteIndex >= "
          << context.stringLengths[static_cast<size_t>(instruction.imm)] << "ull) {\n";
      out << "          std::cerr << \"string index out of bounds in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        uint8_t byte = static_cast<uint8_t>(ps_string_table[" << instruction.imm
          << "][stringByteIndex]);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(byte)));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::LoadStringLength:
      emitStackUnderflowGuard(1, "string length");
      out << "        uint64_t stringLengthIndex = stack[--sp];\n";
      out << "        if (stringLengthIndex >= ps_string_table_count) {\n";
      out << "          std::cerr << \"invalid string index in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        stack[sp++] = static_cast<uint64_t>(ps_string_table_lengths[stringLengthIndex]);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    default:
      return false;
  }
}

} // namespace primec
