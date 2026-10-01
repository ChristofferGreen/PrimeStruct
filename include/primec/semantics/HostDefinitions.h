#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "primec/ast/Ast.h"

namespace primec {

// A host definition declares a function the embedding application provides:
//   [host return<int>] host_add([i32] a, [i32] b) {}
// It has an empty body and primitive signature, and lowers to IrOpcode::CallHost
// (VM target only). See docs/PrimeStruct.md and docs/Embedding.md.
inline bool isHostDefinition(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "host") {
      return true;
    }
  }
  return false;
}

// Canonical host-boundary spelling ("i32", "i64", "u64", "f32", "f64", "bool")
// for a primitive type name, or nullopt when the type cannot cross the boundary.
inline std::optional<std::string_view> canonicalHostTypeName(std::string_view typeName) {
  if (typeName == "i32" || typeName == "int") {
    return "i32";
  }
  if (typeName == "i64") {
    return "i64";
  }
  if (typeName == "u64") {
    return "u64";
  }
  if (typeName == "f32" || typeName == "float") {
    return "f32";
  }
  if (typeName == "f64") {
    return "f64";
  }
  if (typeName == "bool") {
    return "bool";
  }
  return std::nullopt;
}

} // namespace primec
