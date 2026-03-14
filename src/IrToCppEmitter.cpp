#include "primec/IrToCppEmitter.h"

#include "primec/Ir.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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
  std::vector<size_t> stringLengths;
};

size_t computeLocalCount(const IrFunction &function) {
  size_t localCount = 0;
  for (const IrInstruction &instruction : function.instructions) {
    switch (instruction.op) {
      case IrOpcode::LoadLocal:
      case IrOpcode::StoreLocal:
      case IrOpcode::AddressOfLocal:
      case IrOpcode::FileReadByte:
        localCount = std::max(localCount, static_cast<size_t>(instruction.imm) + 1);
        break;
      default:
        break;
    }
  }
  return localCount;
}

bool usesF32Helpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32:
    case IrOpcode::NegF32:
    case IrOpcode::CmpEqF32:
    case IrOpcode::CmpNeF32:
    case IrOpcode::CmpLtF32:
    case IrOpcode::CmpLeF32:
    case IrOpcode::CmpGtF32:
    case IrOpcode::CmpGeF32:
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertU64ToF32:
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF32ToU64:
    case IrOpcode::ConvertF32ToF64:
    case IrOpcode::ConvertF64ToF32:
      return true;
    default:
      return false;
  }
}

bool moduleUsesF32Helpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesF32Helpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

bool usesF64Helpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64:
    case IrOpcode::NegF64:
    case IrOpcode::CmpEqF64:
    case IrOpcode::CmpNeF64:
    case IrOpcode::CmpLtF64:
    case IrOpcode::CmpLeF64:
    case IrOpcode::CmpGtF64:
    case IrOpcode::CmpGeF64:
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF64:
    case IrOpcode::ConvertF64ToI32:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF64ToU64:
    case IrOpcode::ConvertF32ToF64:
    case IrOpcode::ConvertF64ToF32:
      return true;
    default:
      return false;
  }
}

bool moduleUsesF64Helpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesF64Helpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

bool usesClampI32ConvertHelpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF64ToI32:
      return true;
    default:
      return false;
  }
}

bool moduleUsesClampI32ConvertHelpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesClampI32ConvertHelpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

bool usesClampI64ConvertHelpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF64ToI64:
      return true;
    default:
      return false;
  }
}

bool moduleUsesClampI64ConvertHelpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesClampI64ConvertHelpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

bool usesClampU64ConvertHelpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::ConvertF32ToU64:
    case IrOpcode::ConvertF64ToU64:
      return true;
    default:
      return false;
  }
}

bool moduleUsesClampU64ConvertHelpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesClampU64ConvertHelpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

bool usesFileIoHelpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
    case IrOpcode::FileReadByte:
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

bool moduleUsesFileIoHelpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesFileIoHelpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

std::string irFunctionSymbol(size_t functionIndex) {
  return "ps_fn_" + std::to_string(functionIndex);
}

