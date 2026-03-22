#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::isBuiltinBlockCall(const Expr &expr) const {
  if (!isBlockCall(expr)) {
    return false;
  }
  return defMap_.count(resolveCalleePath(expr)) == 0;
}

bool SemanticsValidator::isParam(const std::vector<ParameterInfo> &params, const std::string &name) const {
  for (const auto &param : params) {
    if (param.name == name) {
      return true;
    }
  }
  return false;
}

const BindingInfo *SemanticsValidator::findParamBinding(const std::vector<ParameterInfo> &params,
                                                        const std::string &name) const {
  for (const auto &param : params) {
    if (param.name == name) {
      return &param.binding;
    }
  }
  return nullptr;
}

std::string SemanticsValidator::typeNameForReturnKind(ReturnKind kind) const {
  switch (kind) {
    case ReturnKind::Int:
      return "i32";
    case ReturnKind::Int64:
      return "i64";
    case ReturnKind::UInt64:
      return "u64";
    case ReturnKind::Bool:
      return "bool";
    case ReturnKind::String:
      return "string";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    case ReturnKind::Integer:
      return "integer";
    case ReturnKind::Decimal:
      return "decimal";
    case ReturnKind::Complex:
      return "complex";
    case ReturnKind::Array:
      return "array";
    default:
      return "";
  }
}

bool SemanticsValidator::lookupGraphLocalAutoBinding(const std::string &scopePath,
                                                     const Expr &bindingExpr,
                                                     BindingInfo &bindingOut) const {
  if (scopePath.empty()) {
    return false;
  }
  const auto [sourceLine, sourceColumn] = graphLocalAutoSourceLocation(bindingExpr);
  const auto it = graphLocalAutoBindings_.find(graphLocalAutoBindingKey(
      scopePath, sourceLine, sourceColumn));
  if (it == graphLocalAutoBindings_.end()) {
    return false;
  }
  bindingOut = it->second;
  return true;
}

bool SemanticsValidator::lookupGraphLocalAutoBinding(const Expr &bindingExpr, BindingInfo &bindingOut) const {
  return lookupGraphLocalAutoBinding(currentValidationContext_.definitionPath, bindingExpr, bindingOut);
}

}  // namespace primec::semantics
