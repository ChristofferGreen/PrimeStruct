#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <optional>
#include <string_view>
#include <utility>

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

const StdlibSurfaceMetadata *preDispatchKeyValueHelperSurfaceMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

std::string canonicalKeyValueHelperPathLocal(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = preDispatchKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return {};
  }
  return canonicalCollectionHelperPath(metadata->id, helperName);
}

bool resolvePreDispatchKeyValueHelperResolvedPath(std::string_view path,
                                                  std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      preDispatchKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return false;
  }
  return resolvePublishedCollectionHelperResolvedPath(
      path, metadata->id, helperNameOut);
}

std::string stripGeneratedKeyValueHelperSuffix(std::string path) {
  const size_t leafStart = path.find_last_of('/');
  const size_t searchStart = leafStart == std::string::npos ? 0 : leafStart + 1;
  if (const size_t generatedSuffix = path.find("__", searchStart);
      generatedSuffix != std::string::npos) {
    path.erase(generatedSuffix);
  }
  return path;
}

bool resolveCanonicalKeyValueHelperNameFromSpelling(std::string path,
                                                    std::string &helperNameOut) {
  helperNameOut.clear();
  if (preDispatchKeyValueHelperSurfaceMetadata() == nullptr) {
    return false;
  }
  if (!path.empty() && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  if (resolvePreDispatchKeyValueHelperResolvedPath(path, helperNameOut)) {
    return true;
  }
  path = stripGeneratedKeyValueHelperSuffix(std::move(path));
  return resolvePreDispatchKeyValueHelperResolvedPath(path, helperNameOut);
}

bool resolvePreDispatchKeyValueHelperMemberToken(std::string_view memberToken,
                                                 std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      preDispatchKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return false;
  }
  std::string normalizedToken(memberToken);
  if (!normalizedToken.empty() &&
      normalizedToken.find('/') != std::string::npos &&
      normalizedToken.front() != '/') {
    normalizedToken.insert(normalizedToken.begin(), '/');
  }
  return resolvePublishedCollectionHelperMemberToken(
      normalizedToken, metadata->id, helperNameOut);
}

bool isCanonicalKeyValueAccessReturnStructHelperName(std::string_view helperName) {
  return helperName == "at" || helperName == "at_unsafe" ||
         helperName == "at_ref" || helperName == "at_unsafe_ref";
}

bool isCanonicalMapBuiltinPreDispatchHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         isCanonicalKeyValueAccessReturnStructHelperName(helperName) ||
         helperName == "insert" || helperName == "insert_ref";
}

bool isRemovedKeyValueCompatibilityPreDispatchHelperName(
    std::string_view helperName) {
  return helperName == "size" ||
         isCanonicalMapBuiltinPreDispatchHelperName(helperName);
}

