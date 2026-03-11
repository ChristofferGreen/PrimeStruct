#include "primec/EmitKind.h"

namespace primec {

std::string_view resolveIrBackendEmitKind(std::string_view emitKind) {
  if (emitKind == "cpp") {
    return "cpp-ir";
  }
  if (emitKind == "exe") {
    return "exe-ir";
  }
  if (emitKind == "glsl") {
    return "glsl-ir";
  }
  if (emitKind == "spirv") {
    return "spirv-ir";
  }
  return emitKind;
}

} // namespace primec
