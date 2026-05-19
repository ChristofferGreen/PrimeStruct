#include "SemanticsValidator.h"
#include "MapConstructorHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <string>
#include <string_view>
#include <utility>

namespace primec::semantics {

namespace {

const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadataForLateAccess() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

std::string canonicalKeyValueHelperPathLocal(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataForLateAccess();
  if (metadata == nullptr) {
    return "";
  }
  return canonicalCollectionHelperPath(metadata->id, helperName);
}

std::string canonicalKeyValueHelperNamespaceLocal() {
  const StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataForLateAccess();
  if (metadata == nullptr) {
    return "";
  }
  std::string namespacePath(metadata->canonicalPath);
  if (!namespacePath.empty() && namespacePath.front() == '/') {
    namespacePath.erase(namespacePath.begin());
  }
  return namespacePath;
}

bool resolveCanonicalKeyValueHelperNameFromSpelling(
    std::string path, std::string &helperNameOut) {
  helperNameOut.clear();
  if (!path.empty() && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  const StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataForLateAccess();
  return metadata != nullptr &&
         resolvePublishedCollectionHelperResolvedPath(path, metadata->id,
                                                      helperNameOut);
}

bool isCanonicalKeyValueHelperResolvedPath(const std::string &path,
                                           std::string_view helperName) {
  std::string resolvedHelperName;
  return resolveCanonicalKeyValueHelperNameFromSpelling(path,
                                                        resolvedHelperName) &&
         resolvedHelperName == helperName;
}

bool isCanonicalKeyValueAccessHelperPath(const std::string &path) {
  std::string resolvedHelperName;
  if (!resolveCanonicalKeyValueHelperNameFromSpelling(path, resolvedHelperName)) {
    return false;
  }
  return resolvedHelperName == "at" || resolvedHelperName == "at_ref" ||
         resolvedHelperName == "at_unsafe" ||
         resolvedHelperName == "at_unsafe_ref";
}

bool getCanonicalKeyValueAccessBuiltinName(const Expr &candidate,
                                           std::string &helperOut) {
  helperOut.clear();
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      candidate.name.empty() || candidate.args.size() != 2) {
    return false;
  }
  if (getBuiltinArrayAccessName(candidate, helperOut)) {
    return true;
  }
  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string resolvedKeyValueHelperName;
  if (resolveCanonicalKeyValueHelperNameFromSpelling(
          normalizedName, resolvedKeyValueHelperName) &&
      (resolvedKeyValueHelperName == "at_ref" ||
       resolvedKeyValueHelperName == "at_unsafe_ref")) {
    helperOut = resolvedKeyValueHelperName;
    return true;
  }
  std::string namespacePrefix = candidate.namespacePrefix;
  if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
    namespacePrefix.erase(namespacePrefix.begin());
  }
  if (namespacePrefix == canonicalKeyValueHelperNamespaceLocal() &&
      (candidate.name == "at_ref" || candidate.name == "at_unsafe_ref")) {
    helperOut = candidate.name;
    return true;
  }
  if (candidate.name.find('/') == std::string::npos &&
      (candidate.name == "at_ref" || candidate.name == "at_unsafe_ref")) {
    helperOut = candidate.name;
    return true;
  }
  return false;
}

bool isSpecializedExperimentalMapBackingPath(std::string typeName) {
  typeName = normalizeBindingTypeName(typeName);
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  return isExperimentalCollectionBackingTypeName("map", "Map", typeName) &&
         typeName.find("__") != std::string::npos;
}

bool isExperimentalMapTypeText(const std::string &typeText) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string normalizedPath = normalizedType;
    if (!normalizedPath.empty() && normalizedPath.front() == '/') {
      normalizedPath.erase(normalizedPath.begin());
    }
    if (isSpecializedExperimentalMapBackingPath(normalizedPath)) {
      return true;
    }
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    std::string normalizedBase = base;
    if (!normalizedBase.empty() && normalizedBase.front() == '/') {
      normalizedBase.erase(normalizedBase.begin());
    }
    const size_t leafStart = normalizedBase.find_last_of('/');
    const std::string leaf = leafStart == std::string::npos
                                 ? normalizedBase
                                 : normalizedBase.substr(leafStart + 1);
    if (leaf == "Map" &&
        isExperimentalCollectionBackingTypeName(
            "map", "Map", normalizedBase)) {
      std::vector<std::string> args;
      return splitTopLevelTemplateArgs(arg, args) && args.size() == 2;
    }
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
        return false;
      }
      normalizedType = normalizeBindingTypeName(args.front());
      continue;
    }
    return false;
  }
}

