#include "primec/support/EmitKind.h"

namespace primec {
namespace {

constexpr std::string_view PrimecEmitKinds[] = {"cpp",
                                                "cpp-ir",
                                                "exe",
                                                "exe-ir",
                                                "native",
                                                "ir",
                                                "vm",
                                                "glsl",
                                                "spirv",
                                                "wasm",
                                                "glsl-ir",
                                                "spirv-ir",
                                                "optcpp",
                                                "optcpp-ir",
                                                "optexe",
                                                "optexe-ir"};
constexpr std::string_view PrimecEmitKindsUsage =
    "cpp|cpp-ir|exe|exe-ir|native|ir|vm|glsl|spirv|wasm|glsl-ir|spirv-ir|optcpp|optcpp-ir|optexe|"
    "optexe-ir";

} // namespace

std::span<const std::string_view> listPrimecEmitKinds() {
  return PrimecEmitKinds;
}

bool isPrimecEmitKind(std::string_view emitKind) {
  for (const std::string_view candidate : PrimecEmitKinds) {
    if (candidate == emitKind) {
      return true;
    }
  }
  return false;
}

std::string_view primecEmitKindsUsage() {
  return PrimecEmitKindsUsage;
}

std::string_view resolveIrBackendEmitKind(std::string_view emitKind) {
  if (emitKind == "cpp") {
    return "cpp-ir";
  }
  if (emitKind == "exe") {
    return "exe-ir";
  }
  if (emitKind == "optcpp") {
    return "optcpp-ir";
  }
  if (emitKind == "optexe") {
    return "optexe-ir";
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
