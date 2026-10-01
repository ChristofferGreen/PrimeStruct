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

// Reserved names of the host functions the embedding engine generates for
// exported-function wrappers (see docs/Embedding.md). `__psarg_*` functions fetch
// call arguments and are the only host functions allowed to return `string`: the
// result is the index of a string the engine appended to the module's string
// table for that call.
inline bool isEngineArgumentHostName(std::string_view name) {
  return name.rfind("__psarg_", 0) == 0;
}

// Canonical host-boundary spelling ("i32", "i64", "u64", "f32", "f64", "bool", "string")
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
  if (typeName == "string") {
    return "string";  // parameters only; see isHostReturnTypeName
  }
  return std::nullopt;
}

} // namespace primec
