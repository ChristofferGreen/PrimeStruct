#include "IrToCppEmitterInternal.h"

#include <cstring>
#include <iostream>

namespace primec {
namespace {

std::string irFunctionSymbol(size_t functionIndex) {
  return "ps_fn_" + std::to_string(functionIndex);
}

} // namespace

bool emitInstruction(const IrInstruction &instruction,
                     size_t index,
                     size_t nextIndex,
                     size_t instructionCount,
                     size_t localCount,
                     const EmitContext &context,
                     std::ostringstream &out,
                     std::string &error) {
  const auto emitStackUnderflowGuard = [&](size_t required, const char *operation) {
    out << "        if (!psEnsureStack(sp, " << required << "ull, \"" << operation << "\")) {\n";
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
    emitStackUnderflowGuard(2, "compare");
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
    emitStackUnderflowGuard(2, "compare");
    out << "        int64_t right = static_cast<int64_t>(stack[--sp]);\n";
    out << "        int64_t left = static_cast<int64_t>(stack[--sp]);\n";
    out << "        stack[sp++] = (left " << op << " right) ? 1u : 0u;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitCompareU64 = [&](const char *op) {
    emitStackUnderflowGuard(2, "compare");
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
    emitStackUnderflowGuard(2, "compare");
    out << "        float right = psBitsToF32(stack[--sp]);\n";
    out << "        float left = psBitsToF32(stack[--sp]);\n";
    out << "        stack[sp++] = (left " << op << " right) ? 1u : 0u;\n";
    out << "        pc = " << nextIndex << ";\n";
    out << "        break;\n";
  };
  const auto emitCompareF64 = [&](const char *op) {
    emitStackUnderflowGuard(2, "compare");
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
      emitStackUnderflowGuard(1, "store");
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
      emitStackUnderflowGuard(1, "load indirect");
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
      emitStackUnderflowGuard(2, "store indirect");
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
      emitStackUnderflowGuard(1, "heap alloc");
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
      emitStackUnderflowGuard(1, "heap free");
      out << "        if (!psHeapFree(stack[--sp], heapSlots, heapAllocations)) {\n";
      out << "          std::cerr << \"invalid heap free address in IR\\n\";\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::HeapRealloc:
      emitStackUnderflowGuard(2, "heap realloc");
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
      out << "        if (!psEnsureStack(sp, 1ull, \"dup\")) {\n";
      out << "          return 1;\n";
      out << "        }\n";
      out << "        uint64_t dupValue = stack[sp - 1];\n";
      out << "        stack[sp++] = dupValue;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::Pop:
      out << "        if (!psEnsureStack(sp, 1ull, \"pop\")) {\n";
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
      emitStackUnderflowGuard(1, "negate");
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
      emitStackUnderflowGuard(1, "negate");
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
      emitStackUnderflowGuard(1, "negate");
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
      emitStackUnderflowGuard(1, "negate");
      out << "        double value = psBitsToF64(stack[sp - 1]);\n";
      out << "        stack[sp - 1] = psF64ToBits(-value);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI32ToF32:
      emitStackUnderflowGuard(1, "convert");
      out << "        int32_t value = static_cast<int32_t>(stack[--sp]);\n";
      out << "        stack[sp++] = psF32ToBits(static_cast<float>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI64ToF32:
      emitStackUnderflowGuard(1, "convert");
      out << "        int64_t value = static_cast<int64_t>(stack[--sp]);\n";
      out << "        stack[sp++] = psF32ToBits(static_cast<float>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertU64ToF32:
      emitStackUnderflowGuard(1, "convert");
      out << "        uint64_t value = stack[--sp];\n";
      out << "        stack[sp++] = psF32ToBits(static_cast<float>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToI32:
      emitStackUnderflowGuard(1, "convert");
      out << "        float value = psBitsToF32(stack[--sp]);\n";
      out << "        int32_t converted = psConvertF32ToI32(value);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(static_cast<int64_t>(converted));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToI64:
      emitStackUnderflowGuard(1, "convert");
      out << "        float value = psBitsToF32(stack[--sp]);\n";
      out << "        int64_t converted = psConvertF32ToI64(value);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(converted);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToU64:
      emitStackUnderflowGuard(1, "convert");
      out << "        float value = psBitsToF32(stack[--sp]);\n";
      out << "        uint64_t converted = psConvertF32ToU64(value);\n";
      out << "        stack[sp++] = converted;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI32ToF64:
      emitStackUnderflowGuard(1, "convert");
      out << "        int32_t value = static_cast<int32_t>(stack[--sp]);\n";
      out << "        stack[sp++] = psF64ToBits(static_cast<double>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertI64ToF64:
      emitStackUnderflowGuard(1, "convert");
      out << "        int64_t value = static_cast<int64_t>(stack[--sp]);\n";
      out << "        stack[sp++] = psF64ToBits(static_cast<double>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertU64ToF64:
      emitStackUnderflowGuard(1, "convert");
      out << "        uint64_t value = stack[--sp];\n";
      out << "        stack[sp++] = psF64ToBits(static_cast<double>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToI32:
      emitStackUnderflowGuard(1, "convert");
      out << "        double value = psBitsToF64(stack[--sp]);\n";
      out << "        int32_t converted = psConvertF64ToI32(value);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(static_cast<int64_t>(converted));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToI64:
      emitStackUnderflowGuard(1, "convert");
      out << "        double value = psBitsToF64(stack[--sp]);\n";
      out << "        int64_t converted = psConvertF64ToI64(value);\n";
      out << "        stack[sp++] = static_cast<uint64_t>(converted);\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToU64:
      emitStackUnderflowGuard(1, "convert");
      out << "        double value = psBitsToF64(stack[--sp]);\n";
      out << "        uint64_t converted = psConvertF64ToU64(value);\n";
      out << "        stack[sp++] = converted;\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF32ToF64:
      emitStackUnderflowGuard(1, "convert");
      out << "        float value = psBitsToF32(stack[--sp]);\n";
      out << "        stack[sp++] = psF64ToBits(static_cast<double>(value));\n";
      out << "        pc = " << nextIndex << ";\n";
      out << "        break;\n";
      return true;
    case IrOpcode::ConvertF64ToF32:
      emitStackUnderflowGuard(1, "convert");
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
      emitStackUnderflowGuard(1, "jump");
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
    case IrOpcode::PrintI32:
    case IrOpcode::PrintI64:
    case IrOpcode::PrintU64:
    case IrOpcode::PrintString:
    case IrOpcode::PrintStringDynamic:
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe:
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
    case IrOpcode::FileOpenReadDynamic:
    case IrOpcode::FileOpenWriteDynamic:
    case IrOpcode::FileOpenAppendDynamic:
    case IrOpcode::FileClose:
    case IrOpcode::FileFlush:
    case IrOpcode::FileWriteI32:
    case IrOpcode::FileWriteI64:
    case IrOpcode::FileWriteU64:
    case IrOpcode::FileWriteString:
    case IrOpcode::FileWriteStringDynamic:
    case IrOpcode::FileWriteByte:
    case IrOpcode::FileReadByte:
    case IrOpcode::FileWriteNewline:
    case IrOpcode::LoadStringByte:
    case IrOpcode::LoadStringLength:
      return emitPrintAndFileInstruction(instruction, index, nextIndex, localCount, context, out, error);
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


} // namespace primec