bool isSourceSpelledCanonicalKeyValueAccessCall(const Expr &expr) {
  const std::string &sourceName =
      expr.sourceName.empty() ? expr.name : expr.sourceName;
  std::string helperName;
  return resolveCanonicalKeyValueHelperNameFromSpelling(sourceName, helperName);
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

std::string removedKeyValueCompatibilityHelperFromPath(std::string path) {
  const StdlibSurfaceMetadata *metadata =
      preDispatchKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return {};
  }
  if (!path.empty() && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  path = stripGeneratedKeyValueHelperSuffix(std::move(path));
  for (std::string_view alias : metadata->importAliasSpellings) {
    std::string root(alias);
    if (root.empty()) {
      continue;
    }
    if (root.front() != '/') {
      root.insert(root.begin(), '/');
    }
    const std::string unrootedRoot =
        std::string(trimLeadingSlash(root));
    if (unrootedRoot.find('/') != std::string::npos ||
        path.size() <= root.size() || path.rfind(root, 0) != 0 ||
        path[root.size()] != '/') {
      continue;
    }
    const std::string helper = path.substr(root.size() + 1);
    if (isRemovedKeyValueCompatibilityPreDispatchHelperName(helper)) {
      return helper;
    }
  }
  return {};
}

std::string rootedKeyValueAliasHelperPath(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata =
      preDispatchKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return {};
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    std::string root(alias);
    if (root.empty()) {
      continue;
    }
    if (root.front() != '/') {
      root.insert(root.begin(), '/');
    }
    const std::string unrootedRoot =
        std::string(trimLeadingSlash(root));
    if (unrootedRoot.find('/') != std::string::npos) {
      continue;
    }
    return root + "/" + std::string(helperName);
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
  auto bareKeyValueWrapperHelperPath = [&](const Expr &candidate,
                                      std::string &helperNameOut) {
    helperNameOut.clear();
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
        candidate.name.empty() || !candidate.namespacePrefix.empty()) {
      return std::string();
    }
    const std::string normalizedName =
        std::string(trimLeadingSlash(candidate.name));
    if (!resolvePreDispatchKeyValueHelperMemberToken(
            normalizedName,
            helperNameOut) ||
        normalizedName == helperNameOut) {
      helperNameOut.clear();
      return std::string();
    }
    return canonicalKeyValueHelperPathLocal(helperNameOut);
  };
  std::string bareKeyValueWrapperHelperName;
  const std::string normalizedBareKeyValueWrapperHelperPath =
      bareKeyValueWrapperHelperPath(expr, bareKeyValueWrapperHelperName);
  if (!normalizedBareKeyValueWrapperHelperPath.empty()) {
    const std::string removedCompatibilityPath =
        rootedKeyValueAliasHelperPath(bareKeyValueWrapperHelperName);
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
    if (!removedCompatibilityPath.empty() &&
        (resolvedOut.empty() || resolvesRemovedCompatibilityPath) &&
        !hasDefinitionFamilyPath(removedCompatibilityPath) &&
        (hasImportedDefinitionPath(normalizedBareKeyValueWrapperHelperPath) ||
         hasDeclaredDefinitionPath(normalizedBareKeyValueWrapperHelperPath) ||
         defMap_.count(normalizedBareKeyValueWrapperHelperPath) > 0)) {
      resolvedOut = normalizedBareKeyValueWrapperHelperPath;
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
  const std::string explicitCallPath = [&]() {
    if (expr.isMethodCall || expr.name.empty()) {
      return std::string{};
    }
    if (expr.name.front() == '/') {
      return expr.name;
    }
    std::string prefix = expr.namespacePrefix;
    if (!prefix.empty() && prefix.front() != '/') {
      prefix.insert(prefix.begin(), '/');
    }
    return prefix.empty() ? "/" + expr.name : prefix + "/" + expr.name;
  }();
  std::string removedKeyValueCompatibilityHelper =
      removedKeyValueCompatibilityHelperFromPath(resolvedOut);
  if (removedKeyValueCompatibilityHelper.empty()) {
    removedKeyValueCompatibilityHelper =
        removedKeyValueCompatibilityHelperFromPath(explicitCallPath);
  }
  auto hasExactRemovedKeyValueAliasDefinition = [&](const std::string &path) {
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
  const std::string removedAliasPath =
      rootedKeyValueAliasHelperPath(removedKeyValueCompatibilityHelper);
  if (!expr.isMethodCall && !removedKeyValueCompatibilityHelper.empty() &&
      !removedAliasPath.empty() &&
      !hasExactRemovedKeyValueAliasDefinition(removedAliasPath)) {
    handledOut = true;
    const std::string removedPath = removedAliasPath;
    auto canonicalHelperReturnsStruct = [&]() {
      if (!isCanonicalKeyValueAccessReturnStructHelperName(
              removedKeyValueCompatibilityHelper)) {
        return false;
      }
      const std::string canonicalPath =
          canonicalKeyValueHelperPathLocal(removedKeyValueCompatibilityHelper);
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
        (removedKeyValueCompatibilityHelper == "count" ||
         removedKeyValueCompatibilityHelper == "count_ref" ||
         removedKeyValueCompatibilityHelper == "size")
            ? 1
            : ((removedKeyValueCompatibilityHelper == "at" ||
                removedKeyValueCompatibilityHelper == "at_ref" ||
                removedKeyValueCompatibilityHelper == "at_unsafe" ||
                removedKeyValueCompatibilityHelper == "at_unsafe_ref" ||
                removedKeyValueCompatibilityHelper == "contains" ||
                removedKeyValueCompatibilityHelper == "contains_ref" ||
                removedKeyValueCompatibilityHelper == "tryAt" ||
                removedKeyValueCompatibilityHelper == "tryAt_ref")
                   ? 2
                   : 3);
    if (expr.args.size() != expectedArgCount) {
      return failPreDispatchDirectCallDiagnostic(
          "argument count mismatch for " + removedPath);
    }
    return failPreDispatchDirectCallDiagnostic("unknown call target: " +
                                               removedPath);
  }
  auto failPreDispatchDirectCallKeyValueKeyMismatch =
      [&](const std::string &helperName,
          const std::string &keyValueKeyType,
          const Expr &receiverExpr) {
        const std::string canonicalPath =
            canonicalKeyValueHelperPathLocal(helperName);
        std::string receiverTypeText;
        const bool receiverIsExperimentalKeyValue =
            inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
            isExperimentalMapTypeText(receiverTypeText);
        const bool canonicalKeyValueAccessDiagnostic =
            isSourceSpelledCanonicalKeyValueAccessCall(expr) ||
            expr.sourceIsMethodCall ||
            (receiverIsExperimentalKeyValue &&
             expr.isMethodCall);
        if (canonicalKeyValueAccessDiagnostic) {
          return failPreDispatchDirectCallDiagnostic(
              "argument type mismatch for " + canonicalPath +
              " parameter key");
        }
        if (normalizeBindingTypeName(keyValueKeyType) == "string") {
          return failPreDispatchDirectCallDiagnostic(helperName +
                                                     " requires string map key");
        }
        return failPreDispatchDirectCallDiagnostic(helperName +
                                                   " requires map key type " +
                                                   keyValueKeyType);
      };
  if (context.dispatchBootstrap == nullptr) {
    return true;
  }

  const auto &dispatchBootstrap = *context.dispatchBootstrap;
  auto sourceMethodKeyValueHelperName = [](const Expr &candidate) -> std::string {
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
  if (const std::string helperName = sourceMethodKeyValueHelperName(expr);
      !helperName.empty() && !expr.args.empty()) {
    const size_t receiverIndex =
        this->keyValueHelperReceiverIndex(expr, dispatchBootstrap.dispatchResolvers);
    if (receiverIndex < expr.args.size()) {
      std::string keyType;
      std::string valueType;
      const bool isKeyValueReceiver =
          dispatchBootstrap.dispatchResolvers.resolveMapTarget != nullptr &&
          dispatchBootstrap.dispatchResolvers.resolveMapTarget(
              expr.args[receiverIndex], keyType, valueType);
      keyType.clear();
      valueType.clear();
      const bool isExperimentalKeyValueReceiver =
          dispatchBootstrap.dispatchResolvers.resolveExperimentalMapTarget != nullptr &&
          dispatchBootstrap.dispatchResolvers.resolveExperimentalMapTarget(
              expr.args[receiverIndex], keyType, valueType);
      if (isKeyValueReceiver || isExperimentalKeyValueReceiver) {
        resolvedOut = preferredBareKeyValueHelperTarget(helperName);
      }
    }
  }
  auto isExperimentalKeyValueReceiverExpr = [&](const Expr &candidate) {
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

  std::string canonicalExperimentalKeyValueHelperResolved;
  if (!expr.isMethodCall &&
      (defMap_.count(resolvedOut) == 0 ||
       this->shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath(
           resolvedOut)) &&
      this->canonicalizeExperimentalKeyValueHelperResolvedPath(
          resolvedOut, canonicalExperimentalKeyValueHelperResolved)) {
    resolvedOut = canonicalExperimentalKeyValueHelperResolved;
  }

  Expr rewrittenCanonicalExperimentalVectorHelperCall;
  if (this->tryRewriteCanonicalExperimentalVectorHelperCall(
          expr, dispatchBootstrap.dispatchResolvers,
          rewrittenCanonicalExperimentalVectorHelperCall)) {
    rewrittenExprOut = rewrittenCanonicalExperimentalVectorHelperCall;
    return true;
  }

  Expr rewrittenCanonicalExperimentalKeyValueHelperCall;
  if (this->tryRewriteCanonicalExperimentalKeyValueHelperCall(
          expr, dispatchBootstrap.dispatchResolvers,
          rewrittenCanonicalExperimentalKeyValueHelperCall)) {
    rewrittenExprOut = rewrittenCanonicalExperimentalKeyValueHelperCall;
    return true;
  }

  std::string borrowedCanonicalExperimentalKeyValueHelperPath;
  if (!expr.isMethodCall &&
      this->explicitCanonicalExperimentalKeyValueBorrowedHelperPath(
          expr, dispatchBootstrap.dispatchResolvers,
          borrowedCanonicalExperimentalKeyValueHelperPath)) {
    return failPreDispatchDirectCallDiagnostic(
        "unknown call target: " +
        borrowedCanonicalExperimentalKeyValueHelperPath);
  }

  std::string resolvedCanonicalKeyValueHelperName;
  if (!expr.isMethodCall &&
      resolveCanonicalKeyValueHelperNameFromSpelling(
          resolvedOut, resolvedCanonicalKeyValueHelperName) &&
      isCanonicalMapBuiltinPreDispatchHelperName(
          resolvedCanonicalKeyValueHelperName) &&
      !hasImportedDefinitionPath(resolvedOut) &&
      !hasDeclaredDefinitionPath(resolvedOut)) {
    const size_t receiverIndex =
        this->keyValueHelperReceiverIndex(expr, dispatchBootstrap.dispatchResolvers);
    if (receiverIndex < expr.args.size()) {
      const Expr &receiverExpr = expr.args[receiverIndex];
      std::string keyType;
      std::string valueType;
      const bool isExperimentalKeyValueTarget =
          dispatchBootstrap.dispatchResolvers.resolveExperimentalMapTarget != nullptr &&
          dispatchBootstrap.dispatchResolvers.resolveExperimentalMapTarget(
              receiverExpr, keyType, valueType);
      keyType.clear();
      valueType.clear();
      const bool isBuiltinKeyValueTarget =
          dispatchBootstrap.dispatchResolvers.resolveMapTarget != nullptr &&
          dispatchBootstrap.dispatchResolvers.resolveMapTarget(
              receiverExpr, keyType, valueType);
      if (isBuiltinKeyValueTarget && !isExperimentalKeyValueTarget &&
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
    auto hasVisibleStdlibKeyValueAccessDefinition = [&](const std::string &helperName) {
      const std::string path = canonicalKeyValueHelperPathLocal(helperName);
      return hasImportedDefinitionPath(path) || hasDeclaredDefinitionPath(path);
    };
    const bool isBareKeyValueAccessBuiltinSurface =
        getBuiltinArrayAccessName(expr, builtinAccessName) &&
        isUnqualifiedCollectionAccessCall(expr, builtinAccessName);
    const std::string builtinAccessPath =
        canonicalKeyValueHelperPathLocal(builtinAccessName);
    const std::string rootedBuiltinAccessAlias =
        rootedKeyValueAliasHelperPath(builtinAccessName);
    if (!builtinAccessName.empty() &&
        !rootedBuiltinAccessAlias.empty() &&
        hasVisibleStdlibKeyValueAccessDefinition(builtinAccessName) &&
        (isBareKeyValueAccessBuiltinSurface ||
         defMap_.find(builtinAccessPath) == defMap_.end()) &&
        !hasDeclaredDefinitionPath(rootedBuiltinAccessAlias)) {
      size_t receiverIndex = 0;
      size_t keyIndex = 1;
      const bool hasBareKeyValueOperands = this->bareKeyValueHelperOperandIndices(
          expr, dispatchBootstrap.dispatchResolvers, receiverIndex, keyIndex);
      const Expr &receiverExpr =
          hasBareKeyValueOperands ? expr.args[receiverIndex] : expr.args.front();
      const Expr &keyExpr =
          hasBareKeyValueOperands ? expr.args[keyIndex] : expr.args[1];
      std::string receiverTypeText;
      std::string keyValueKeyType;
      std::string keyValueValueType;
      if (inferQueryExprTypeText(receiverExpr, params, locals,
                                 receiverTypeText) &&
          extractMapKeyValueTypesFromTypeText(receiverTypeText, keyValueKeyType,
                                             keyValueValueType)) {
        if (!keyValueKeyType.empty()) {
          if (normalizeBindingTypeName(keyValueKeyType) == "string") {
            if (!this->isStringExprForArgumentValidation(
                    keyExpr, dispatchBootstrap.dispatchResolvers)) {
              return failPreDispatchDirectCallKeyValueKeyMismatch(
                  builtinAccessName, keyValueKeyType, receiverExpr);
            }
          } else {
            ReturnKind keyKind =
                returnKindForTypeName(normalizeBindingTypeName(keyValueKeyType));
            if (keyKind != ReturnKind::Unknown) {
              if (dispatchBootstrap.dispatchResolvers.resolveStringTarget(
                      keyExpr)) {
                return failPreDispatchDirectCallKeyValueKeyMismatch(
                    builtinAccessName, keyValueKeyType, receiverExpr);
              }
              ReturnKind indexKind =
                  inferExprReturnKind(keyExpr, params, locals);
              if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
                return failPreDispatchDirectCallKeyValueKeyMismatch(
                    builtinAccessName, keyValueKeyType, receiverExpr);
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
    auto resolvesNonRootExperimentalKeyValueTarget =
        [&](const Expr &candidate) {
          return !isRootMapConstructorExpr(candidate) &&
                 dispatchBootstrap.dispatchResolvers.resolveExperimentalMapValueTarget(
                     candidate, receiverTypeText, borrowedHelperProbe);
        };
    auto isNonRootExperimentalKeyValueReceiverExpr = [&](const Expr &candidate) {
      return !isRootMapConstructorExpr(candidate) &&
             isExperimentalKeyValueReceiverExpr(candidate);
    };
    if (getBuiltinArrayAccessName(expr, builtinAccessName) &&
        isUnqualifiedCollectionAccessCall(expr, builtinAccessName) &&
        expr.args.size() == 2 &&
        ((dispatchBootstrap.dispatchResolvers.resolveExperimentalMapValueTarget !=
          nullptr &&
          (resolvesNonRootExperimentalKeyValueTarget(expr.args.front()) ||
           resolvesNonRootExperimentalKeyValueTarget(expr.args[1]))) ||
         isNonRootExperimentalKeyValueReceiverExpr(expr.args.front()) ||
         isNonRootExperimentalKeyValueReceiverExpr(expr.args[1]))) {
      return failPreDispatchDirectCallDiagnostic(
          "unknown call target: " + canonicalKeyValueHelperPathLocal(builtinAccessName));
    }
  }

  return true;
}

} // namespace primec::semantics
