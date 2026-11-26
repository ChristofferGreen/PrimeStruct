#pragma once

#include <string>

#include "primec/ast/Ast.h"

namespace primec {

// Two compiler-internal definition markers that give the function address opcodes (docs/spec/vm-design.md, Function
// addresses) a place in the source. Both have an empty body and an explicit return type, like an imported declaration.
//
//   [indirect_call return<i32>] call_it([u64] address, [i32] x) {}
//     A call of it is a call of the function the address names: the call site binds the parameters as for any real
//     call (the address first, then the callee's own) and the lowerer emits CallIndirect, so the definition has no
//     function in the module. Every function an address can name takes the parameters after the address.
//   [function_address("/path/to/def") return<u64>] address_of_def() {}
//     A call of it is the address of the definition it names: the definition becomes a real-call function and the
//     lowerer emits FunctionAddress for it.
inline const Transform *indirectCallTransform(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "indirect_call") {
      return &transform;
    }
  }
  return nullptr;
}

inline bool isIndirectCallDefinition(const Definition &def) {
  return indirectCallTransform(def) != nullptr;
}

inline const Transform *functionAddressTransform(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "function_address") {
      return &transform;
    }
  }
  return nullptr;
}

inline bool isFunctionAddressDefinition(const Definition &def) {
  return functionAddressTransform(def) != nullptr;
}

// The path a function address definition names, or empty when the marker has no single argument. Written in source as
// a string literal (`function_address("/add_one")`, which the parser keeps with its quotes and suffix), and as the bare
// path by the passes that generate the definition.
inline std::string functionAddressTarget(const Definition &def) {
  const Transform *transform = functionAddressTransform(def);
  if (transform == nullptr || transform->arguments.size() != 1) {
    return std::string();
  }
  const std::string &argument = transform->arguments.front();
  if (!argument.empty() && argument.front() == '"') {
    const size_t close = argument.rfind('"');
    return close > 0 ? argument.substr(1, close - 1) : std::string();
  }
  return argument;
}

} // namespace primec
