#include "IrLowererSetupMathHelpers.h"

namespace primec::ir_lowerer {

namespace {
bool parseMathName(const std::string &name, bool hasMathImport, std::string &out) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/math/", 0) == 0) {
    out = normalized.substr(9);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (!hasMathImport) {
    return false;
  }
  out = normalized;
  return true;
}
} // namespace

bool isMathImportPath(const std::string &path) {
  if (path == "/std/math/*") {
    return true;
  }
  return path.rfind("/std/math/", 0) == 0 && path.size() > 10;
}

bool hasProgramMathImport(const std::vector<std::string> &imports) {
  for (const auto &importPath : imports) {
    if (isMathImportPath(importPath)) {
      return true;
    }
  }
  return false;
}

bool getSetupMathBuiltinName(const Expr &expr, bool hasMathImport, std::string &out) {
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (!parseMathName(expr.name, hasMathImport, out)) {
    return false;
  }
  if (out == "abs" || out == "sign" || out == "min" || out == "max" || out == "clamp" || out == "lerp" ||
      out == "saturate" || out == "floor" || out == "ceil" || out == "round" || out == "trunc" ||
      out == "fract" || out == "sqrt" || out == "cbrt" || out == "pow" || out == "exp" || out == "exp2" ||
      out == "log" || out == "log2" || out == "log10" || out == "sin" || out == "cos" || out == "tan" ||
      out == "asin" || out == "acos" || out == "atan" || out == "atan2" || out == "radians" ||
      out == "degrees" || out == "sinh" || out == "cosh" || out == "tanh" || out == "asinh" ||
      out == "acosh" || out == "atanh" || out == "fma" || out == "hypot" || out == "copysign" ||
      out == "is_nan" || out == "is_inf" || out == "is_finite") {
    return true;
  }
  return false;
}

bool getSetupMathConstantName(const std::string &name, bool hasMathImport, std::string &out) {
  if (!parseMathName(name, hasMathImport, out)) {
    return false;
  }
  return out == "pi" || out == "tau" || out == "e";
}

} // namespace primec::ir_lowerer
