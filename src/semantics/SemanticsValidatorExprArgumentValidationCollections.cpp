#include "SemanticsValidator.h"

#include <optional>
#include <string>
#include <vector>

namespace primec::semantics {

namespace {

bool getCanonicalMapAccessBuiltinName(const Expr &candidate,
                                      std::string &helperOut) {
  helperOut.clear();
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty() ||
      candidate.args.size() != 2) {
    return false;
  }
  if (getBuiltinArrayAccessName(candidate, helperOut)) {
    return true;
  }
  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  if (normalizedName == "map/at_ref" ||
      normalizedName == "std/collections/map/at_ref") {
    helperOut = "at_ref";
    return true;
  }
  if (normalizedName == "map/at_unsafe_ref" ||
      normalizedName == "std/collections/map/at_unsafe_ref") {
    helperOut = "at_unsafe_ref";
    return true;
  }
  return false;
}

bool isBuiltinSoaVectorTypeBaseForArgumentValidation(const std::string &base) {
  const std::string normalizedBase = normalizeBindingTypeName(base);
  return normalizedBase == "soa_vector" ||
         normalizedBase == "/soa_vector" ||
         normalizedBase == "std/collections/soa_vector" ||
         normalizedBase == "/std/collections/soa_vector" ||
         isExperimentalSoaVectorTypePath(normalizedBase);
}

bool extractBuiltinSoaVectorElementTypeFromTypeText(const std::string &typeText,
                                                    std::string &elemTypeOut) {
  elemTypeOut.clear();
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizedType, base, argText) || base.empty()) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      normalizedType = normalizeBindingTypeName(args.front());
      continue;
    }
    if (!isBuiltinSoaVectorTypeBaseForArgumentValidation(base) || argText.empty()) {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    elemTypeOut = args.front();
    return true;
  }
}

} // namespace

bool SemanticsValidator::isBuiltinCollectionLiteralExpr(const Expr &candidate) const {
  if (candidate.kind != Expr::Kind::Call) {
    return false;
  }
  if (defMap_.find(resolveCalleePath(candidate)) != defMap_.end()) {
    return false;
  }
  std::string collectionName;
  return getBuiltinCollectionName(candidate, collectionName);
}

bool SemanticsValidator::isStringExprForArgumentValidation(
    const Expr &arg,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  if (dispatchResolvers.resolveStringTarget != nullptr &&
      dispatchResolvers.resolveStringTarget(arg)) {
    return true;
  }
  if (arg.kind == Expr::Kind::Call) {
    const std::string resolvedPath = resolveCalleePath(arg);
    const bool treatAsBuiltinAccess =
        defMap_.find(resolvedPath) == defMap_.end() ||
        resolvedPath.rfind("/std/collections/vector/at", 0) == 0 ||
        resolvedPath == "/map/at" ||
        resolvedPath == "/map/at_ref" ||
        resolvedPath == "/map/at_unsafe" ||
        resolvedPath == "/map/at_unsafe_ref" ||
        resolvedPath == "/std/collections/map/at" ||
        resolvedPath == "/std/collections/map/at_ref" ||
        resolvedPath == "/std/collections/map/at_unsafe" ||
        resolvedPath == "/std/collections/map/at_unsafe_ref";
    std::string accessName;
    if (treatAsBuiltinAccess &&
        getCanonicalMapAccessBuiltinName(arg, accessName) &&
        arg.args.size() == 2) {
      std::string mapValueType;
      if (resolveMapValueType(arg.args.front(), dispatchResolvers, mapValueType) &&
          normalizeBindingTypeName(mapValueType) == "string") {
        return true;
      }
    }
  }
  return false;
}

bool SemanticsValidator::extractExperimentalMapFieldTypesFromStructPath(
    const std::string &structPath,
    std::string &keyTypeOut,
    std::string &valueTypeOut) const {
  keyTypeOut.clear();
  valueTypeOut.clear();
  if (structPath.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second->statements) {
    if (!fieldExpr.isBinding) {
      continue;
    }
    BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(fieldExpr,
                          defIt->second->namespacePrefix,
                          structNames_,
                          importAliases_,
                          fieldBinding,
                          restrictType,
                          parseError)) {
      continue;
    }
    std::string elemType;
    if (normalizeBindingTypeName(fieldBinding.typeName) == "vector" &&
        !fieldBinding.typeTemplateArg.empty()) {
      elemType = fieldBinding.typeTemplateArg;
    } else if (!extractExperimentalVectorElementType(fieldBinding, elemType)) {
      continue;
    }
    if (fieldExpr.name == "keys") {
      keyTypeOut = elemType;
    } else if (fieldExpr.name == "payloads") {
      valueTypeOut = elemType;
    }
  }
  return !keyTypeOut.empty() && !valueTypeOut.empty();
}

