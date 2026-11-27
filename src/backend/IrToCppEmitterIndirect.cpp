#include "IrToCppEmitterInternal.h"

#include <set>
#include <string>

namespace primec {

namespace {

std::string cppStringLiteral(const std::string &text) {
  std::string out = "\"";
  for (const char c : text) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

} // namespace

std::string cppIndirectCallSymbol(uint64_t parameterCount) {
  return "psCallIndirect" + std::to_string(parameterCount);
}

// One function per parameter count an indirect call passes (docs/spec/vm-design.md, Function addresses): it checks the
// address with the VM's three faults, removes the address from under the parameters and calls the function it names,
// a switch over every function that takes that many parameters. The generated functions follow the declarations of the
// IR functions.
void emitIndirectCallHelpers(std::ostringstream &out, const IrModule &module) {
  std::set<uint64_t> parameterCounts;
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (instruction.op == IrOpcode::CallIndirect || instruction.op == IrOpcode::CallIndirectVoid) {
        parameterCounts.insert(instruction.imm);
      }
    }
  }
  if (parameterCounts.empty()) {
    return;
  }
  out << "static const unsigned psFunctionParameters[] = {";
  for (const IrFunction &function : module.functions) {
    out << function.parameterCount << ", ";
  }
  out << "0};\nstatic const char *const psFunctionNames[] = {";
  for (const IrFunction &function : module.functions) {
    out << cppStringLiteral(function.name) << ", ";
  }
  out << "\"\"};\n";
  for (const uint64_t parameterCount : parameterCounts) {
    out << "static int64_t " << cppIndirectCallSymbol(parameterCount)
        << "(PsStack &stack, std::size_t &sp, VmHeapCore &heap, int argc, char **argv) {\n";
    out << "  if (!psEnsureStack(sp, " << parameterCount + 1 << "ull, \"indirect call\")) {\n    std::exit(3);\n  }\n";
    out << "  const std::size_t base = sp - " << parameterCount << "ull - 1ull;\n";
    out << "  const uint64_t address = stack[base];\n";
    out << "  switch (address) {\n";
    for (size_t index = 0; index < module.functions.size(); ++index) {
      if (module.functions[index].parameterCount != parameterCount) {
        continue;
      }
      out << "  case " << index + 1 << "ull:\n";
      if (parameterCount > 0) {
        out << "    for (std::size_t slot = 0; slot < " << parameterCount
            << "ull; ++slot) {\n      stack[base + slot] = stack[base + slot + 1];\n    }\n";
      }
      out << "    sp -= 1;\n    return ps_fn_" << index << "(stack, sp, heap, argc, argv);\n";
    }
    out << "  default:\n";
    out << "    if (address == 0ull) {\n"
        << "      std::cerr << \"VM error: indirect call through a null function address\\n\";\n"
        << "    } else if (address > " << module.functions.size() << "ull) {\n"
        << "      std::cerr << \"VM error: indirect call through an invalid function address\\n\";\n"
        << "    } else {\n"
        << "      std::cerr << \"VM error: indirect call passes " << parameterCount
        << " parameters to \" << psFunctionNames[address - 1ull] << \", which takes \"\n"
        << "                << psFunctionParameters[address - 1ull] << '\\n';\n"
        << "    }\n"
        << "    std::exit(3);\n  }\n}\n\n";
  }
}

} // namespace primec
