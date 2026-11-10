#pragma once

#include "primec/ir/Ir.h"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace primec {

struct EmitContext {
  size_t stringCount = 0;
  size_t functionCount = 0;
  std::vector<size_t> stringLengths;
  // The module's `[host]` imports, for IrOpcode::CallHost (IrToCppHostImports.h).
  const std::vector<IrHostImport> *hostImports = nullptr;
};

// The integer arithmetic and compare instructions run through one shared helper each (emitted once per program by
// emitIntegerOpHelpers), so an instruction is a call instead of nine lines: struct-heavy programs emit fewer bytes
// and clang compiles them faster.
struct CppIntegerOpHelper {
  const char *name;        // the emitted function's name
  const char *operandType; // int32_t, int64_t or uint64_t
  const char *op;          // the C++ operator
  const char *operation;   // the stack-underflow message
  bool compare;            // pushes 1u/0u instead of the result
};

inline constexpr CppIntegerOpHelper CppIntegerOpHelpers[] = {
    {"psI32Add", "int32_t", "+", "add", false},    {"psI32Sub", "int32_t", "-", "sub", false},
    {"psI32Mul", "int32_t", "*", "mul", false},    {"psI32Div", "int32_t", "/", "div", false},
    {"psI64Add", "int64_t", "+", "add", false},    {"psI64Sub", "int64_t", "-", "sub", false},
    {"psI64Mul", "int64_t", "*", "mul", false},    {"psI64Div", "int64_t", "/", "div", false},
    {"psI32Eq", "int32_t", "==", "compare", true}, {"psI32Ne", "int32_t", "!=", "compare", true},
    {"psI32Lt", "int32_t", "<", "compare", true},  {"psI32Le", "int32_t", "<=", "compare", true},
    {"psI32Gt", "int32_t", ">", "compare", true},  {"psI32Ge", "int32_t", ">=", "compare", true},
    {"psI64Eq", "int64_t", "==", "compare", true}, {"psI64Ne", "int64_t", "!=", "compare", true},
    {"psI64Lt", "int64_t", "<", "compare", true},  {"psI64Le", "int64_t", "<=", "compare", true},
    {"psI64Gt", "int64_t", ">", "compare", true},  {"psI64Ge", "int64_t", ">=", "compare", true},
    {"psU64Lt", "uint64_t", "<", "compare", true}, {"psU64Le", "uint64_t", "<=", "compare", true},
    {"psU64Gt", "uint64_t", ">", "compare", true}, {"psU64Ge", "uint64_t", ">=", "compare", true},
};

void emitIntegerOpHelpers(std::ostringstream &out);
// Emits `if (!helper(stack, sp)) return 1;` and the step to the next instruction; false when no helper matches.
bool emitIntegerOpHelperCall(const char *operandType,
                             const char *op,
                             bool compare,
                             size_t nextIndex,
                             std::ostringstream &out);

bool emitInstruction(const IrInstruction &instruction,
                     size_t index,
                     size_t nextIndex,
                     size_t instructionCount,
                     size_t localCount,
                     const EmitContext &context,
                     std::ostringstream &out,
                     std::string &error);

bool emitPrintAndFileInstruction(const IrInstruction &instruction,
                                 size_t index,
                                 size_t nextIndex,
                                 size_t localCount,
                                 const EmitContext &context,
                                 std::ostringstream &out,
                                 std::string &error);

} // namespace primec