bool SemanticsValidator::extractExperimentalVectorElementTypeFromStructPath(
    const std::string &structPath,
    std::string &elemTypeOut) const {
  elemTypeOut.clear();
  if (structPath.rfind("/std/collections/experimental_vector/Vector__", 0) != 0) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second->statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(fieldExpr,
                          defIt->second->namespacePrefix,
                          structNames_,
                          importAliases_,
                          fieldBinding,
                          restrictType,
                          parseError)) {
      continue;
    }
    if (normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" ||
        fieldBinding.typeTemplateArg.empty()) {
      continue;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(fieldBinding.typeTemplateArg),
                               pointeeBase,
                               pointeeArgText) ||
        normalizeBindingTypeName(pointeeBase) != "uninitialized") {
      continue;
    }
    std::vector<std::string> pointeeArgs;
    if (!splitTopLevelTemplateArgs(pointeeArgText, pointeeArgs) ||
        pointeeArgs.size() != 1) {
      continue;
    }
    elemTypeOut = pointeeArgs.front();
    return true;
  }
  return false;
}

bool SemanticsValidator::extractExperimentalVectorElementType(const BindingInfo &binding,
                                                              std::string &elemTypeOut) const {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = normalizeBindingTypeName(args.front());
          continue;
        }
        std::string normalizedBase = base;
        if (!normalizedBase.empty() && normalizedBase.front() == '/') {
          normalizedBase.erase(normalizedBase.begin());
        }
        if ((normalizedBase == "Vector" ||
             normalizedBase == "std/collections/experimental_vector/Vector") &&
            !argText.empty()) {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (normalizedResolvedPath.rfind("std/collections/experimental_vector/Vector__", 0) != 0) {
        return false;
      }
      return extractExperimentalVectorElementTypeFromStructPath(resolvedPath, elemTypeOut);
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

bool SemanticsValidator::extractExperimentalSoaVectorElementTypeFromStructPath(
    const std::string &structPath,
    std::string &elemTypeOut) const {
  elemTypeOut.clear();
  if (!isExperimentalSoaVectorSpecializedTypePath(structPath)) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second->statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "storage") {
      continue;
    }
    BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(fieldExpr,
                          defIt->second->namespacePrefix,
                          structNames_,
                          importAliases_,
                          fieldBinding,
                          restrictType,
                          parseError)) {
      continue;
    }
    std::string normalizedFieldType = normalizeBindingTypeName(fieldBinding.typeName);
    if (!normalizedFieldType.empty() && normalizedFieldType.front() == '/') {
      normalizedFieldType.erase(normalizedFieldType.begin());
    }
    if (!fieldBinding.typeTemplateArg.empty() &&
        (normalizedFieldType == "SoaColumn" ||
         normalizedFieldType == "std/collections/internal_soa_storage/SoaColumn")) {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, args) || args.size() != 1) {
        continue;
      }
      elemTypeOut = args.front();
      return true;
    }
    std::string resolvedFieldPath = normalizeBindingTypeName(fieldBinding.typeName);
    if (!resolvedFieldPath.empty() && resolvedFieldPath.front() != '/') {
      resolvedFieldPath.insert(resolvedFieldPath.begin(), '/');
    }
    if (normalizedFieldType.rfind("std/collections/internal_soa_storage/SoaColumn__", 0) != 0 ||
        !extractExperimentalSoaColumnElementTypeFromStructPath(resolvedFieldPath, elemTypeOut)) {
      continue;
    }
    return true;
  }
  return false;
}

bool SemanticsValidator::extractExperimentalSoaColumnElementTypeFromStructPath(
    const std::string &structPath,
    std::string &elemTypeOut) const {
  elemTypeOut.clear();
  if (structPath.rfind("/std/collections/internal_soa_storage/SoaColumn__", 0) != 0) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second->statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(fieldExpr,
                          defIt->second->namespacePrefix,
                          structNames_,
                          importAliases_,
                          fieldBinding,
                          restrictType,
                          parseError)) {
      continue;
    }
    if (normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" ||
        fieldBinding.typeTemplateArg.empty()) {
      continue;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(fieldBinding.typeTemplateArg),
                               pointeeBase,
                               pointeeArgText) ||
        normalizeBindingTypeName(pointeeBase) != "uninitialized") {
      continue;
    }
    std::vector<std::string> pointeeArgs;
    if (!splitTopLevelTemplateArgs(pointeeArgText, pointeeArgs) ||
        pointeeArgs.size() != 1) {
      continue;
    }
    elemTypeOut = pointeeArgs.front();
    return true;
  }
  return false;
}

