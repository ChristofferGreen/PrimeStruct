#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <optional>

namespace primec::semantics {
namespace {

bool isExperimentalMapTypeText(const std::string &typeText) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base == "Map" || base == "/Map" ||
        base == "std/collections/experimental_map/Map" ||
        base == "/std/collections/experimental_map/Map") {
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

bool isUnqualifiedCollectionAccessCall(const Expr &candidate,
                                       const std::string &helperName) {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      !candidate.namespacePrefix.empty() || candidate.name.empty()) {
    return false;
  }
  std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  if (normalizedName.find('/') != std::string::npos) {
    return false;
  }
  std::string resolvedHelperName;
  return getBuiltinArrayAccessName(candidate, resolvedHelperName) &&
         resolvedHelperName == helperName;
}

bool isSourceSpelledCanonicalMapAccessCall(const Expr &expr) {
  const std::string &sourceName =
      expr.sourceName.empty() ? expr.name : expr.sourceName;
  return sourceName.rfind("/std/collections/map/", 0) == 0 ||
         sourceName.rfind("std/collections/map/", 0) == 0;
}

bool isRootMapConstructorExpr(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      candidate.name.empty()) {
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
  return normalizedName == "map" || normalizedName.rfind("map__", 0) == 0;
}

bool isPublishedMapConstructorExpr(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      candidate.name.empty()) {
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
  if (!normalizedName.empty() && normalizedName.front() != '/') {
    normalizedName.insert(normalizedName.begin(), '/');
  }
  return isResolvedMapConstructorPath(normalizedName);
}

std::string removedMapCompatibilityHelperFromPath(std::string path) {
  if (!path.empty() && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  if (path.rfind("/map/", 0) != 0) {
    return {};
  }
  std::string helper = path.substr(std::string("/map/").size());
  const size_t specializationSuffix = helper.find("__");
  if (specializationSuffix != std::string::npos) {
    helper.erase(specializationSuffix);
  }
  if (helper == "count" || helper == "count_ref" ||
      helper == "size" ||
      helper == "contains" || helper == "contains_ref" ||
      helper == "tryAt" || helper == "tryAt_ref" ||
      helper == "at" || helper == "at_ref" ||
      helper == "at_unsafe" || helper == "at_unsafe_ref" ||
      helper == "insert" || helper == "insert_ref") {
    return helper;
  }
  return {};
}

} // namespace

bool SemanticsValidator::validateExprPreDispatchDirectCalls(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprPreDispatchDirectCallContext &context,
    std::string &resolvedOut,
    std::optional<Expr> &rewrittenExprOut,
    bool &handledOut) {
  rewrittenExprOut.reset();
  handledOut = false;
  resolvedOut = resolveCalleePath(expr);
  auto bareMapWrapperHelperPath = [&](const Expr &candidate,
                                      std::string &helperNameOut) {
    helperNameOut.clear();
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
        candidate.name.empty() || !candidate.namespacePrefix.empty()) {
      return std::string();
    }
    const std::string normalizedName =
        std::string(trimLeadingSlash(candidate.name));
    if (!resolvePublishedCollectionHelperMemberToken(
            normalizedName,
            StdlibSurfaceId::CollectionsMapHelpers,
            helperNameOut) ||
        normalizedName == helperNameOut) {
      helperNameOut.clear();
      return std::string();
    }
    return "/std/collections/" + normalizedName;
  };
  std::string bareMapWrapperHelperName;
  const std::string normalizedBareMapWrapperHelperPath =
      bareMapWrapperHelperPath(expr, bareMapWrapperHelperName);
  if (!normalizedBareMapWrapperHelperPath.empty()) {
    const std::string removedCompatibilityPath =
        "/map/" + bareMapWrapperHelperName;
    auto hasDefinitionFamilyPath = [&](const std::string &path) {
      if (defMap_.count(path) > 0 || paramsByDef_.count(path) > 0) {
        return true;
      }
      const std::string templatedPrefix = path + "<";
      const std::string specializedPrefix = path + "__t";
      const std::string overloadPrefix = path + "__ov";
      for (const auto &def : program_.definitions) {
        if (def.fullPath == path || def.fullPath.rfind(templatedPrefix, 0) == 0 ||
            def.fullPath.rfind(specializedPrefix, 0) == 0 ||
            def.fullPath.rfind(overloadPrefix, 0) == 0) {
          return true;
        }
      }
      for (const auto &[candidatePath, paramsForDef] : paramsByDef_) {
        (void)paramsForDef;
        if (candidatePath == path || candidatePath.rfind(templatedPrefix, 0) == 0 ||
            candidatePath.rfind(specializedPrefix, 0) == 0 ||
            candidatePath.rfind(overloadPrefix, 0) == 0) {
          return true;
        }
      }
      return false;
    };
    const bool resolvesRemovedCompatibilityPath =
        resolvedOut == removedCompatibilityPath ||
        resolvedOut.rfind(removedCompatibilityPath + "__t", 0) == 0;
    if ((resolvedOut.empty() || resolvesRemovedCompatibilityPath) &&
        !hasDefinitionFamilyPath(removedCompatibilityPath) &&
        (hasImportedDefinitionPath(normalizedBareMapWrapperHelperPath) ||
         hasDeclaredDefinitionPath(normalizedBareMapWrapperHelperPath) ||
         defMap_.count(normalizedBareMapWrapperHelperPath) > 0)) {
      resolvedOut = normalizedBareMapWrapperHelperPath;
    }
  }
  auto failPreDispatchDirectCallDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (!expr.isMethodCall && expr.name.rfind("/file/", 0) == 0) {
    resolvedOut = expr.name;
    return true;
  }
  if (!expr.isMethodCall && resolvedOut == "/file_error/why") {
    return true;
  }
  if (expr.isFieldAccess) {
    return true;
  }
  const std::string removedMapCompatibilityHelper =
      removedMapCompatibilityHelperFromPath(resolvedOut);
  auto hasExactRemovedMapAliasDefinition = [&](const std::string &path) {
    if (defMap_.count(path) > 0 || paramsByDef_.count(path) > 0) {
      return true;
    }
    const std::string templatedPrefix = path + "<";
    const std::string specializedPrefix = path + "__";
    for (const auto &definition : program_.definitions) {
      if (definition.fullPath == path ||
          definition.fullPath.rfind(templatedPrefix, 0) == 0 ||
          definition.fullPath.rfind(specializedPrefix, 0) == 0) {
        return true;
      }
    }
    return false;
  };
  if (!expr.isMethodCall && !removedMapCompatibilityHelper.empty() &&
      !hasExactRemovedMapAliasDefinition("/map/" + removedMapCompatibilityHelper)) {
    handledOut = true;
    const std::string removedPath = "/map/" + removedMapCompatibilityHelper;
    auto canonicalHelperReturnsStruct = [&]() {
      if (removedMapCompatibilityHelper != "at" &&
          removedMapCompatibilityHelper != "at_unsafe" &&
          removedMapCompatibilityHelper != "at_ref" &&
          removedMapCompatibilityHelper != "at_unsafe_ref") {
        return false;
      }
      const std::string canonicalPath =
          "/std/collections/map/" + removedMapCompatibilityHelper;
      auto defIt = defMap_.find(canonicalPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string returnType = normalizeBindingTypeName(transform.templateArgs.front());
        if (!returnType.empty() && returnType.front() == '/') {
          returnType.erase(returnType.begin());
        }
        return !returnType.empty() && !isRootBuiltinName(returnType) &&
               returnType != "string" && returnType != "map" &&
               returnType != "vector" && returnType != "array";
      }
      return false;
    };
    if (canonicalHelperReturnsStruct()) {
      handledOut = false;
      return true;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failPreDispatchDirectCallDiagnostic(
          "block arguments require a definition target: " + removedPath);
    }
    const size_t expectedArgCount =
        (removedMapCompatibilityHelper == "count" ||
         removedMapCompatibilityHelper == "count_ref" ||
         removedMapCompatibilityHelper == "size")
            ? 1
            : ((removedMapCompatibilityHelper == "at" ||
                removedMapCompatibilityHelper == "at_ref" ||
                removedMapCompatibilityHelper == "at_unsafe" ||
                removedMapCompatibilityHelper == "at_unsafe_ref" ||
                removedMapCompatibilityHelper == "contains" ||
                removedMapCompatibilityHelper == "contains_ref" ||
                removedMapCompatibilityHelper == "tryAt" ||
                removedMapCompatibilityHelper == "tryAt_ref")
                   ? 2
                   : 3);
    if (expr.args.size() != expectedArgCount) {
      return failPreDispatchDirectCallDiagnostic(
          "argument count mismatch for " + removedPath);
    }
    return failPreDispatchDirectCallDiagnostic("unknown call target: " +
                                               removedPath);
  }
  auto failPreDispatchDirectCallMapKeyMismatch =
      [&](const std::string &helperName,
          const std::string &mapKeyType,
          const Expr &receiverExpr) {
        const std::string canonicalPath =
            "/std/collections/map/" + helperName;
        std::string receiverTypeText;
        const bool receiverIsExperimentalMap =
            inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
            isExperimentalMapTypeText(receiverTypeText);
        const bool canonicalMapAccessDiagnostic =
            isSourceSpelledCanonicalMapAccessCall(expr) ||
            expr.sourceIsMethodCall ||
            (receiverIsExperimentalMap &&
             expr.isMethodCall);
        if (canonicalMapAccessDiagnostic) {
          return failPreDispatchDirectCallDiagnostic(
              "argument type mismatch for " + canonicalPath +
              " parameter key");
        }
        if (normalizeBindingTypeName(mapKeyType) == "string") {
          return failPreDispatchDirectCallDiagnostic(helperName +
                                                     " requires string map key");
        }
        return failPreDispatchDirectCallDiagnostic(helperName +
                                                   " requires map key type " +
                                                   mapKeyType);
      };
  if (context.dispatchBootstrap == nullptr) {
    return true;
  }