bool emitInstruction(const IrInstruction &instruction,
                     size_t index,
                     size_t nextIndex,
                     size_t instructionCount,
                     size_t localCount,
                     const EmitContext &context,
                     std::ostringstream &out,
                     std::string &error) {
  const auto emitStackUnderflowGuard = [&](size_t required, const char *operation) {
    if (required == 1) {
      out << "        if (sp == 0) {\n";
    } else {
      out << "        if (sp < " << required << ") {\n";
    }
    out << "          std::cerr << \"IR stack underflow on " << operation << "\\n\";\n";
    out << "          return 1;\n";
    out << "        }\n";
  };
  const auto emitBinaryI32 = [&](const char *op, const char *underflowOperation) {
    emitStackUnderflowGuard(2, underflowOperation);
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
  const auto emitBinaryI64 = [&](const char *op, const char *underflowOperation) {
    emitStackUnderflowGuard(2, underflowOperation);
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
    out << "        stack[sp++] = (left " << op << " right) ? 1u : 0u;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitBinaryF32 = [&](const char *op, const char *underflowOperation) {
    emitStackUnderflowGuard(2, underflowOperation);
    out << "        float right = psBitsToF32(stack[--sp]);\n";
    out << "        float left = psBitsToF32(stack[--sp]);\n";
    out << "        stack[sp++] = psF32ToBits(left " << op << " right);\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitCompareF32 = [&](const char *op) {
    out << "        float right = psBitsToF32(stack[--sp]);\n";
    out << "        float left = psBitsToF32(stack[--sp]);\n";
    out << "        stack[sp++] = (left " << op << " right) ? 1u : 0u;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitCompareF64 = [&](const char *op) {
    out << "        double right = psBitsToF64(stack[--sp]);\n";
    out << "        double left = psBitsToF64(stack[--sp]);\n";
    out << "        stack[sp++] = (left " << op << " right) ? 1u : 0u;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitBinaryF64 = [&](const char *op, const char *underflowOperation) {
    emitStackUnderflowGuard(2, underflowOperation);
    out << "        double right = psBitsToF64(stack[--sp]);\n";
    out << "        double left = psBitsToF64(stack[--sp]);\n";
    out << "        stack[sp++] = psF64ToBits(left " << op << " right);\n";
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
    case IrOpcode::PushF32: {
      const uint32_t bits = static_cast<uint32_t>(instruction.imm);
      out << "        stack[sp++] = static_cast<uint64_t>(" << bits << "u);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    }
    case IrOpcode::PushF64:
      out << "        stack[sp++] = static_cast<uint64_t>(" << instruction.imm << "ull);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::LoadLocal:
      if (instruction.imm >= localCount) {
        error = "IrToCppEmitter local index out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        stack[sp++] = locals[" << instruction.imm << "];\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::StoreLocal:
      if (instruction.imm >= localCount) {
        error = "IrToCppEmitter local index out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        locals[" << instruction.imm << "] = stack[--sp];\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::AddressOfLocal:
      if (instruction.imm >= localCount) {
        error = "IrToCppEmitter local index out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        stack[sp++] = static_cast<uint64_t>(" << instruction.imm << "ull * " << IrSlotBytes
          << "ull);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::LoadIndirect:
      out << "        uint64_t loadIndirectAddress = stack[--sp];\n";
      out << "        if ((loadIndirectAddress % " << IrSlotBytes << "ull) != 0ull) {\n";
      out << "          std::cerr << \"unaligned indirect address in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        if ((loadIndirectAddress & ps_heap_address_tag) != 0ull) {\n";
      out << "          std::size_t loadHeapIndex = 0;\n";
      out << "          if (!psResolveHeapSlot(loadIndirectAddress, heapSlots, heapAllocations, loadHeapIndex)) {\n";
      out << "            std::cerr << \"invalid indirect address in IR\\n\";\n";
      out << "            return 1;\n";
      out << "          }\n";
      out << "          stack[sp++] = heapSlots[loadHeapIndex];\n";
      out << "        } else {\n";
      out << "          uint64_t loadIndirectIndex = loadIndirectAddress / " << IrSlotBytes << "ull;\n";
      out << "          if (loadIndirectIndex >= " << localCount << "ull) {\n";
      out << "            std::cerr << \"invalid indirect address in IR\\n\";\n";
      out << "            return 1;\n";
      out << "          }\n";
      out << "          stack[sp++] = locals[loadIndirectIndex];\n";
      out << "        }\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::StoreIndirect:
      out << "        uint64_t storeIndirectValue = stack[--sp];\n";
      out << "        uint64_t storeIndirectAddress = stack[--sp];\n";
      out << "        if ((storeIndirectAddress % " << IrSlotBytes << "ull) != 0ull) {\n";
      out << "          std::cerr << \"unaligned indirect address in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        if ((storeIndirectAddress & ps_heap_address_tag) != 0ull) {\n";
      out << "          std::size_t storeHeapIndex = 0;\n";
      out << "          if (!psResolveHeapSlot(storeIndirectAddress, heapSlots, heapAllocations, storeHeapIndex)) {\n";
      out << "            std::cerr << \"invalid indirect address in IR\\n\";\n";
      out << "            return 1;\n";
      out << "          }\n";
      out << "          heapSlots[storeHeapIndex] = storeIndirectValue;\n";
      out << "        } else {\n";
      out << "          uint64_t storeIndirectIndex = storeIndirectAddress / " << IrSlotBytes << "ull;\n";
      out << "          if (storeIndirectIndex >= " << localCount << "ull) {\n";
      out << "            std::cerr << \"invalid indirect address in IR\\n\";\n";
      out << "            return 1;\n";
      out << "          }\n";
      out << "          locals[storeIndirectIndex] = storeIndirectValue;\n";
      out << "        }\n";
      out << "        stack[sp++] = storeIndirectValue;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::HeapAlloc:
      out << "        uint64_t heapAllocSlotCount = stack[--sp];\n";
      out << "        uint64_t heapAllocAddress = 0ull;\n";
      out << "        if (!psHeapAlloc(heapAllocSlotCount, heapSlots, heapAllocations, heapAllocAddress)) {\n";
      out << "          std::cerr << \"VM heap allocation overflow\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        stack[sp++] = heapAllocAddress;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::HeapFree:
      out << "        if (!psHeapFree(stack[--sp], heapSlots, heapAllocations)) {\n";
      out << "          std::cerr << \"invalid heap free address in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::HeapRealloc:
      out << "        uint64_t heapReallocSlotCount = stack[--sp];\n";
      out << "        uint64_t heapReallocAddress = stack[--sp];\n";
      out << "        uint64_t heapReallocResult = 0ull;\n";
      out << "        if (!psHeapRealloc(heapReallocAddress, heapReallocSlotCount, heapSlots, heapAllocations, "
             "heapReallocResult)) {\n";
      out << "          std::cerr << \"invalid heap realloc address in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        stack[sp++] = heapReallocResult;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::Dup:
      out << "        if (sp == 0) {\n";
      out << "          std::cerr << \"IR stack underflow on dup\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        stack[sp] = stack[sp - 1];\n";
      out << "        ++sp;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::Pop:
      out << "        if (sp == 0) {\n";
      out << "          std::cerr << \"IR stack underflow on pop\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        --sp;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::AddI32:
      emitBinaryI32("+", "add");
      return true;
    case IrOpcode::SubI32:
      emitBinaryI32("-", "sub");
      return true;
    case IrOpcode::MulI32:
      emitBinaryI32("*", "mul");
      return true;
    case IrOpcode::DivI32:
      emitBinaryI32("/", "div");
      return true;
    case IrOpcode::NegI32:
      out << "        int32_t value = static_cast<int32_t>(stack[sp - 1]);\n";
      out << "        stack[sp - 1] = static_cast<uint64_t>(-value);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::AddI64:
      emitBinaryI64("+", "add");
      return true;
    case IrOpcode::SubI64:
      emitBinaryI64("-", "sub");
      return true;
    case IrOpcode::MulI64:
      emitBinaryI64("*", "mul");
      return true;
    case IrOpcode::DivI64:
      emitBinaryI64("/", "div");
      return true;
    case IrOpcode::DivU64:
      emitStackUnderflowGuard(2, "div");
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
    case IrOpcode::AddF32:
      emitBinaryF32("+", "float op");
      return true;
    case IrOpcode::SubF32:
      emitBinaryF32("-", "float op");
      return true;
    case IrOpcode::MulF32:
      emitBinaryF32("*", "float op");
      return true;
    case IrOpcode::DivF32:
      emitBinaryF32("/", "float op");
      return true;
    case IrOpcode::NegF32:
      out << "        float value = psBitsToF32(stack[sp - 1]);\n";
      out << "        stack[sp - 1] = psF32ToBits(-value);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
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
      emitCompareF64("==");
      return true;
    case IrOpcode::CmpNeF64:
      emitCompareF64("!=");
      return true;
    case IrOpcode::CmpLtF64:
      emitCompareF64("<");
      return true;
    case IrOpcode::CmpLeF64:
      emitCompareF64("<=");
      return true;
    case IrOpcode::CmpGtF64:
      emitCompareF64(">");
      return true;
    case IrOpcode::CmpGeF64:
      emitCompareF64(">=");
      return true;
    case IrOpcode::AddF64:
      emitBinaryF64("+", "float op");
      return true;
    case IrOpcode::SubF64:
      emitBinaryF64("-", "float op");
      return true;
    case IrOpcode::MulF64:
      emitBinaryF64("*", "float op");
      return true;
    case IrOpcode::DivF64:
      emitBinaryF64("/", "float op");
      return true;
    case IrOpcode::NegF64:
      out << "        double value = psBitsToF64(stack[sp - 1]);\n";
      out << "        stack[sp - 1] = psF64ToBits(-value);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI32ToF32:
      out << "        int32_t value = static_cast<int32_t>(stack[--sp]);\n";
      out << "        stack[sp++] = psF32ToBits(static_cast<float>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI64ToF32:
      out << "        int64_t value = static_cast<int64_t>(stack[--sp]);\n";
      out << "        stack[sp++] = psF32ToBits(static_cast<float>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertU64ToF32:
      out << "        uint64_t value = stack[--sp];\n";
      out << "        stack[sp++] = psF32ToBits(static_cast<float>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToI32:
      out << "        float value = psBitsToF32(stack[--sp]);\n";
      out << "        int32_t converted = psConvertF32ToI32(value);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(static_cast<int64_t>(converted));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToI64:
      out << "        float value = psBitsToF32(stack[--sp]);\n";
      out << "        int64_t converted = psConvertF32ToI64(value);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(converted);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToU64:
      out << "        float value = psBitsToF32(stack[--sp]);\n";
      out << "        uint64_t converted = psConvertF32ToU64(value);\n";
      out << "        stack[sp++] = converted;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI32ToF64:
      out << "        int32_t value = static_cast<int32_t>(stack[--sp]);\n";
      out << "        stack[sp++] = psF64ToBits(static_cast<double>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI64ToF64:
      out << "        int64_t value = static_cast<int64_t>(stack[--sp]);\n";
      out << "        stack[sp++] = psF64ToBits(static_cast<double>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertU64ToF64:
      out << "        uint64_t value = stack[--sp];\n";
      out << "        stack[sp++] = psF64ToBits(static_cast<double>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToI32:
      out << "        double value = psBitsToF64(stack[--sp]);\n";
      out << "        int32_t converted = psConvertF64ToI32(value);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(static_cast<int64_t>(converted));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToI64:
      out << "        double value = psBitsToF64(stack[--sp]);\n";
      out << "        int64_t converted = psConvertF64ToI64(value);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(converted);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToU64:
      out << "        double value = psBitsToF64(stack[--sp]);\n";
      out << "        uint64_t converted = psConvertF64ToU64(value);\n";
      out << "        stack[sp++] = converted;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToF64:
      out << "        float value = psBitsToF32(stack[--sp]);\n";
      out << "        stack[sp++] = psF64ToBits(static_cast<double>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToF32:
      out << "        double value = psBitsToF64(stack[--sp]);\n";
      out << "        stack[sp++] = psF32ToBits(static_cast<float>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::Jump:
      if (instruction.imm > instructionCount) {
        error = "IrToCppEmitter jump target out of range at instruction " + std::to_string(index);
        return false;
      }
      out << "        pc = " << instruction.imm << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::JumpIfZero:
      if (instruction.imm > instructionCount) {
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
            << "(stack, sp, heapSlots, heapAllocations, argc, argv);\n";
        out << "        stack[sp++] = static_cast<uint64_t>(callValue);\n";
      } else {
        out << "        " << irFunctionSymbol(static_cast<size_t>(instruction.imm))
            << "(stack, sp, heapSlots, heapAllocations, argc, argv);\n";
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
        error = "IrToCppEmitter string index out of range at instruction " + std::to_string(index);
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
        error = "IrToCppEmitter string index out of range at instruction " + std::to_string(index);
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
        error = "IrToCppEmitter string index out of range at instruction " + std::to_string(index);
        return false;
      }
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
    case IrOpcode::ReturnI32:
      emitStackUnderflowGuard(1, "return");
      out << "        return static_cast<int64_t>(static_cast<int32_t>(stack[--sp]));\n";
      return true;
    case IrOpcode::ReturnI64:
      emitStackUnderflowGuard(1, "return");
      out << "        return static_cast<int64_t>(stack[--sp]);\n";
      return true;
    case IrOpcode::ReturnF32:
      emitStackUnderflowGuard(1, "return");
      out << "        return static_cast<int64_t>(static_cast<uint32_t>(stack[--sp]));\n";
      return true;
    case IrOpcode::ReturnF64:
      emitStackUnderflowGuard(1, "return");
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
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    if (module.functions[functionIndex].instructions.empty()) {
      error = "IrToCppEmitter function has no instructions at index " + std::to_string(functionIndex);
      return false;
    }
  }

  std::ostringstream body;
  const bool needsF32Helpers = moduleUsesF32Helpers(module);
  const bool needsF64Helpers = moduleUsesF64Helpers(module);
  const bool needsClampI32ConvertHelpers = moduleUsesClampI32ConvertHelpers(module);
  const bool needsClampI64ConvertHelpers = moduleUsesClampI64ConvertHelpers(module);
  const bool needsClampU64ConvertHelpers = moduleUsesClampU64ConvertHelpers(module);
  const bool needsFileIoHelpers = moduleUsesFileIoHelpers(module);
  body << "#include <algorithm>\n";
  body << "#include <cstddef>\n";
  body << "#include <cstdint>\n";
  body << "#include <limits>\n";
  body << "#include <string>\n";
  body << "#include <vector>\n";
  if (needsClampI32ConvertHelpers || needsClampI64ConvertHelpers || needsClampU64ConvertHelpers) {
    body << "#include <cmath>\n";
  }
  if (needsF32Helpers || needsF64Helpers) {
    body << "#include <cstring>\n";
  }
  if (needsFileIoHelpers) {
    body << "#include <cerrno>\n";
    body << "#include <fcntl.h>\n";
    body << "#include <unistd.h>\n";
  }
  body << "\n";
  body << "#include <iostream>\n\n";
  if (needsF32Helpers) {
    body << "static float psBitsToF32(uint64_t raw) {\n";
    body << "  uint32_t bits = static_cast<uint32_t>(raw);\n";
    body << "  float value = 0.0f;\n";
    body << "  std::memcpy(&value, &bits, sizeof(value));\n";
    body << "  return value;\n";
    body << "}\n\n";
    body << "static uint64_t psF32ToBits(float value) {\n";
    body << "  uint32_t bits = 0;\n";
    body << "  std::memcpy(&bits, &value, sizeof(bits));\n";
    body << "  return static_cast<uint64_t>(bits);\n";
    body << "}\n\n";
  }
  if (needsF64Helpers) {
    body << "static double psBitsToF64(uint64_t raw) {\n";
    body << "  double value = 0.0;\n";
    body << "  std::memcpy(&value, &raw, sizeof(value));\n";
    body << "  return value;\n";
    body << "}\n\n";
    body << "static uint64_t psF64ToBits(double value) {\n";
    body << "  uint64_t bits = 0;\n";
    body << "  std::memcpy(&bits, &value, sizeof(bits));\n";
    body << "  return bits;\n";
    body << "}\n\n";
  }
  if (needsClampI32ConvertHelpers) {
    body << "static int32_t psConvertF32ToI32(float value) {\n";
    body << "  if (std::isnan(value)) {\n";
    body << "    return 0;\n";
    body << "  }\n";
    body << "  if (value <= static_cast<float>(std::numeric_limits<int32_t>::min())) {\n";
    body << "    return std::numeric_limits<int32_t>::min();\n";
    body << "  }\n";
    body << "  if (value >= static_cast<float>(std::numeric_limits<int32_t>::max())) {\n";
    body << "    return std::numeric_limits<int32_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<int32_t>(value);\n";
    body << "}\n\n";
    body << "static int32_t psConvertF64ToI32(double value) {\n";
    body << "  if (std::isnan(value)) {\n";
    body << "    return 0;\n";
    body << "  }\n";
    body << "  if (value <= static_cast<double>(std::numeric_limits<int32_t>::min())) {\n";
    body << "    return std::numeric_limits<int32_t>::min();\n";
    body << "  }\n";
    body << "  if (value >= static_cast<double>(std::numeric_limits<int32_t>::max())) {\n";
    body << "    return std::numeric_limits<int32_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<int32_t>(value);\n";
    body << "}\n\n";
  }
  if (needsClampI64ConvertHelpers) {
    body << "static int64_t psConvertF32ToI64(float value) {\n";
    body << "  if (std::isnan(value)) {\n";
    body << "    return 0;\n";
    body << "  }\n";
    body << "  if (value <= static_cast<float>(std::numeric_limits<int64_t>::min())) {\n";
    body << "    return std::numeric_limits<int64_t>::min();\n";
    body << "  }\n";
    body << "  if (value >= static_cast<float>(std::numeric_limits<int64_t>::max())) {\n";
    body << "    return std::numeric_limits<int64_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<int64_t>(value);\n";
    body << "}\n\n";
    body << "static int64_t psConvertF64ToI64(double value) {\n";
    body << "  if (std::isnan(value)) {\n";
    body << "    return 0;\n";
    body << "  }\n";
    body << "  if (value <= static_cast<double>(std::numeric_limits<int64_t>::min())) {\n";
    body << "    return std::numeric_limits<int64_t>::min();\n";
    body << "  }\n";
    body << "  if (value >= static_cast<double>(std::numeric_limits<int64_t>::max())) {\n";
    body << "    return std::numeric_limits<int64_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<int64_t>(value);\n";
    body << "}\n\n";
  }
  if (needsClampU64ConvertHelpers) {
    body << "static uint64_t psConvertF32ToU64(float value) {\n";
    body << "  if (std::isnan(value) || value <= 0.0f) {\n";
    body << "    return 0u;\n";
    body << "  }\n";
    body << "  if (value >= static_cast<float>(std::numeric_limits<uint64_t>::max())) {\n";
    body << "    return std::numeric_limits<uint64_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<uint64_t>(value);\n";
    body << "}\n\n";
    body << "static uint64_t psConvertF64ToU64(double value) {\n";
    body << "  if (std::isnan(value) || value <= 0.0) {\n";
    body << "    return 0u;\n";
    body << "  }\n";
    body << "  if (value >= static_cast<double>(std::numeric_limits<uint64_t>::max())) {\n";
    body << "    return std::numeric_limits<uint64_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<uint64_t>(value);\n";
    body << "}\n\n";
  }
  if (needsFileIoHelpers) {
    body << "static uint32_t psWriteAll(int fd, const void *data, std::size_t size) {\n";
    body << "  const char *cursor = static_cast<const char *>(data);\n";
    body << "  std::size_t remaining = size;\n";
    body << "  while (remaining > 0) {\n";
    body << "    ssize_t wrote = ::write(fd, cursor, remaining);\n";
    body << "    if (wrote < 0) {\n";
    body << "      return errno == 0 ? 1u : static_cast<uint32_t>(errno);\n";
    body << "    }\n";
    body << "    if (wrote == 0) {\n";
    body << "      return 5u;\n";
    body << "    }\n";
    body << "    remaining -= static_cast<std::size_t>(wrote);\n";
    body << "    cursor += wrote;\n";
    body << "  }\n";
    body << "  return 0u;\n";
    body << "}\n\n";
  }
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
  body << "static constexpr std::size_t ps_string_table_lengths[] = {\n";
  if (module.stringTable.empty()) {
    body << "  0u,\n";
  } else {
    for (const std::string &text : module.stringTable) {
      body << "  " << text.size() << "u,\n";
    }
  }
  body << "};\n\n";
  body << "static constexpr uint64_t ps_heap_address_tag = 1ull << 63;\n\n";
  body << "struct PsHeapAllocation {\n";
  body << "  std::size_t baseIndex = 0;\n";
  body << "  std::size_t slotCount = 0;\n";
  body << "  bool live = false;\n";
  body << "};\n\n";
  body << "static bool psResolveHeapSlot(uint64_t address,\n";
  body << "                              const std::vector<uint64_t> &heapSlots,\n";
  body << "                              const std::vector<PsHeapAllocation> &heapAllocations,\n";
  body << "                              std::size_t &slotIndexOut) {\n";
  body << "  if ((address & ps_heap_address_tag) == 0ull || (address % " << IrSlotBytes << "ull) != 0ull) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  uint64_t heapAddress = address & ~ps_heap_address_tag;\n";
  body << "  uint64_t heapIndex = heapAddress / " << IrSlotBytes << "ull;\n";
  body << "  if (heapIndex >= heapSlots.size()) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  for (const auto &allocation : heapAllocations) {\n";
  body << "    if (!allocation.live) {\n";
  body << "      continue;\n";
  body << "    }\n";
  body << "    uint64_t baseIndex = static_cast<uint64_t>(allocation.baseIndex);\n";
  body << "    uint64_t endIndex = baseIndex + static_cast<uint64_t>(allocation.slotCount);\n";
  body << "    if (heapIndex >= baseIndex && heapIndex < endIndex) {\n";
  body << "      slotIndexOut = static_cast<std::size_t>(heapIndex);\n";
  body << "      return true;\n";
  body << "    }\n";
  body << "  }\n";
  body << "  return false;\n";
  body << "}\n\n";
  body << "static bool psHeapAlloc(uint64_t slotCount,\n";
  body << "                        std::vector<uint64_t> &heapSlots,\n";
  body << "                        std::vector<PsHeapAllocation> &heapAllocations,\n";
  body << "                        uint64_t &addressOut) {\n";
  body << "  if (slotCount == 0ull) {\n";
  body << "    addressOut = 0ull;\n";
  body << "    return true;\n";
  body << "  }\n";
  body << "  if (slotCount > static_cast<uint64_t>(heapSlots.max_size() - heapSlots.size())) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  std::size_t baseIndex = heapSlots.size();\n";
  body << "  uint64_t maxAddressableIndex = (std::numeric_limits<uint64_t>::max() - ps_heap_address_tag) / "
       << IrSlotBytes << "ull;\n";
  body << "  if (baseIndex > maxAddressableIndex) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  heapSlots.resize(baseIndex + static_cast<std::size_t>(slotCount), 0ull);\n";
  body << "  heapAllocations.push_back({baseIndex, static_cast<std::size_t>(slotCount), true});\n";
  body << "  addressOut = ps_heap_address_tag + static_cast<uint64_t>(baseIndex) * " << IrSlotBytes << "ull;\n";
  body << "  return true;\n";
  body << "}\n\n";
  body << "static bool psHeapFree(uint64_t address,\n";
  body << "                       std::vector<uint64_t> &heapSlots,\n";
  body << "                       std::vector<PsHeapAllocation> &heapAllocations) {\n";
  body << "  if (address == 0ull) {\n";
  body << "    return true;\n";
  body << "  }\n";
  body << "  if ((address & ps_heap_address_tag) == 0ull || (address % " << IrSlotBytes << "ull) != 0ull) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  uint64_t heapAddress = address & ~ps_heap_address_tag;\n";
  body << "  std::size_t baseIndex = static_cast<std::size_t>(heapAddress / " << IrSlotBytes << "ull);\n";
  body << "  for (auto &allocation : heapAllocations) {\n";
  body << "    if (allocation.baseIndex != baseIndex) {\n";
  body << "      continue;\n";
  body << "    }\n";
  body << "    if (!allocation.live || allocation.baseIndex + allocation.slotCount > heapSlots.size()) {\n";
  body << "      return false;\n";
  body << "    }\n";
  body << "    for (std::size_t i = 0; i < allocation.slotCount; ++i) {\n";
  body << "      heapSlots[allocation.baseIndex + i] = 0ull;\n";
  body << "    }\n";
  body << "    allocation.live = false;\n";
  body << "    return true;\n";
  body << "  }\n";
  body << "  return false;\n";
  body << "}\n\n";
  body << "static bool psHeapRealloc(uint64_t address,\n";
  body << "                          uint64_t slotCount,\n";
  body << "                          std::vector<uint64_t> &heapSlots,\n";
  body << "                          std::vector<PsHeapAllocation> &heapAllocations,\n";
  body << "                          uint64_t &addressOut) {\n";
  body << "  if (address == 0ull) {\n";
  body << "    return psHeapAlloc(slotCount, heapSlots, heapAllocations, addressOut);\n";
  body << "  }\n";
  body << "  if (slotCount == 0ull) {\n";
  body << "    if (!psHeapFree(address, heapSlots, heapAllocations)) {\n";
  body << "      return false;\n";
  body << "    }\n";
  body << "    addressOut = 0ull;\n";
  body << "    return true;\n";
  body << "  }\n";
  body << "  if ((address & ps_heap_address_tag) == 0ull || (address % " << IrSlotBytes << "ull) != 0ull) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  uint64_t heapAddress = address & ~ps_heap_address_tag;\n";
  body << "  std::size_t baseIndex = static_cast<std::size_t>(heapAddress / " << IrSlotBytes << "ull);\n";
  body << "  for (auto &allocation : heapAllocations) {\n";
  body << "    if (allocation.baseIndex != baseIndex) {\n";
  body << "      continue;\n";
  body << "    }\n";
  body << "    if (!allocation.live || allocation.baseIndex + allocation.slotCount > heapSlots.size()) {\n";
  body << "      return false;\n";
  body << "    }\n";
  body << "    std::size_t oldBaseIndex = allocation.baseIndex;\n";
  body << "    std::size_t oldSlotCount = allocation.slotCount;\n";
  body << "    uint64_t newAddress = 0ull;\n";
  body << "    if (!psHeapAlloc(slotCount, heapSlots, heapAllocations, newAddress)) {\n";
  body << "      addressOut = 0ull;\n";
  body << "      return true;\n";
  body << "    }\n";
  body << "    std::size_t newBaseIndex = static_cast<std::size_t>((newAddress & ~ps_heap_address_tag) / "
       << IrSlotBytes << "ull);\n";
  body << "    std::size_t copySlots = std::min(oldSlotCount, static_cast<std::size_t>(slotCount));\n";
  body << "    for (std::size_t i = 0; i < copySlots; ++i) {\n";
  body << "      heapSlots[newBaseIndex + i] = heapSlots[oldBaseIndex + i];\n";
  body << "    }\n";
  body << "    for (std::size_t i = 0; i < oldSlotCount; ++i) {\n";
  body << "      heapSlots[oldBaseIndex + i] = 0ull;\n";
  body << "    }\n";
  body << "    for (auto &candidate : heapAllocations) {\n";
  body << "      if (candidate.baseIndex == oldBaseIndex) {\n";
  body << "        candidate.live = false;\n";
  body << "        break;\n";
  body << "      }\n";
  body << "    }\n";
  body << "    addressOut = newAddress;\n";
  body << "    return true;\n";
  body << "  }\n";
  body << "  return false;\n";
  body << "}\n\n";

  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    body << "static int64_t " << irFunctionSymbol(functionIndex)
         << "(uint64_t *stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, "
            "std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv);\n";
  }
  body << "\n";

  std::vector<size_t> stringLengths;
  stringLengths.reserve(module.stringTable.size());
  for (const std::string &text : module.stringTable) {
    stringLengths.push_back(text.size());
  }
  const EmitContext context{
      .stringCount = module.stringTable.size(),
      .functionCount = module.functions.size(),
      .stringLengths = std::move(stringLengths),
  };
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const IrFunction &function = module.functions[functionIndex];
    const size_t localCount = computeLocalCount(function);
    body << "static int64_t " << irFunctionSymbol(functionIndex)
         << "(uint64_t *stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, "
            "std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv) {\n";
    body << "  std::vector<uint64_t> locals(" << localCount << "ull, 0ull);\n";
    body << "  std::size_t pc = 0;\n";
    body << "  while (true) {\n";
    body << "    switch (pc) {\n";

    const size_t instructionCount = function.instructions.size();
    for (size_t i = 0; i < instructionCount; ++i) {
      const size_t next = i + 1;
      body << "      case " << i << ": {\n";
      if (!emitInstruction(function.instructions[i], i, next, instructionCount, localCount, context, body, error)) {
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
  body << "  std::vector<uint64_t> heapSlots;\n";
  body << "  std::vector<PsHeapAllocation> heapAllocations;\n";
  body << "  return " << irFunctionSymbol(static_cast<size_t>(module.entryIndex))
       << "(stack, sp, heapSlots, heapAllocations, argc, argv);\n";
  body << "}\n\n";
  body << "int main(int argc, char **argv) {\n";
  body << "  return static_cast<int>(ps_entry_" << static_cast<size_t>(module.entryIndex) << "(argc, argv));\n";
  body << "}\n";

  out = body.str();
  return true;
}

} // namespace primec