bool SemanticsValidator::extractExperimentalSoaVectorElementType(const BindingInfo &binding,
                                                                 std::string &elemTypeOut) const {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = normalizeBindingTypeName(args.front());
          continue;
        }
        std::string normalizedBase = base;
        if (!normalizedBase.empty() && normalizedBase.front() == '/') {
          normalizedBase.erase(normalizedBase.begin());
        }
        // Keep the experimental SOA guard paired with a concrete template payload:
        // isExperimentalSoaVectorTypePath(normalizedBase) && !argText.empty()
        if (normalizedBase == "soa_vector" ||
            (isExperimentalSoaVectorTypePath(normalizedBase) &&
             !argText.empty())) {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (!isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)) {
        return false;
      }
      return extractExperimentalSoaVectorElementTypeFromStructPath(resolvedPath, elemTypeOut);
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

bool SemanticsValidator::resolveExperimentalBorrowedSoaTypeText(
    const std::string &typeText, std::string &elemTypeOut) const {
  BindingInfo inferredBinding;
  const std::string normalizedType = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    inferredBinding.typeName = normalizeBindingTypeName(base);
    inferredBinding.typeTemplateArg = argText;
  } else {
    inferredBinding.typeName = normalizedType;
    inferredBinding.typeTemplateArg.clear();
  }
  const std::string normalizedBindingType =
      normalizeBindingTypeName(inferredBinding.typeName);
  if (normalizedBindingType != "Reference" &&
      normalizedBindingType != "Pointer") {
    return false;
  }
  if (extractBuiltinSoaVectorElementTypeFromTypeText(normalizedType,
                                                     elemTypeOut)) {
    return true;
  }
  return extractExperimentalSoaVectorElementType(inferredBinding, elemTypeOut);
}

bool SemanticsValidator::resolveDirectSoaVectorOrExperimentalBorrowedReceiver(
    const Expr &target,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveDirectReceiver,
    std::string &elemTypeOut) {
  if (resolveDirectReceiver(target, elemTypeOut)) {
    return true;
  }
  if (target.kind != Expr::Kind::Name) {
    return false;
  }

  auto extractBorrowedBinding = [&](const BindingInfo &binding) -> bool {
    const std::string normalizedType =
        normalizeBindingTypeName(binding.typeName);
    if (normalizedType != "Reference" &&
        normalizedType != "Pointer") {
      return false;
    }
    if (extractBuiltinSoaVectorElementTypeFromTypeText(
            binding.typeTemplateArg.empty()
                ? normalizedType
                : normalizedType + "<" + binding.typeTemplateArg + ">",
            elemTypeOut)) {
      return true;
    }
    return extractExperimentalSoaVectorElementType(binding, elemTypeOut);
  };

  if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
    return extractBorrowedBinding(*paramBinding);
  }
  if (auto it = locals.find(target.name); it != locals.end()) {
    return extractBorrowedBinding(it->second);
  }
  return false;
}

bool SemanticsValidator::resolveSoaVectorOrExperimentalBorrowedReceiver(
    const Expr &target,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveDirectReceiver,
    std::string &elemTypeOut) {
  auto withPreservedError = [&](const std::function<bool()> &fn) {
    const std::string previousError = error_;
    error_.clear();
    const bool ok = fn();
    error_.clear();
    error_ = previousError;
    return ok;
  };
  if (resolveDirectSoaVectorOrExperimentalBorrowedReceiver(
          target, params, locals, resolveDirectReceiver, elemTypeOut)) {
    return true;
  }
  if (target.kind != Expr::Kind::Call) {
    return false;
  }

  auto resolveValueExpr = [&](const Expr &valueExpr) -> bool {
    if (resolveDirectSoaVectorOrExperimentalBorrowedReceiver(
            valueExpr, params, locals, resolveDirectReceiver, elemTypeOut)) {
      return true;
    }
    if (valueExpr.kind != Expr::Kind::Call || valueExpr.isBinding) {
      return false;
    }
    std::string inferredTypeText;
    return withPreservedError([&]() {
             return inferQueryExprTypeText(valueExpr, params, locals, inferredTypeText);
           }) &&
           !inferredTypeText.empty() &&
           resolveExperimentalBorrowedSoaTypeText(inferredTypeText, elemTypeOut);
  };

  if (!target.isBinding && isSimpleCallName(target, "location") &&
      target.args.size() == 1) {
    return resolveValueExpr(target.args.front());
  }
  if (!target.isBinding && isSimpleCallName(target, "dereference") &&
      target.args.size() == 1) {
    const Expr &borrowedExpr = target.args.front();
    return borrowedExpr.kind == Expr::Kind::Call && !borrowedExpr.isBinding &&
           isSimpleCallName(borrowedExpr, "location") &&
           borrowedExpr.args.size() == 1 &&
           resolveValueExpr(borrowedExpr.args.front());
  }

  std::string inferredTypeText;
  return withPreservedError([&]() {
           return inferQueryExprTypeText(target, params, locals, inferredTypeText);
         }) &&
         !inferredTypeText.empty() &&
         resolveExperimentalBorrowedSoaTypeText(inferredTypeText, elemTypeOut);
}

} // namespace primec::semantics
