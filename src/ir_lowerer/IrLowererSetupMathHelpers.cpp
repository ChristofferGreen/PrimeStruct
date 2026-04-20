#include "IrLowererSetupMathHelpers.h"

namespace primec::ir_lowerer {

namespace {
std::string resolveMathExprName(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

bool parseMathName(const std::string &name, bool hasMathImport, std::string &out) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  const bool hasLeadingSlash = !normalized.empty() && normalized[0] == '/';
  if (hasLeadingSlash) {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/math/", 0) == 0) {
    out = normalized.substr(9);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (hasLeadingSlash) {
    out = normalized;
    return true;
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

bool isSupportedMathBuiltinName(const std::string &name) {
  return name == "abs" || name == "sign" || name == "min" || name == "max" ||
         name == "clamp" || name == "lerp" || name == "saturate" || name == "floor" ||
         name == "ceil" || name == "round" || name == "trunc" || name == "fract" ||
         name == "sqrt" || name == "cbrt" || name == "pow" || name == "exp" ||
         name == "exp2" || name == "log" || name == "log2" || name == "log10" ||
         name == "sin" || name == "cos" || name == "tan" || name == "asin" ||
         name == "acos" || name == "atan" || name == "atan2" || name == "radians" ||
         name == "degrees" || name == "sinh" || name == "cosh" || name == "tanh" ||
         name == "asinh" || name == "acosh" || name == "atanh" || name == "fma" ||
         name == "hypot" || name == "copysign" || name == "is_nan" ||
         name == "is_inf" || name == "is_finite";
}

SetupMathResolvers makeSetupMathResolvers(bool hasMathImport) {
  SetupMathResolvers resolvers;
  resolvers.getMathBuiltinName = makeGetSetupMathBuiltinName(hasMathImport);
  resolvers.getMathConstantName = makeGetSetupMathConstantName(hasMathImport);
  return resolvers;
}

SetupMathAndBindingAdapters makeSetupMathAndBindingAdapters(bool hasMathImport,
                                                           const SemanticProgram *semanticProgram) {
  SetupMathAndBindingAdapters adapters;
  adapters.setupMathResolvers = makeSetupMathResolvers(hasMathImport);
  adapters.bindingTypeAdapters = makeBindingTypeAdapters(semanticProgram);
  return adapters;
}

GetSetupMathBuiltinNameFn makeGetSetupMathBuiltinName(bool hasMathImport) {
  return [hasMathImport](const Expr &expr, std::string &out) {
    return getSetupMathBuiltinName(expr, hasMathImport, out);
  };
}

GetSetupMathConstantNameFn makeGetSetupMathConstantName(bool hasMathImport) {
  return [hasMathImport](const std::string &name, std::string &out) {
    return getSetupMathConstantName(name, hasMathImport, out);
  };
}

bool getSetupMathBuiltinName(const Expr &expr, bool hasMathImport, std::string &out) {
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), hasMathImport, out)) {
    return false;
  }
  return isSupportedMathBuiltinName(out);
}

bool getSetupMathConstantName(const std::string &name, bool hasMathImport, std::string &out) {
  if (!parseMathName(name, hasMathImport, out)) {
    return false;
  }
  return out == "pi" || out == "tau" || out == "e";
}

} // namespace primec::ir_lowerer
