#include "SemanticsValidator.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprCollectionLiteralBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    bool &handledOut) {
  handledOut = false;
  auto isStringExpr = [&](const Expr &arg) -> bool {
    if (arg.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (inferExprReturnKind(arg, params, locals) == ReturnKind::String) {
      return true;
    }
    std::string collectionTypePath;
    return arg.kind == Expr::Kind::Call &&
           resolveCallCollectionTypePath(arg, params, locals, collectionTypePath) &&
           collectionTypePath == "/string";
  };

  auto validateCollectionElementType = [&](const Expr &arg,
                                           const std::string &typeName,
                                           const std::string &errorPrefix) {
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    if (normalizedType == "string") {
      if (!isStringExpr(arg)) {
        error_ = errorPrefix + typeName;
        return false;
      }
      return true;
    }
    ReturnKind expectedKind = returnKindForTypeName(normalizedType);
    if (expectedKind == ReturnKind::Unknown) {
      return true;
    }
    if (isStringExpr(arg)) {
      error_ = errorPrefix + typeName;
      return false;
    }
    ReturnKind argKind = inferExprReturnKind(arg, params, locals);
    if (argKind != ReturnKind::Unknown && argKind != expectedKind) {
      error_ = errorPrefix + typeName;
      return false;
    }
    return true;
  };

  std::string builtinName;
  if (!getBuiltinCollectionName(expr, builtinName)) {
    return true;
  }

  handledOut = true;
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    error_ = builtinName + " literal does not accept block arguments";
    return false;
  }
  if (builtinName == "soa_vector") {
    if (expr.templateArgs.size() != 1) {
      error_ = "soa_vector literal requires exactly one template argument";
      return false;
    }
    if (!isSoaVectorStructElementType(expr.templateArgs.front(),
                                      expr.namespacePrefix, structNames_,
                                      importAliases_)) {
      error_ = "soa_vector literal requires struct element type";
      return false;
    }
    if (!validateSoaVectorElementFieldEnvelopes(expr.templateArgs.front(),
                                                expr.namespacePrefix)) {
      return false;
    }
    if (!expr.args.empty() && activeEffects_.count("heap_alloc") == 0) {
      error_ = "soa_vector literal requires heap_alloc effect";
      return false;
    }
  }
  if (builtinName == "vector" && !expr.args.empty()) {
    if (activeEffects_.count("heap_alloc") == 0) {
      error_ = "vector literal requires heap_alloc effect";
      return false;
    }
  }
  if (builtinName == "array" || builtinName == "vector" ||
      builtinName == "soa_vector") {
    if (expr.templateArgs.size() != 1) {
      if (builtinName == "array" && expr.templateArgs.size() > 1) {
        error_ =
            "array<T, N> is unsupported; use array<T> (runtime-count array)";
        return false;
      }
      error_ = builtinName + " literal requires exactly one template argument";
      return false;
    }
  } else {
    if (expr.templateArgs.size() != 2) {
      error_ = "map literal requires exactly two template arguments";
      return false;
    }
    if (expr.args.size() % 2 != 0) {
      error_ = "map literal requires an even number of arguments";
      return false;
    }
  }
  for (const auto &arg : expr.args) {
    if (!validateExpr(params, locals, arg)) {
      return false;
    }
  }
  if ((builtinName == "array" || builtinName == "vector" ||
       builtinName == "soa_vector") &&
      !expr.templateArgs.empty()) {
    const std::string &elemType = expr.templateArgs.front();
    for (const auto &arg : expr.args) {
      if (!validateCollectionElementType(
              arg, elemType,
              builtinName + " literal requires element type ")) {
        return false;
      }
    }
  }
  if (builtinName == "map" && expr.templateArgs.size() == 2) {
    const std::string &keyType = expr.templateArgs[0];
    const std::string &valueType = expr.templateArgs[1];
    for (size_t i = 0; i + 1 < expr.args.size(); i += 2) {
      if (!validateCollectionElementType(expr.args[i], keyType,
                                         "map literal requires key type ")) {
        return false;
      }
      if (!validateCollectionElementType(expr.args[i + 1], valueType,
                                         "map literal requires value type ")) {
        return false;
      }
    }
  }
  return true;
}

} // namespace primec::semantics
