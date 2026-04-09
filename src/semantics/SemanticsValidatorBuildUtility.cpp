#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::isBuiltinBlockCall(const Expr &expr) const {
  if (!isBlockCall(expr)) {
    return false;
  }
  return defMap_.count(resolveCalleePath(expr)) == 0;
}

bool SemanticsValidator::isEnvelopeValueExpr(const Expr &expr, bool allowAnyName) const {
  if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.isMethodCall) {
    return false;
  }
  if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
    return false;
  }
  if (!expr.hasBodyArguments && expr.bodyArguments.empty()) {
    return false;
  }
  return allowAnyName || isBuiltinBlockCall(expr);
}

bool SemanticsValidator::isSyntheticBlockValueBinding(const Expr &expr) const {
  if (expr.kind != Expr::Kind::Call || !expr.isBinding || expr.isMethodCall || expr.name != "block") {
    return false;
  }
  if (expr.args.size() != 1 || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
    return false;
  }
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return false;
  }
  return expr.transforms.empty();
}

const Expr *SemanticsValidator::getEnvelopeValueExpr(const Expr &expr, bool allowAnyName) const {
  if (!isEnvelopeValueExpr(expr, allowAnyName)) {
    return nullptr;
  }

  const Expr *valueExpr = nullptr;
  for (const auto &bodyExpr : expr.bodyArguments) {
    if (bodyExpr.isBinding) {
      if (isSyntheticBlockValueBinding(bodyExpr)) {
        valueExpr = &bodyExpr.args.front();
      }
      continue;
    }
    if (isReturnCall(bodyExpr) && bodyExpr.args.size() == 1) {
      valueExpr = &bodyExpr.args.front();
      break;
    }
    valueExpr = &bodyExpr;
  }
  return valueExpr;
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
  return lookupGraphLocalAutoBinding(currentValidationState_.context.definitionPath, bindingExpr, bindingOut);
}

bool SemanticsValidator::lookupGraphLocalAutoDirectCallFact(const std::string &scopePath,
                                                            const Expr &bindingExpr,
                                                            std::string &resolvedPathOut,
                                                            ReturnKind &returnKindOut) const {
  resolvedPathOut.clear();
  returnKindOut = ReturnKind::Unknown;
  if (scopePath.empty()) {
    return false;
  }
  const auto [sourceLine, sourceColumn] = graphLocalAutoSourceLocation(bindingExpr);
  const GraphLocalAutoKey bindingKey = graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn);
  bool found = false;
  if (const auto resolvedPathIt = graphLocalAutoDirectCallResolvedPaths_.find(bindingKey);
      resolvedPathIt != graphLocalAutoDirectCallResolvedPaths_.end()) {
    resolvedPathOut = resolvedPathIt->second;
    found = true;
  }
  if (const auto returnKindIt = graphLocalAutoDirectCallReturnKinds_.find(bindingKey);
      returnKindIt != graphLocalAutoDirectCallReturnKinds_.end()) {
    returnKindOut = returnKindIt->second;
    found = true;
  }
  return found;
}

bool SemanticsValidator::lookupGraphLocalAutoDirectCallFact(const Expr &bindingExpr,
                                                            std::string &resolvedPathOut,
                                                            ReturnKind &returnKindOut) const {
  return lookupGraphLocalAutoDirectCallFact(
      currentValidationState_.context.definitionPath, bindingExpr, resolvedPathOut, returnKindOut);
}

bool SemanticsValidator::lookupGraphLocalAutoMethodCallFact(const std::string &scopePath,
                                                            const Expr &bindingExpr,
                                                            std::string &resolvedPathOut,
                                                            ReturnKind &returnKindOut) const {
  resolvedPathOut.clear();
  returnKindOut = ReturnKind::Unknown;
  if (scopePath.empty()) {
    return false;
  }
  const auto [sourceLine, sourceColumn] = graphLocalAutoSourceLocation(bindingExpr);
  const GraphLocalAutoKey bindingKey = graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn);
  bool found = false;
  if (const auto resolvedPathIt = graphLocalAutoMethodCallResolvedPaths_.find(bindingKey);
      resolvedPathIt != graphLocalAutoMethodCallResolvedPaths_.end()) {
    resolvedPathOut = resolvedPathIt->second;
    found = true;
  }
  if (const auto returnKindIt = graphLocalAutoMethodCallReturnKinds_.find(bindingKey);
      returnKindIt != graphLocalAutoMethodCallReturnKinds_.end()) {
    returnKindOut = returnKindIt->second;
    found = true;
  }
  return found;
}

bool SemanticsValidator::lookupGraphLocalAutoMethodCallFact(const Expr &bindingExpr,
                                                            std::string &resolvedPathOut,
                                                            ReturnKind &returnKindOut) const {
  return lookupGraphLocalAutoMethodCallFact(
      currentValidationState_.context.definitionPath, bindingExpr, resolvedPathOut, returnKindOut);
}

}  // namespace primec::semantics
