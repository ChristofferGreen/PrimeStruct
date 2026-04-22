#include "SemanticsValidator.h"
#include "SemanticsValidatorExprNumericInternal.h"

namespace primec::semantics {

bool SemanticsValidator::isNumericExpr(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &expr) {
  auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
    if (binding.typeName == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
          return ReturnKind::Array;
        }
      }
      return returnKindForTypeName(binding.typeTemplateArg);
    }
    return returnKindForTypeName(binding.typeName);
  };

  ReturnKind kind = inferExprReturnKind(expr, params, locals);
  const bool isSoftware = kind == ReturnKind::Integer || kind == ReturnKind::Decimal || kind == ReturnKind::Complex;
  if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
      kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || isSoftware) {
    return true;
  }
  if (kind == ReturnKind::Bool || kind == ReturnKind::Void || kind == ReturnKind::Array) {
    return false;
  }
  if (kind == ReturnKind::Unknown) {
    if (expr.kind == Expr::Kind::StringLiteral) {
      return false;
    }
    if (isPointerExpr(expr, params, locals)) {
      return false;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
        ReturnKind paramKind = returnKindForBinding(*paramBinding);
        return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
               paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64 ||
               paramKind == ReturnKind::Integer || paramKind == ReturnKind::Decimal ||
               paramKind == ReturnKind::Complex;
      }
      auto it = locals.find(expr.name);
      if (it != locals.end()) {
        ReturnKind localKind = returnKindForBinding(it->second);
        return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
               localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64 ||
               localKind == ReturnKind::Integer || localKind == ReturnKind::Decimal ||
               localKind == ReturnKind::Complex;
      }
    }
    return true;
  }
  return false;
}

bool SemanticsValidator::isFloatExpr(const std::vector<ParameterInfo> &params,
                                    const std::unordered_map<std::string, BindingInfo> &locals,
                                    const Expr &expr) {
  auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
    if (binding.typeName == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
          return ReturnKind::Array;
        }
      }
      return returnKindForTypeName(binding.typeTemplateArg);
    }
    return returnKindForTypeName(binding.typeName);
  };

  ReturnKind kind = inferExprReturnKind(expr, params, locals);
  if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Decimal) {
    return true;
  }
  if (kind == ReturnKind::Bool || kind == ReturnKind::Void || kind == ReturnKind::Array || kind == ReturnKind::Int ||
      kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Integer ||
      kind == ReturnKind::Complex) {
    return false;
  }
  if (kind == ReturnKind::Unknown) {
    if (expr.kind == Expr::Kind::FloatLiteral) {
      return true;
    }
    if (expr.kind == Expr::Kind::StringLiteral) {
      return false;
    }
    if (isPointerExpr(expr, params, locals)) {
      return false;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
        ReturnKind paramKind = returnKindForBinding(*paramBinding);
        return paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64 ||
               paramKind == ReturnKind::Decimal;
      }
      auto it = locals.find(expr.name);
      if (it != locals.end()) {
        ReturnKind localKind = returnKindForBinding(it->second);
        return localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64 ||
               localKind == ReturnKind::Decimal;
      }
    }
    return true;
  }
  return false;
}

