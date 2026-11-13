#include "primec/ir_lowerer/IrLowererSetupMathHelpers.h"
#include "primec/support/MathNames.h"

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
  return isMathFunctionName(name);
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
  if (!primec::parseMathName(resolveMathExprName(expr), out, {hasMathImport})) {
    return false;
  }
  return isSupportedMathBuiltinName(out);
}

bool getSetupMathConstantName(const std::string &name, bool hasMathImport, std::string &out) {
  return primec::classifyMathName(name, out, {hasMathImport}, MathFamily::Constant);
}

} // namespace primec::ir_lowerer
