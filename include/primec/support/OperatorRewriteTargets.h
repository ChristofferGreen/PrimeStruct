#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace primec {

// The lowerer rewrites an operator on a vector, a matrix or a quaternion (`plus(v, v)`, `multiply(m, m)`) into a call of a helper
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
  for (const std::string_view prefix :
       {"mat2_", "mat3_", "mat4_", "quat_", "vec2_", "vec3_", "vec4_"}) {
    if (name.substr(0, prefix.size()) == prefix && name.find('/') == std::string_view::npos) {
      return true;
    }
  }
  return false;
}

// The helper a typed operator site `builtin(left, right)` is rewritten to, given the math type of each operand
// (`/std/math/Vec2`, `/std/math/Mat4`, `/std/math/Quat`) or empty for a scalar operand. The validator names the
// helpers a program needs with this, so the lazy stdlib import takes exactly those. Empty when the site has none.
inline std::string operatorRewriteTargetPath(std::string_view builtin,
                                             std::string_view leftType,
                                             std::string_view rightType) {
  constexpr std::string_view Namespace = "/std/math/";
  auto base = [&](std::string_view typePath) {
    std::string name(typePath.substr(Namespace.size()));
    for (char &c : name) {
      c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    }
    return name;
  };
  const bool hasLeft = !leftType.empty();
  const bool hasRight = !rightType.empty();
  std::string name;
  if (builtin == "plus" || builtin == "minus") {
    if (!hasLeft || leftType != rightType) {
      return {};
    }
    name = base(leftType) + (builtin == "plus" ? "_add_internal" : "_sub_internal");
  } else if (builtin == "divide") {
    if (!hasLeft || hasRight) {
      return {};
    }
    name = base(leftType) + "_div_scalar_internal";
  } else if (builtin == "multiply") {
    if (hasLeft && hasRight) {
      if (leftType == rightType) {
        name = leftType == "/std/math/Quat"
                   ? "quat_multiply_internal"
                   : base(leftType) + "_mul_" + base(leftType) + "_internal";
      } else if (leftType == "/std/math/Quat") {
        name = "quat_rotate_vec3_internal";
      } else {
        name = base(leftType) + "_mul_" + base(rightType) + "_internal";
      }
    } else if (hasLeft) {
      name = base(leftType) + "_scale_internal";
    } else if (hasRight) {
      name = base(rightType) + "_scale_left_internal";
    } else {
      return {};
    }
  } else {
    return {};
  }
  return std::string(Namespace) + name;
}

} // namespace primec