  const auto &dispatchBootstrap = *context.dispatchBootstrap;
  auto sourceMethodMapHelperName = [](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
        !candidate.sourceIsMethodCall || !candidate.namespacePrefix.empty() ||
        candidate.name.empty()) {
      return {};
    }
    std::string helperName = candidate.name;
    if (!helperName.empty() && helperName.front() == '/') {
      helperName.erase(helperName.begin());
    }
    if (helperName == "count" || helperName == "count_ref" ||
        helperName == "size" ||
        helperName == "contains" || helperName == "contains_ref" ||
        helperName == "tryAt" || helperName == "tryAt_ref" ||
        helperName == "at" || helperName == "at_ref" ||
        helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
        helperName == "insert" || helperName == "insert_ref") {
      return helperName;
    }
    return {};
  };
  if (const std::string helperName = sourceMethodMapHelperName(expr);
      !helperName.empty() && !expr.args.empty()) {
    const size_t receiverIndex =
        this->mapHelperReceiverIndex(expr, dispatchBootstrap.dispatchResolvers);
    if (receiverIndex < expr.args.size()) {
      std::string keyType;
      std::string valueType;
      const bool isMapReceiver =
          dispatchBootstrap.dispatchResolvers.resolveMapTarget != nullptr &&
          dispatchBootstrap.dispatchResolvers.resolveMapTarget(
              expr.args[receiverIndex], keyType, valueType);
      keyType.clear();
      valueType.clear();
      const bool isExperimentalMapReceiver =
          dispatchBootstrap.dispatchResolvers.resolveExperimentalMapTarget != nullptr &&
          dispatchBootstrap.dispatchResolvers.resolveExperimentalMapTarget(
              expr.args[receiverIndex], keyType, valueType);
      if (isMapReceiver || isExperimentalMapReceiver) {
        resolvedOut = preferredBareMapHelperTarget(helperName);
      }
    }
  }
  auto isExperimentalMapReceiverExpr = [&](const Expr &candidate) {
    std::string receiverTypeText;
    if (inferQueryExprTypeText(candidate, params, locals, receiverTypeText) &&
        isExperimentalMapTypeText(receiverTypeText)) {
      return true;
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    auto defIt = defMap_.find(resolveCalleePath(candidate));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    BindingInfo inferredReturn;
    if (!inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      return false;
    }
    const std::string inferredTypeText =
        inferredReturn.typeTemplateArg.empty()
            ? inferredReturn.typeName
            : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
    return isExperimentalMapTypeText(inferredTypeText);
  };

  std::string canonicalExperimentalMapHelperResolved;
  if (!expr.isMethodCall &&
      (defMap_.count(resolvedOut) == 0 ||
       this->shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath(resolvedOut)) &&
      this->canonicalizeExperimentalMapHelperResolvedPath(
          resolvedOut, canonicalExperimentalMapHelperResolved)) {
    resolvedOut = canonicalExperimentalMapHelperResolved;
  }

  Expr rewrittenCanonicalExperimentalVectorHelperCall;
  if (this->tryRewriteCanonicalExperimentalVectorHelperCall(
          expr, dispatchBootstrap.dispatchResolvers,
          rewrittenCanonicalExperimentalVectorHelperCall)) {
    rewrittenExprOut = rewrittenCanonicalExperimentalVectorHelperCall;
    return true;
  }

  Expr rewrittenCanonicalExperimentalMapHelperCall;
  if (this->tryRewriteCanonicalExperimentalMapHelperCall(
          expr, dispatchBootstrap.dispatchResolvers,
          rewrittenCanonicalExperimentalMapHelperCall)) {
    rewrittenExprOut = rewrittenCanonicalExperimentalMapHelperCall;
    return true;
  }

  std::string borrowedCanonicalExperimentalMapHelperPath;
  if (!expr.isMethodCall &&
      this->explicitCanonicalExperimentalMapBorrowedHelperPath(
          expr, dispatchBootstrap.dispatchResolvers,
          borrowedCanonicalExperimentalMapHelperPath)) {
    return failPreDispatchDirectCallDiagnostic(
        "unknown call target: " + borrowedCanonicalExperimentalMapHelperPath);
  }

  if (!expr.isMethodCall &&
      (resolvedOut == "/std/collections/map/count" ||
       resolvedOut == "/std/collections/map/count_ref" ||
       resolvedOut == "/std/collections/map/contains" ||
       resolvedOut == "/std/collections/map/contains_ref" ||
       resolvedOut == "/std/collections/map/tryAt" ||
       resolvedOut == "/std/collections/map/tryAt_ref" ||
       resolvedOut == "/std/collections/map/at" ||
       resolvedOut == "/std/collections/map/at_ref" ||
       resolvedOut == "/std/collections/map/at_unsafe" ||
       resolvedOut == "/std/collections/map/at_unsafe_ref" ||
       resolvedOut == "/std/collections/map/insert" ||
       resolvedOut == "/std/collections/map/insert_ref") &&
      !hasImportedDefinitionPath(resolvedOut) &&
      !hasDeclaredDefinitionPath(resolvedOut)) {
    const size_t receiverIndex =
        this->mapHelperReceiverIndex(expr, dispatchBootstrap.dispatchResolvers);
    if (receiverIndex < expr.args.size()) {
      const Expr &receiverExpr = expr.args[receiverIndex];
      std::string keyType;
      std::string valueType;
      const bool isExperimentalMapTarget =
          dispatchBootstrap.dispatchResolvers.resolveExperimentalMapTarget != nullptr &&
          dispatchBootstrap.dispatchResolvers.resolveExperimentalMapTarget(
              receiverExpr, keyType, valueType);
      keyType.clear();
      valueType.clear();
      const bool isBuiltinMapTarget =
          dispatchBootstrap.dispatchResolvers.resolveMapTarget != nullptr &&
          dispatchBootstrap.dispatchResolvers.resolveMapTarget(
              receiverExpr, keyType, valueType);
      if (isBuiltinMapTarget && !isExperimentalMapTarget &&
          !isRootMapConstructorExpr(receiverExpr) &&
          !isPublishedMapConstructorExpr(receiverExpr)) {
        return failPreDispatchDirectCallDiagnostic("unknown call target: " +
                                                   resolvedOut);
      }
    }
  }

  if (expr.isMethodCall && expr.args.size() == 1 &&
      (expr.name == "count" || expr.name == "capacity")) {
    std::string receiverCollectionTypePath;
    if (expr.args.front().kind == Expr::Kind::Call &&
        resolveCallCollectionTypePath(expr.args.front(), params, locals,
                                      receiverCollectionTypePath) &&
        receiverCollectionTypePath == "/vector") {
      if ((expr.name == "count" || expr.name == "capacity") &&
          expr.templateArgs.empty() && !expr.hasBodyArguments &&
          expr.bodyArguments.empty()) {
        rewrittenExprOut = expr.args.front();
        return true;
      }
      rewrittenExprOut = expr;
      rewrittenExprOut->isMethodCall = false;
      rewrittenExprOut->namespacePrefix.clear();
      rewrittenExprOut->name = expr.name;
      return true;
    }
  }

  if (!expr.isMethodCall && expr.args.size() == 2) {
    std::string builtinAccessName;
    auto hasVisibleStdlibMapAccessDefinition = [&](const std::string &helperName) {
      const std::string path = "/std/collections/map/" + helperName;
      return hasImportedDefinitionPath(path) || hasDeclaredDefinitionPath(path);
    };
    const bool isBareMapAccessBuiltinSurface =
        getBuiltinArrayAccessName(expr, builtinAccessName) &&
        isUnqualifiedCollectionAccessCall(expr, builtinAccessName);
    if (!builtinAccessName.empty() &&
        hasVisibleStdlibMapAccessDefinition(builtinAccessName) &&
        (isBareMapAccessBuiltinSurface ||
         defMap_.find("/std/collections/map/" + builtinAccessName) == defMap_.end()) &&
        !hasDeclaredDefinitionPath("/map/" + builtinAccessName)) {
      size_t receiverIndex = 0;
      size_t keyIndex = 1;
      const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
          expr, dispatchBootstrap.dispatchResolvers, receiverIndex, keyIndex);
      const Expr &receiverExpr =
          hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
      const Expr &keyExpr =
          hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
      std::string receiverTypeText;
      std::string mapKeyType;
      std::string mapValueType;
      if (inferQueryExprTypeText(receiverExpr, params, locals,
                                 receiverTypeText) &&
          extractMapKeyValueTypesFromTypeText(receiverTypeText, mapKeyType,
                                             mapValueType)) {
        if (!mapKeyType.empty()) {
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            if (!this->isStringExprForArgumentValidation(
                    keyExpr, dispatchBootstrap.dispatchResolvers)) {
              return failPreDispatchDirectCallMapKeyMismatch(
                  builtinAccessName, mapKeyType, receiverExpr);
            }
          } else {
            ReturnKind keyKind =
                returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
            if (keyKind != ReturnKind::Unknown) {
              if (dispatchBootstrap.dispatchResolvers.resolveStringTarget(
                      keyExpr)) {
                return failPreDispatchDirectCallMapKeyMismatch(
                    builtinAccessName, mapKeyType, receiverExpr);
              }
              ReturnKind indexKind =
                  inferExprReturnKind(keyExpr, params, locals);
              if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
                return failPreDispatchDirectCallMapKeyMismatch(
                    builtinAccessName, mapKeyType, receiverExpr);
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
  }

  if (!expr.isMethodCall) {
    std::string builtinAccessName;
    std::string receiverTypeText;
    std::string borrowedHelperProbe;
    auto resolvesNonRootExperimentalMapValueTarget =
        [&](const Expr &candidate) {
          return !isRootMapConstructorExpr(candidate) &&
                 dispatchBootstrap.dispatchResolvers.resolveExperimentalMapValueTarget(
                     candidate, receiverTypeText, borrowedHelperProbe);
        };
    auto isNonRootExperimentalMapReceiverExpr = [&](const Expr &candidate) {
      return !isRootMapConstructorExpr(candidate) &&
             isExperimentalMapReceiverExpr(candidate);
    };
    if (getBuiltinArrayAccessName(expr, builtinAccessName) &&
        isUnqualifiedCollectionAccessCall(expr, builtinAccessName) &&
        expr.args.size() == 2 &&
        ((dispatchBootstrap.dispatchResolvers.resolveExperimentalMapValueTarget !=
          nullptr &&
          (resolvesNonRootExperimentalMapValueTarget(expr.args.front()) ||
           resolvesNonRootExperimentalMapValueTarget(expr.args[1]))) ||
         isNonRootExperimentalMapReceiverExpr(expr.args.front()) ||
         isNonRootExperimentalMapReceiverExpr(expr.args[1]))) {
      return failPreDispatchDirectCallDiagnostic(
          "unknown call target: /std/collections/map/" + builtinAccessName);
    }
  }

  return true;
}

} // namespace primec::semantics
