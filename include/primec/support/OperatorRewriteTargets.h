#pragma once

#include <string_view>

namespace primec {

// The lowerer rewrites an operator on a matrix or a quaternion (`multiply(m, m)`, `plus(q, q)`) into a call of a helper
// of `/std/math` (src/ir_lowerer/IrLowererOperatorArithmeticHelpers.cpp). The helpers are `[private]`: a program cannot
// call them, but a library that defines them must hand them to a program that links it, so its interface carries them
// as private entries. This is the one place that says which definitions those are.
inline bool isOperatorRewriteTargetPath(std::string_view path) {
  constexpr std::string_view Namespace = "/std/math/";
  constexpr std::string_view Suffix = "_internal";
  if (path.substr(0, Namespace.size()) != Namespace ||
      path.size() <= Namespace.size() + Suffix.size() ||
      path.substr(path.size() - Suffix.size()) != Suffix) {
    return false;
  }
  const std::string_view name = path.substr(Namespace.size());
  for (const std::string_view prefix : {"mat2_", "mat3_", "mat4_", "quat_"}) {
    if (name.substr(0, prefix.size()) == prefix && name.find('/') == std::string_view::npos) {
      return true;
    }
  }
  return false;
}

} // namespace primec