bool SemanticsValidator::isComparisonOperand(const std::vector<ParameterInfo> &params,
                                             const std::unordered_map<std::string, BindingInfo> &locals,
                                             const Expr &expr) {
  auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
    if (binding.typeName == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
          return ReturnKind::Array;
        }
      }
      return returnKindForTypeName(binding.typeTemplateArg);
    }
    return returnKindForTypeName(binding.typeName);
  };

  ReturnKind kind = inferExprReturnKind(expr, params, locals);
  if (kind == ReturnKind::Bool || kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
      kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Integer ||
      kind == ReturnKind::Decimal || kind == ReturnKind::Complex) {
    return true;
  }
  if (kind == ReturnKind::Void || kind == ReturnKind::Array) {
    return false;
  }
  if (kind == ReturnKind::Unknown) {
    if (expr.kind == Expr::Kind::StringLiteral) {
      return false;
    }
    if (isPointerExpr(expr, params, locals)) {
      return false;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
        ReturnKind paramKind = returnKindForBinding(*paramBinding);
        return paramKind == ReturnKind::Bool || paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
               paramKind == ReturnKind::UInt64 || paramKind == ReturnKind::Float32 ||
               paramKind == ReturnKind::Float64 || paramKind == ReturnKind::Integer ||
               paramKind == ReturnKind::Decimal || paramKind == ReturnKind::Complex;
      }
      auto it = locals.find(expr.name);
      if (it != locals.end()) {
        ReturnKind localKind = returnKindForBinding(it->second);
        return localKind == ReturnKind::Bool || localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
               localKind == ReturnKind::UInt64 || localKind == ReturnKind::Float32 ||
               localKind == ReturnKind::Float64 || localKind == ReturnKind::Integer ||
               localKind == ReturnKind::Decimal || localKind == ReturnKind::Complex;
      }
    }
    return true;
  }
  return false;
}

std::string SemanticsValidator::inferMatrixQuaternionTypePath(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  std::string typePath = inferStructReturnPath(expr, params, locals);
  if (!detail::isMatrixQuaternionTypePath(typePath)) {
    return "";
  }
  return typePath;
}

std::string SemanticsValidator::inferVectorTypePath(const Expr &expr,
                                                    const std::vector<ParameterInfo> &params,
                                                    const std::unordered_map<std::string, BindingInfo> &locals) {
  std::string typePath = inferStructReturnPath(expr, params, locals);
  if (!detail::isVectorTypePath(typePath)) {
    return "";
  }
  return typePath;
}

bool SemanticsValidator::isImplicitMatrixQuaternionConversion(const std::string &expectedTypePath,
                                                              const std::string &actualTypePath) const {
  return !expectedTypePath.empty() && !actualTypePath.empty() && expectedTypePath != actualTypePath &&
         detail::isMatrixQuaternionConversionTypePath(expectedTypePath) &&
         detail::isMatrixQuaternionConversionTypePath(actualTypePath) &&
         (detail::isMatrixQuaternionTypePath(expectedTypePath) ||
          detail::isMatrixQuaternionTypePath(actualTypePath));
}

std::string SemanticsValidator::implicitMatrixQuaternionConversionDiagnostic(
    const std::string &expectedTypePath,
    const std::string &actualTypePath) const {
  return "implicit matrix/quaternion family conversion requires explicit helper: expected " + expectedTypePath +
         " got " + actualTypePath;
}

bool SemanticsValidator::resolveReflectedStructEqualityHelperPath(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &builtinName,
    std::string &helperPathOut) const {
  helperPathOut.clear();
  if (builtinName != "equal" || expr.kind != Expr::Kind::Call ||
      expr.args.size() != 2) {
    return false;
  }

  auto *mutableSelf = const_cast<SemanticsValidator *>(this);
  const std::string leftStructPath =
      mutableSelf->inferStructReturnPath(expr.args[0], params, locals);
  if (leftStructPath.empty()) {
    return false;
  }
  const std::string rightStructPath =
      mutableSelf->inferStructReturnPath(expr.args[1], params, locals);
  if (rightStructPath.empty() || rightStructPath != leftStructPath) {
    return false;
  }

  const auto structIt = defMap_.find(leftStructPath);
  if (structIt == defMap_.end() || structIt->second == nullptr) {
    return false;
  }

  bool hasGeneratedEqual = false;
  for (const Transform &transform : structIt->second->transforms) {
    if (transform.name != "generate") {
      continue;
    }
    for (const std::string &arg : transform.args) {
      if (arg == "Equal") {
        hasGeneratedEqual = true;
        break;
      }
    }
    if (hasGeneratedEqual) {
      break;
    }
  }
  if (!hasGeneratedEqual) {
    return false;
  }

  const std::string helperPath = leftStructPath + "/Equal";
  if (defMap_.count(helperPath) == 0 && paramsByDef_.count(helperPath) == 0) {
    return false;
  }

  helperPathOut = helperPath;
  return true;
}

} // namespace primec::semantics