bool isSourceSpelledCanonicalKeyValueAccessCall(const Expr &expr) {
  const std::string &sourceName =
      expr.sourceName.empty() ? expr.name : expr.sourceName;
  return isCanonicalKeyValueAccessHelperPath(sourceName);
}

} // namespace

bool SemanticsValidator::validateExprLateMapAccessBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    const ExprLateMapAccessBuiltinContext &context,
    bool &handledOut) {
  handledOut = false;
  if (context.dispatchResolvers == nullptr) {
    return true;
  }

  auto failLateKeyValueAccessDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  auto failLateKeyValueAccessKeyMismatch = [&](const std::string &helperName,
                                          const std::string &mapKeyType,
                                          const Expr &receiverExpr) {
    std::string receiverTypeText;
    const bool receiverIsExperimentalMap =
        inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
        isExperimentalMapTypeText(receiverTypeText);
    const bool canonicalKeyValueAccessDiagnostic =
        isSourceSpelledCanonicalKeyValueAccessCall(expr) ||
        expr.sourceIsMethodCall ||
        (receiverIsExperimentalMap && expr.isMethodCall);
    if (canonicalKeyValueAccessDiagnostic) {
      return failLateKeyValueAccessDiagnostic(
          "argument type mismatch for " +
          canonicalKeyValueHelperPathLocal(helperName) +
          " parameter key");
    }
    if (normalizeBindingTypeName(mapKeyType) == "string") {
      return failLateKeyValueAccessDiagnostic(helperName +
                                                " requires string map key");
    }
    return failLateKeyValueAccessDiagnostic(helperName +
                                              " requires map key type " +
                                              mapKeyType);
  };
  auto hasBareKeyValueContainsDefinition = [&]() {
    return hasImportedDefinitionPath(canonicalKeyValueHelperPathLocal("contains")) ||
           hasDeclaredDefinitionPath(canonicalKeyValueHelperPathLocal("contains")) ||
           hasImportedDefinitionPath(
               canonicalKeyValueHelperPathLocal("contains_ref")) ||
           hasDeclaredDefinitionPath(
               canonicalKeyValueHelperPathLocal("contains_ref")) ||
           hasImportedDefinitionPath("/contains") ||
           hasDeclaredDefinitionPath("/contains");
  };
  auto hasVisibleStdlibKeyValueAccessDefinition = [&](const std::string &helperName) {
    const std::string path = canonicalKeyValueHelperPathLocal(helperName);
    return hasImportedDefinitionPath(path) || hasDeclaredDefinitionPath(path);
  };
  auto isLocalRootMapAliasCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
        !hasDeclaredDefinitionPath("/map")) {
      return false;
    }
    const std::string resolvedCandidate = resolveCalleePath(candidate);
    return resolvedCandidate == "/map" ||
           resolvedCandidate.rfind("/map__", 0) == 0;
  };
  auto isMapLikeReceiver = [&](const Expr &candidate) {
    return !isLocalRootMapAliasCall(candidate) &&
           this->isMapLikeBareAccessReceiver(candidate, params, locals,
                                             *context.dispatchResolvers);
  };
  auto rootMapConstructorKeyType = [&](const Expr &candidate,
                                       std::string &keyTypeOut) {
    keyTypeOut.clear();
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
        candidate.name.empty() || candidate.templateArgs.size() != 2) {
      return false;
    }
    std::string normalizedName = candidate.name;
    if (!candidate.namespacePrefix.empty() &&
        normalizedName.find('/') == std::string::npos) {
      std::string normalizedPrefix = candidate.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      if (!normalizedPrefix.empty()) {
        normalizedName = normalizedPrefix + "/" + normalizedName;
      }
    }
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    if (normalizedName != "map" && normalizedName.rfind("map__", 0) != 0) {
      return false;
    }
    keyTypeOut = candidate.templateArgs.front();
    return true;
  };
  std::string builtinName;
  auto isExperimentalKeyValueReceiver = [&](const Expr &receiverExpr) {
    std::string ignoredKeyType;
    if (rootMapConstructorKeyType(receiverExpr, ignoredKeyType)) {
      return false;
    }
    std::string receiverTypeText;
    return inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
           isExperimentalMapTypeText(receiverTypeText);
  };

  if (!expr.isMethodCall &&
      getBuiltinArrayAccessName(expr, builtinName) &&
      expr.args.size() == 2 && !hasNamedArguments(expr.argNames)) {
    const Expr &receiverExpr = expr.args.front();
    if (isExperimentalKeyValueReceiver(receiverExpr)) {
      return failLateKeyValueAccessDiagnostic(
          "unknown call target: " + canonicalKeyValueHelperPathLocal(builtinName));
    }
  }

  if (!expr.isMethodCall &&
      getCanonicalKeyValueAccessBuiltinName(expr, builtinName) &&
      expr.args.size() == 2 && !hasNamedArguments(expr.argNames)) {
    if (!isMapLikeReceiver(expr.args.front()) &&
        isMapLikeReceiver(expr.args[1])) {
      Expr rewrittenKeyValueAccessCall = expr;
      std::swap(rewrittenKeyValueAccessCall.args[0],
                rewrittenKeyValueAccessCall.args[1]);
      handledOut = true;
      return validateExpr(params, locals, rewrittenKeyValueAccessCall);
    }
  }

  if (!expr.isMethodCall && isSimpleCallName(expr, "contains") &&
      !context.shouldBuiltinValidateBareMapContainsCall) {
    Expr rewrittenKeyValueHelperCall;
    if (this->tryRewriteBareKeyValueHelperCall(expr, "contains",
                                          *context.dispatchResolvers,
                                          rewrittenKeyValueHelperCall)) {
      handledOut = true;
      return validateExpr(params, locals, rewrittenKeyValueHelperCall);
    }
  }

  if (!expr.isMethodCall && isSimpleCallName(expr, "tryAt") &&
      !context.shouldBuiltinValidateBareMapTryAtCall) {
    Expr rewrittenKeyValueHelperCall;
    if (this->tryRewriteBareKeyValueHelperCall(expr, "tryAt",
                                          *context.dispatchResolvers,
                                          rewrittenKeyValueHelperCall)) {
      if (isCanonicalKeyValueHelperResolvedPath(
              rewrittenKeyValueHelperCall.name, "tryAt") &&
          !hasVisibleStdlibKeyValueAccessDefinition("tryAt") &&
          !hasImportedDefinitionPath("/tryAt") &&
          !hasDeclaredDefinitionPath("/tryAt")) {
        return failLateKeyValueAccessDiagnostic(
            "unknown call target: " + canonicalKeyValueHelperPathLocal("tryAt"));
      }
      handledOut = true;
      return validateExpr(params, locals, rewrittenKeyValueHelperCall);
    }
  }

  if (!expr.isMethodCall &&
      getCanonicalKeyValueAccessBuiltinName(expr, builtinName) &&
      !context.shouldBuiltinValidateBareMapAccessCall) {
    Expr rewrittenKeyValueHelperCall;
    if (this->tryRewriteBareKeyValueHelperCall(expr, builtinName,
                                          *context.dispatchResolvers,
                                          rewrittenKeyValueHelperCall)) {
      handledOut = true;
      return validateExpr(params, locals, rewrittenKeyValueHelperCall);
    }
  }

  auto resolveMapKeyTypeWithInference =
      [&](const Expr &receiverExpr, std::string &mapKeyTypeOut) {
        if (this->resolveMapKeyType(receiverExpr, *context.dispatchResolvers,
                                    mapKeyTypeOut)) {
          return true;
        }
        std::string receiverTypeText;
        std::string mapValueType;
        return inferQueryExprTypeText(receiverExpr, params, locals,
                                      receiverTypeText) &&
               extractMapKeyValueTypesFromTypeText(receiverTypeText,
                                                  mapKeyTypeOut, mapValueType);
      };

  if (!expr.isMethodCall && isSimpleCallName(expr, "contains") &&
      expr.args.size() == 2 &&
      (context.shouldBuiltinValidateBareMapContainsCall ||
       isMapLikeReceiver(
           expr.args[this->keyValueHelperReceiverIndex(expr, *context.dispatchResolvers)]) ||
       this->isIndexedArgsPackMapReceiverTarget(
           expr.args.front(), *context.dispatchResolvers))) {
    if (!hasBareKeyValueContainsDefinition()) {
      return failLateKeyValueAccessDiagnostic(
          "unknown call target: " + canonicalKeyValueHelperPathLocal("contains"));
    }
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareKeyValueOperands =
        this->bareKeyValueHelperOperandIndices(expr, *context.dispatchResolvers,
                                          receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareKeyValueOperands ? expr.args[receiverIndex] : expr.args.front();
    const Expr &keyExpr =
        hasBareKeyValueOperands ? expr.args[keyIndex] : expr.args[1];
    std::string mapKeyType;
    if (!resolveMapKeyTypeWithInference(receiverExpr, mapKeyType)) {
      if (!validateExpr(params, locals, receiverExpr)) {
        return false;
      }
      return failLateKeyValueAccessDiagnostic("contains requires map target");
    }
    if (!mapKeyType.empty()) {
      if (normalizeBindingTypeName(mapKeyType) == "string") {
        if (!this->isStringExprForArgumentValidation(keyExpr,
                                                     *context.dispatchResolvers)) {
          return failLateKeyValueAccessDiagnostic(
              "contains requires string map key");
        }
      } else {
        ReturnKind keyKind =
            returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
        if (keyKind != ReturnKind::Unknown) {
          if (context.dispatchResolvers->resolveStringTarget(keyExpr)) {
            return failLateKeyValueAccessDiagnostic(
                "contains requires map key type " + mapKeyType);
          }
          ReturnKind candidateKind = inferExprReturnKind(keyExpr, params, locals);
          if (candidateKind != ReturnKind::Unknown &&
              candidateKind != keyKind) {
            return failLateKeyValueAccessDiagnostic(
                "contains requires map key type " + mapKeyType);
          }
        }
      }
    }
    if (!validateExpr(params, locals, expr.args.front()) ||
        !validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    handledOut = true;
    return true;
  }

  if (((expr.isMethodCall &&
        (isCanonicalKeyValueHelperResolvedPath(resolved, "tryAt") ||
         isCanonicalKeyValueHelperResolvedPath(resolved, "tryAt_ref"))) ||
       (!expr.isMethodCall &&
        (isSimpleCallName(expr, "tryAt") ||
         isCanonicalKeyValueHelperResolvedPath(resolved, "tryAt")))) &&
      expr.args.size() == 2 &&
      (expr.isMethodCall || !hasDeclaredDefinitionPath(resolved)) &&
      (context.shouldBuiltinValidateBareMapTryAtCall ||
       isMapLikeReceiver(
           expr.args[this->keyValueHelperReceiverIndex(expr, *context.dispatchResolvers)]) ||
       this->isIndexedArgsPackMapReceiverTarget(
           expr.args[this->keyValueHelperReceiverIndex(expr, *context.dispatchResolvers)],
           *context.dispatchResolvers))) {
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareKeyValueOperands =
        this->bareKeyValueHelperOperandIndices(expr, *context.dispatchResolvers,
                                          receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareKeyValueOperands ? expr.args[receiverIndex] : expr.args.front();
    const Expr &keyExpr =
        hasBareKeyValueOperands ? expr.args[keyIndex] : expr.args[1];
    const bool resolvesTryAtRef =
        isCanonicalKeyValueHelperResolvedPath(resolved, "tryAt_ref");
    const bool resolvesCanonicalTryAt =
        isCanonicalKeyValueHelperResolvedPath(resolved, "tryAt");
    if (((!expr.isMethodCall && resolvesCanonicalTryAt) || resolvesTryAtRef) &&
        !hasImportedDefinitionPath(
            canonicalKeyValueHelperPathLocal(resolvesTryAtRef ? "tryAt_ref"
                                                         : "tryAt")) &&
        !hasDeclaredDefinitionPath(
            canonicalKeyValueHelperPathLocal(resolvesTryAtRef ? "tryAt_ref"
                                                         : "tryAt")) &&
        !hasImportedDefinitionPath("/tryAt") &&
        !hasDeclaredDefinitionPath("/tryAt")) {
      return failLateKeyValueAccessDiagnostic(
          "unknown call target: " +
          canonicalKeyValueHelperPathLocal(resolvesTryAtRef ? "tryAt_ref"
                                                       : "tryAt"));
    }
    std::string mapKeyType;
    if (!resolveMapKeyTypeWithInference(receiverExpr, mapKeyType)) {
      if (!validateExpr(params, locals, receiverExpr)) {
        return false;
      }
      return failLateKeyValueAccessDiagnostic("tryAt requires map target");
    }
    if (!mapKeyType.empty()) {
      if (normalizeBindingTypeName(mapKeyType) == "string") {
        if (!this->isStringExprForArgumentValidation(keyExpr,
                                                     *context.dispatchResolvers)) {
          return failLateKeyValueAccessDiagnostic(
              "tryAt requires string map key");
        }
      } else {
        ReturnKind keyKind =
            returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
        if (keyKind != ReturnKind::Unknown) {
          if (context.dispatchResolvers->resolveStringTarget(keyExpr)) {
            return failLateKeyValueAccessDiagnostic(
                "tryAt requires map key type " + mapKeyType);
          }
          ReturnKind candidateKind = inferExprReturnKind(keyExpr, params, locals);
          if (candidateKind != ReturnKind::Unknown &&
              candidateKind != keyKind) {
            return failLateKeyValueAccessDiagnostic(
                "tryAt requires map key type " + mapKeyType);
          }
        }
      }
    }
    if (!validateExpr(params, locals, expr.args.front()) ||
        !validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    handledOut = true;
    return true;
  }

  auto hasResolvedDefinition = [&](const std::string &path) {
    const auto it = defMap_.find(path);
    return it != defMap_.end() && it->second != nullptr;
  };
  std::string ignoredRootMapKeyType;
  if (!expr.isMethodCall &&
      getCanonicalKeyValueAccessBuiltinName(expr, builtinName) &&
      expr.args.size() == 2 &&
      !hasResolvedDefinition(resolved) &&
      (context.shouldBuiltinValidateBareMapAccessCall ||
       isMapLikeReceiver(
           expr.args[this->keyValueHelperReceiverIndex(expr, *context.dispatchResolvers)]) ||
       rootMapConstructorKeyType(
           expr.args[this->keyValueHelperReceiverIndex(expr, *context.dispatchResolvers)],
           ignoredRootMapKeyType) ||
       this->isIndexedArgsPackMapReceiverTarget(
           expr.args[this->keyValueHelperReceiverIndex(expr, *context.dispatchResolvers)],
           *context.dispatchResolvers))) {
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareKeyValueOperands =
        this->bareKeyValueHelperOperandIndices(expr, *context.dispatchResolvers,
                                          receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareKeyValueOperands ? expr.args[receiverIndex] : expr.args.front();
    const Expr &keyExpr =
        hasBareKeyValueOperands ? expr.args[keyIndex] : expr.args[1];
    if (isCanonicalKeyValueAccessHelperPath(expr.name) &&
        !hasVisibleStdlibKeyValueAccessDefinition(builtinName)) {
      return failLateKeyValueAccessDiagnostic(
          "unknown call target: " + canonicalKeyValueHelperPathLocal(builtinName));
    }
    std::string mapKeyType;
    if (rootMapConstructorKeyType(receiverExpr, mapKeyType) ||
        resolveMapKeyTypeWithInference(receiverExpr, mapKeyType)) {
      if (!mapKeyType.empty()) {
        if (normalizeBindingTypeName(mapKeyType) == "string") {
          if (!this->isStringExprForArgumentValidation(keyExpr,
                                                       *context.dispatchResolvers)) {
            return failLateKeyValueAccessKeyMismatch(builtinName, mapKeyType, receiverExpr);
          }
        } else {
          ReturnKind keyKind =
              returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
          if (keyKind != ReturnKind::Unknown) {
            if (context.dispatchResolvers->resolveStringTarget(keyExpr)) {
              return failLateKeyValueAccessKeyMismatch(builtinName, mapKeyType, receiverExpr);
            }
            ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
            if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
              return failLateKeyValueAccessKeyMismatch(builtinName, mapKeyType, receiverExpr);
            }
          }
        }
      }
      if (!validateExpr(params, locals, expr.args.front()) ||
          !validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      handledOut = true;
      return true;
    }
  }

  if (!expr.isMethodCall &&
      getCanonicalKeyValueAccessBuiltinName(expr, builtinName) &&
      expr.args.size() == 2 && !hasResolvedDefinition(resolved) &&
      hasVisibleStdlibKeyValueAccessDefinition(builtinName)) {
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareKeyValueOperands =
        this->bareKeyValueHelperOperandIndices(expr, *context.dispatchResolvers,
                                          receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareKeyValueOperands ? expr.args[receiverIndex] : expr.args.front();
    const Expr &keyExpr =
        hasBareKeyValueOperands ? expr.args[keyIndex] : expr.args[1];
    std::string receiverTypeText;
    std::string mapKeyType;
    std::string mapValueType;
    if (rootMapConstructorKeyType(receiverExpr, mapKeyType) ||
        (inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
         extractMapKeyValueTypesFromTypeText(receiverTypeText, mapKeyType,
                                            mapValueType))) {
      if (!mapKeyType.empty()) {
        if (normalizeBindingTypeName(mapKeyType) == "string") {
          if (!this->isStringExprForArgumentValidation(keyExpr,
                                                       *context.dispatchResolvers)) {
            return failLateKeyValueAccessKeyMismatch(builtinName, mapKeyType, receiverExpr);
          }
        } else {
          ReturnKind keyKind =
              returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
          if (keyKind != ReturnKind::Unknown) {
            if (context.dispatchResolvers->resolveStringTarget(keyExpr)) {
              return failLateKeyValueAccessKeyMismatch(builtinName, mapKeyType, receiverExpr);
            }
            ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
            if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
              return failLateKeyValueAccessKeyMismatch(builtinName, mapKeyType, receiverExpr);
            }
          }
        }
      }
      if (!validateExpr(params, locals, expr.args.front()) ||
          !validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      handledOut = true;
      return true;
    }
  }

  return true;
}

} // namespace primec::semantics
