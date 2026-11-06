#include "SemanticsValidator.h"

namespace primec::semantics {

std::string SemanticsValidator::moveOnlyPlaceType(const Expr &value,
                                                  const std::vector<ParameterInfo> &params,
                                                  const std::unordered_map<std::string, BindingInfo> &locals) {
  const bool isPlace = value.kind == Expr::Kind::Name || (value.kind == Expr::Kind::Call && value.isFieldAccess);
  if (!isPlace || currentValidationState_.context.definitionIsUnsafe) {
    return {};
  }
  // Generic code (every template instance) and the stdlib copy values of any type bitwise by design; the rule is for
  // concrete user code.
  const std::string &definitionPath = currentValidationState_.context.definitionPath;
  if (definitionPath.rfind("/std/", 0) == 0 || definitionPath.find("__t") != std::string::npos) {
    return {};
  }
  const std::string structPath = inferStructReturnPath(value, params, locals);
  if (structPath.empty() || structNames_.count(structPath) == 0) {
    return {};
  }
  if (defMap_.count(structPath + "/Destroy") == 0 || defMap_.count(structPath + "/Copy") != 0) {
    return {};
  }
  return structPath;
}

bool SemanticsValidator::failIfMoveOnlyCopy(const Expr &diagnosticExpr,
                                            const Expr &value,
                                            const std::vector<ParameterInfo> &params,
                                            const std::unordered_map<std::string, BindingInfo> &locals) {
  const std::string moveOnlyType = moveOnlyPlaceType(value, params, locals);
  if (moveOnlyType.empty()) {
    return false;
  }
  (void)failExprDiagnostic(diagnosticExpr,
                           "move-only type " + moveOnlyType + " cannot be copied: use move(" +
                               (value.kind == Expr::Kind::Name ? value.name : std::string("...")) + ")");
  return true;
}

} // namespace primec::semantics
