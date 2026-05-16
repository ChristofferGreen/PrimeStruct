#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {

namespace {

std::string stripGeneratedHelperSuffix(std::string path) {
  const size_t leafStart = path.find_last_of('/');
  const size_t searchStart = leafStart == std::string::npos ? 0 : leafStart + 1;
  if (const size_t templateSuffix = path.find("__t", searchStart);
      templateSuffix != std::string::npos) {
    path.erase(templateSuffix);
  } else if (const size_t overloadSuffix = path.find("__ov", searchStart);
             overloadSuffix != std::string::npos) {
    path.erase(overloadSuffix);
  }
  return path;
}

std::string canonicalMapHelperPathLocal(std::string_view helperName) {
  return canonicalCollectionHelperPath(StdlibSurfaceId::CollectionsMapHelpers,
                                       helperName);
}

bool resolveCanonicalMapHelperNameFromSpelling(std::string path,
                                               std::string &helperNameOut) {
  helperNameOut.clear();
  if (!path.empty() && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  return resolvePublishedCollectionHelperResolvedPath(
      path, StdlibSurfaceId::CollectionsMapHelpers, helperNameOut);
}

bool isCanonicalMapAccessResolvedPath(const std::string &path) {
  std::string helperName;
  if (!resolveCanonicalMapHelperNameFromSpelling(path, helperName)) {
    return false;
  }
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

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
  std::string resolvedMapHelperName;
  if (resolveCanonicalMapHelperNameFromSpelling(
          normalizedName, resolvedMapHelperName) &&
      (resolvedMapHelperName == "at_ref" ||
       resolvedMapHelperName == "at_unsafe_ref")) {
    helperOut = resolvedMapHelperName;
    return true;
  }
  return false;
}

bool isBuiltinSoaVectorTypeBaseForArgumentValidation(const std::string &base) {
  const std::string normalizedBase = normalizeBindingTypeName(base);
  return normalizedBase == "soa" "_vector" ||
         normalizedBase == "/soa" "_vector" ||
         normalizedBase == "std/collections/" "soa" "_vector" ||
         normalizedBase == "/std/collections/" "soa" "_vector" ||
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

bool definitionReturnTypeTextForArgumentValidation(const Definition &definition,
                                                   std::string &typeTextOut) {
  typeTextOut.clear();
  for (const auto &transform : definition.transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1) {
      typeTextOut = transform.templateArgs.front();
      return true;
    }
  }
  return false;
}

bool isSpecializedExperimentalMapBackingStructPath(std::string structPath) {
  structPath = normalizeBindingTypeName(std::move(structPath));
  if (!structPath.empty() && structPath.front() == '/') {
    structPath.erase(structPath.begin());
  }
  const size_t leafStart = structPath.find_last_of('/');
  const std::string leaf =
      leafStart == std::string::npos ? structPath : structPath.substr(leafStart + 1);
  return leaf != "Map" &&
         isExperimentalCollectionBackingTypeName("map", "Map", structPath);
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
    const std::string resolvedBasePath = stripGeneratedHelperSuffix(resolvedPath);
    auto methodMapAccessDefinitionReturnsString = [&]() {
      if (arg.args.size() != 2) {
        return false;
      }
      std::string helperName = arg.name;
      if (!helperName.empty() && helperName.front() == '/') {
        helperName.erase(helperName.begin());
      }
      const size_t helperLeaf = helperName.find_last_of('/');
      if (helperLeaf != std::string::npos) {
        helperName = helperName.substr(helperLeaf + 1);
      }
      helperName = stripGeneratedHelperSuffix(std::move(helperName));
      if (helperName != "at" && helperName != "at_unsafe") {
        if (!getCanonicalMapAccessBuiltinName(arg, helperName) ||
            (helperName != "at" && helperName != "at_unsafe")) {
          return false;
        }
      }
      std::string keyType;
      std::string valueType;
      bool receiverIsMap =
          dispatchResolvers.resolveMapTarget != nullptr &&
          dispatchResolvers.resolveMapTarget(arg.args.front(), keyType, valueType);
      if (!receiverIsMap && arg.args.front().kind == Expr::Kind::Call) {
        const std::string receiverPath = resolveCalleePath(arg.args.front());
        auto receiverDefIt = defMap_.find(receiverPath);
        if (receiverDefIt != defMap_.end() && receiverDefIt->second != nullptr) {
          std::string receiverReturnType;
          if (definitionReturnTypeTextForArgumentValidation(*receiverDefIt->second,
                                                            receiverReturnType)) {
            receiverIsMap =
                extractMapKeyValueTypesFromTypeText(receiverReturnType, keyType, valueType);
          }
        }
      }
      if (!receiverIsMap) {
        return false;
      }
      const std::string helperPath = canonicalMapHelperPathLocal(helperName);
      auto defIt = defMap_.find(helperPath);
      if (defIt == defMap_.end()) {
        for (auto candidateIt = defMap_.begin(); candidateIt != defMap_.end(); ++candidateIt) {
          if (stripGeneratedHelperSuffix(candidateIt->first) == helperPath) {
            defIt = candidateIt;
            break;
          }
        }
      }
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      std::string returnTypeText;
      return definitionReturnTypeTextForArgumentValidation(*defIt->second, returnTypeText) &&
             normalizeBindingTypeName(returnTypeText) == "string";
    };
    if (methodMapAccessDefinitionReturnsString()) {
      return true;
    }
    auto methodVectorAccessDefinitionReturnsString = [&]() {
      if (arg.args.size() != 2) {
        return false;
      }
      std::string helperName = arg.name;
      if (!helperName.empty() && helperName.front() == '/') {
        helperName.erase(helperName.begin());
      }
      const size_t helperLeaf = helperName.find_last_of('/');
      if (helperLeaf != std::string::npos) {
        helperName = helperName.substr(helperLeaf + 1);
      }
      helperName = stripGeneratedHelperSuffix(std::move(helperName));
      if (helperName != "at_unsafe") {
        return false;
      }
      std::string elemType;
      if (dispatchResolvers.resolveVectorTarget == nullptr ||
          !dispatchResolvers.resolveVectorTarget(arg.args.front(), elemType)) {
        return false;
      }
      const std::string canonicalAtUnsafe =
          canonicalVectorCompatibilityHelperPathOrFallback("at_unsafe");
      auto defIt = defMap_.find(canonicalAtUnsafe);
      if (defIt == defMap_.end()) {
        return false;
      }
      std::string returnTypeText;
      return defIt->second != nullptr &&
             definitionReturnTypeTextForArgumentValidation(*defIt->second, returnTypeText) &&
             normalizeBindingTypeName(returnTypeText) == "string";
    };
    if (methodVectorAccessDefinitionReturnsString()) {
      return true;
    }
    const bool isExplicitMapAccessPath =
        isCanonicalMapAccessResolvedPath(resolvedBasePath);
    if (isExplicitMapAccessPath) {
      auto defIt = defMap_.find(resolvedPath);
      if (defIt == defMap_.end()) {
        defIt = defMap_.find(resolvedBasePath);
      }
      if (defIt != defMap_.end() && defIt->second != nullptr) {
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name == "return" && transform.templateArgs.size() == 1) {
            return normalizeBindingTypeName(transform.templateArgs.front()) == "string";
          }
        }
      }
    }
    const bool treatAsBuiltinAccess =
        defMap_.find(resolvedPath) == defMap_.end() ||
        isStdNamespacedVectorCompatibilityHelperPath(resolvedPath, "at") ||
        isStdNamespacedVectorCompatibilityHelperPath(resolvedPath, "at_unsafe") ||
        isExplicitMapAccessPath;
    std::string accessName;
    if (treatAsBuiltinAccess &&
        getCanonicalMapAccessBuiltinName(arg, accessName) &&
        arg.args.size() == 2) {
      std::string mapValueType;
      std::string experimentalMapKeyType;
      std::string experimentalMapValueType;
      if (dispatchResolvers.resolveExperimentalMapTarget != nullptr &&
          dispatchResolvers.resolveExperimentalMapTarget(arg.args.front(),
                                                         experimentalMapKeyType,
                                                         experimentalMapValueType)) {
        return false;
      }
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
  if (!isSpecializedExperimentalMapBackingStructPath(structPath)) {
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
  if (!isLegacyExperimentalVectorCompatibilitySpecializedTypePath(structPath)) {
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
             isLegacyExperimentalVectorCompatibilityPath(
                 "/" + normalizedBase)) &&
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
      if (!isLegacyExperimentalVectorCompatibilitySpecializedTypePath(
              "/" + normalizedResolvedPath)) {
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
        if (normalizedBase == "soa" "_vector" ||
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
