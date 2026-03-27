#include "IrToGlslEmitterInternal.h"
#include "primec/IrToGlslEmitter.h"

#include <sstream>

namespace primec {

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
    if (!ir_to_glsl_internal::emitFunction(module.functions[i], i, module.functions.size(), module.stringTable, body,
                                           error)) {
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
