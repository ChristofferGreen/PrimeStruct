#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace primec::semantics {

namespace {

std::string explicitCallPathForCandidate(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return "";
  }
  if (!candidate.name.empty() && candidate.name.front() == '/') {
    return candidate.name;
  }
  std::string namespacePrefix = candidate.namespacePrefix;
  if (!namespacePrefix.empty() && namespacePrefix.front() != '/') {
    namespacePrefix.insert(namespacePrefix.begin(), '/');
  }
  if (namespacePrefix.empty()) {
    return "/" + candidate.name;
  }
  return namespacePrefix + "/" + candidate.name;
}

bool isBareKeyValueAccessHelperName(std::string_view helperName) {
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

const StdlibSurfaceMetadata *collectionRewriteKeyValueHelperMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

std::string canonicalKeyValueHelperPathForRewrite(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata =
      collectionRewriteKeyValueHelperMetadata();
  if (metadata == nullptr) {
    return "";
  }
  return canonicalCollectionHelperPath(metadata->id, helperName);
}

bool resolveKeyValueHelperMemberTokenForRewrite(
    std::string_view memberToken,
    std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      collectionRewriteKeyValueHelperMetadata();
  return metadata != nullptr &&
         resolvePublishedCollectionHelperMemberToken(memberToken, metadata->id,
                                                     helperNameOut);
}

bool resolveExplicitKeyValueHelperPathForRewrite(
    std::string_view path,
    std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      collectionRewriteKeyValueHelperMetadata();
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view helperName =
      resolveStdlibSurfaceMemberName(*metadata, path);
  if (helperName.empty()) {
    return false;
  }
  helperNameOut.assign(helperName);
  return true;
}

std::string preferredKeyValueHelperLoweringPathForRewrite(
    std::string_view helperName,
    std::string_view preferredPrefix) {
  const StdlibSurfaceMetadata *metadata =
      collectionRewriteKeyValueHelperMetadata();
  if (metadata == nullptr) {
    return "";
  }
  return preferredPublishedCollectionLoweringPath(helperName, metadata->id,
                                                 preferredPrefix);
}

} // namespace

size_t SemanticsValidator::keyValueHelperReceiverIndex(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  if (hasNamedArguments(candidate.argNames)) {
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        return i;
      }
    }
  }
  if (dispatchResolvers.resolveMapTarget == nullptr || candidate.args.empty()) {
    return 0;
  }
  std::string keyType;
  std::string valueType;
  if (!dispatchResolvers.resolveMapTarget(candidate.args.front(), keyType, valueType)) {
    for (size_t i = 1; i < candidate.args.size(); ++i) {
      if (dispatchResolvers.resolveMapTarget(candidate.args[i], keyType, valueType)) {
        return i;
      }
    }
  }
  return 0;
}

bool SemanticsValidator::bareKeyValueHelperOperandIndices(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    size_t &receiverIndexOut,
    size_t &keyIndexOut) const {
  receiverIndexOut = 0;
  keyIndexOut = 0;
  if (candidate.kind != Expr::Kind::Call || candidate.args.size() != 2 ||
      dispatchResolvers.resolveMapTarget == nullptr) {
    return false;
  }
  receiverIndexOut = keyValueHelperReceiverIndex(candidate, dispatchResolvers);
  if (receiverIndexOut >= candidate.args.size()) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  if (!dispatchResolvers.resolveMapTarget(candidate.args[receiverIndexOut], keyType, valueType)) {
    return false;
  }
  keyIndexOut = receiverIndexOut == 0 ? 1 : 0;
  return keyIndexOut < candidate.args.size();
}

std::string SemanticsValidator::preferredBareKeyValueHelperTarget(
    std::string_view helperName) const {
  auto hasVisibleKeyValueHelperFamily = [&](const std::string &path) {
    return hasDeclaredDefinitionPath(path) ||
           hasImportedDefinitionPath(path) ||
           hasDefinitionFamilyPath(path);
  };
  const std::string canonical = canonicalKeyValueHelperPathForRewrite(helperName);
  if (hasVisibleKeyValueHelperFamily(canonical)) {
    return canonical;
  }
  if (isPublishedKeyValueBaseHelperName(helperName) ||
      isPublishedBorrowedKeyValueHelperName(helperName)) {
    return canonical;
  }
  return canonical;
}

std::string SemanticsValidator::specializedExperimentalKeyValueHelperTarget(
    std::string_view helperName,
    const std::string &keyType,
    const std::string &valueType) const {
  auto fnv1a64 = [](const std::string &text) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : text) {
      hash ^= static_cast<uint64_t>(ch);
      hash *= 1099511628211ULL;
    }
    return hash;
  };
  auto stripWhitespace = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
      if (!std::isspace(ch)) {
        out.push_back(static_cast<char>(ch));
      }
    }
    return out;
  };
  const std::string currentNamespacePrefix = [&]() -> std::string {
    if (currentValidationState_.context.definitionPath.empty()) {
      return {};
    }
    const size_t slash = currentValidationState_.context.definitionPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return {};
    }
    return currentValidationState_.context.definitionPath.substr(0, slash);
  }();
  std::function<std::string(const std::string &)> canonicalizeTypeText =
      [&](const std::string &typeText) -> std::string {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return normalizedType;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args)) {
        return normalizedType;
      }
      for (std::string &arg : args) {
        arg = canonicalizeTypeText(arg);
      }
      std::string canonicalBase = normalizeBindingTypeName(base);
      if (!canonicalBase.empty() && canonicalBase.front() != '/' &&
          std::isupper(static_cast<unsigned char>(canonicalBase.front()))) {
        std::string resolved = resolveStructTypePath(canonicalBase, currentNamespacePrefix, structNames_);
        if (resolved.empty()) {
          resolved = resolveTypePath(canonicalBase, currentNamespacePrefix);
        }
        if (!resolved.empty()) {
          canonicalBase = resolved;
        }
      }
      return canonicalBase + "<" + joinTemplateArgs(args) + ">";
    }
    if (!normalizedType.empty() && normalizedType.front() != '/' &&
        std::isupper(static_cast<unsigned char>(normalizedType.front()))) {
      std::string resolved = resolveStructTypePath(normalizedType, currentNamespacePrefix, structNames_);
      if (resolved.empty()) {
        resolved = resolveTypePath(normalizedType, currentNamespacePrefix);
      }
      if (!resolved.empty()) {
        return resolved;
      }
    }
    return normalizedType;
  };

  const std::string basePath =
      preferredCanonicalExperimentalKeyValueHelperTarget(helperName);
  std::ostringstream specializedPath;
  specializedPath << basePath
                  << "__t"
                  << std::hex
                  << fnv1a64(stripWhitespace(joinTemplateArgs(
                      {canonicalizeTypeText(keyType), canonicalizeTypeText(valueType)})));
  if (defMap_.count(specializedPath.str()) > 0) {
    return specializedPath.str();
  }
  const std::string canonicalKeyType = canonicalizeTypeText(keyType);
  const std::string canonicalValueType = canonicalizeTypeText(valueType);
  const std::string specializationPrefix = basePath + "__t";
  for (const auto &[path, params] : paramsByDef_) {
    if (path.rfind(specializationPrefix, 0) != 0 || params.empty()) {
      continue;
    }
    std::string candidateKeyType;
    std::string candidateValueType;
    if (!extractMapKeyValueTypes(params.front().binding, candidateKeyType, candidateValueType)) {
      continue;
    }
    if (canonicalizeTypeText(candidateKeyType) == canonicalKeyType &&
        canonicalizeTypeText(candidateValueType) == canonicalValueType) {
      return path;
    }
  }
  return basePath;
}

std::string SemanticsValidator::preferredBareVectorHelperTarget(std::string_view helperName) const {
  auto hasVisibleVectorHelperFamily = [&](const std::string &path) {
    return hasDeclaredDefinitionPath(path) ||
           hasImportedDefinitionPath(path) ||
           hasDefinitionFamilyPath(path);
  };
  const std::string canonical =
      canonicalVectorCompatibilityHelperPathOrFallback(helperName);
  if (isRemovedPublishedVectorStatementHelperName(helperName)) {
    const std::string samePath = rootedVectorHelperPath(helperName);
    if (hasVisibleVectorHelperFamily(samePath)) {
      return samePath;
    }
    return canonical;
  }
  if (helperName == "at" || helperName == "at_unsafe") {
    const std::string samePath = rootedVectorHelperPath(helperName);
    if (hasVisibleVectorHelperFamily(samePath)) {
      return samePath;
    }
    return canonical;
  }
  const std::string samePath = rootedVectorHelperPath(helperName);
  if (hasVisibleVectorHelperFamily(samePath)) {
    return samePath;
  }
  if (hasVisibleVectorHelperFamily(canonical)) {
    return canonical;
  }
  return canonical;
}

bool SemanticsValidator::tryRewriteBareKeyValueHelperCall(
    const Expr &candidate,
    std::string_view helperName,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    Expr &rewrittenOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty() ||
      dispatchResolvers.resolveMapTarget == nullptr) {
    return false;
  }
  std::string explicitHelperName;
  if (resolveExplicitKeyValueHelperPathForRewrite(
          explicitCallPathForCandidate(candidate), explicitHelperName) &&
      explicitHelperName == helperName) {
    return false;
  }
  const size_t receiverIndex = keyValueHelperReceiverIndex(candidate, dispatchResolvers);
  if (receiverIndex >= candidate.args.size()) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  if (!dispatchResolvers.resolveMapTarget(candidate.args[receiverIndex], keyType, valueType)) {
    return false;
  }
  rewrittenOut = candidate;
  if (dispatchResolvers.resolveExperimentalMapTarget != nullptr &&
      dispatchResolvers.resolveExperimentalMapTarget(candidate.args[receiverIndex], keyType, valueType)) {
    if (isBareKeyValueAccessHelperName(helperName)) {
      return false;
    }
    rewrittenOut.name =
        specializedExperimentalKeyValueHelperTarget(helperName, keyType, valueType);
    if (rewrittenOut.name.find("__t") != std::string::npos) {
      rewrittenOut.templateArgs.clear();
    } else if (rewrittenOut.templateArgs.empty()) {
      rewrittenOut.templateArgs = {keyType, valueType};
    }
  } else {
    rewrittenOut.name = preferredBareKeyValueHelperTarget(helperName);
  }
  rewrittenOut.namespacePrefix.clear();
  return true;
}

bool SemanticsValidator::tryRewriteBareVectorHelperCall(
    const Expr &candidate,
    std::string_view helperName,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    Expr &rewrittenOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty() ||
      dispatchResolvers.resolveVectorTarget == nullptr) {
    return false;
  }
  const bool isBareHelperSpelling =
      candidate.name == helperName &&
      candidate.name.find('/') == std::string::npos &&
      candidate.namespacePrefix.empty();
  if (!isBareHelperSpelling) {
    return false;
  }
  const std::string directHelperPath = "/" + std::string(helperName);
  if (hasDeclaredDefinitionPath(directHelperPath) || hasImportedDefinitionPath(directHelperPath)) {
    return false;
  }
  size_t receiverIndex = 0;
  if (hasNamedArguments(candidate.argNames)) {
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        receiverIndex = i;
        break;
      }
    }
  } else {
    auto resolvesVectorTarget = [&](const Expr &receiverCandidate) {
      std::string elemType;
      return (dispatchResolvers.resolveVectorTarget != nullptr &&
              dispatchResolvers.resolveVectorTarget(receiverCandidate, elemType)) ||
             (dispatchResolvers.resolveExperimentalVectorValueTarget != nullptr &&
              dispatchResolvers.resolveExperimentalVectorValueTarget(receiverCandidate,
                                                                    elemType)) ||
             (dispatchResolvers.resolveExperimentalVectorTarget != nullptr &&
              dispatchResolvers.resolveExperimentalVectorTarget(receiverCandidate,
                                                               elemType));
    };
    if (!resolvesVectorTarget(candidate.args.front())) {
      for (size_t i = 1; i < candidate.args.size(); ++i) {
        if (resolvesVectorTarget(candidate.args[i])) {
          receiverIndex = i;
          break;
        }
      }
    }
  }
  if (receiverIndex >= candidate.args.size()) {
    return false;
  }
  std::string builtinElemType;
  const bool resolvesBuiltinVector =
      dispatchResolvers.resolveVectorTarget(candidate.args[receiverIndex], builtinElemType);
  std::string experimentalElemType;
  const bool resolvesExperimentalVector =
      dispatchResolvers.resolveExperimentalVectorValueTarget != nullptr &&
      dispatchResolvers.resolveExperimentalVectorValueTarget(candidate.args[receiverIndex], experimentalElemType);
  if (!resolvesBuiltinVector && !resolvesExperimentalVector) {
    return false;
  }
  rewrittenOut = candidate;
  if (resolvesExperimentalVector) {
    const std::string preferredHelperPath =
        preferredBareVectorHelperTarget(helperName);
    if (hasImportedDefinitionPath(preferredHelperPath) ||
        hasDeclaredDefinitionPath(preferredHelperPath)) {
      rewrittenOut.name = preferredHelperPath;
      rewrittenOut.namespacePrefix.clear();
      return true;
    }
    const std::string experimentalHelperPath =
        specializedExperimentalVectorHelperTarget(helperName, experimentalElemType);
    const std::string experimentalBasePath =
        preferredCanonicalExperimentalVectorHelperTarget(helperName);
    const bool hasVisibleExperimentalVectorHelperBase =
        hasImportedDefinitionPath(experimentalBasePath) ||
        hasDeclaredDefinitionPath(experimentalBasePath);
    const bool hasVisibleExperimentalVectorHelperPath =
        hasVisibleExperimentalVectorHelperBase ||
        hasImportedDefinitionPath(experimentalHelperPath) ||
        hasDeclaredDefinitionPath(experimentalHelperPath) ||
        (hasVisibleExperimentalVectorHelperBase &&
         experimentalHelperPath.find("__t") != std::string::npos &&
         defMap_.count(experimentalHelperPath) > 0);
    if (!hasVisibleExperimentalVectorHelperPath) {
      rewrittenOut.name = preferredBareVectorHelperTarget(helperName);
    } else if (receiverIndex == 0 && !hasNamedArguments(candidate.argNames)) {
      rewrittenOut.isMethodCall = true;
      rewrittenOut.name = std::string(helperName);
      rewrittenOut.namespacePrefix.clear();
    } else {
      rewrittenOut.name = experimentalHelperPath;
      if (rewrittenOut.name.find("__t") != std::string::npos) {
        rewrittenOut.templateArgs.clear();
      } else if (rewrittenOut.templateArgs.empty()) {
        rewrittenOut.templateArgs = {experimentalElemType};
      }
    }
  } else {
    rewrittenOut.name = preferredBareVectorHelperTarget(helperName);
  }
  rewrittenOut.namespacePrefix.clear();
  return true;
}

bool SemanticsValidator::tryRewriteCanonicalExperimentalVectorHelperCall(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    Expr &rewrittenOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.args.empty() ||
      dispatchResolvers.resolveExperimentalVectorTarget == nullptr ||
      dispatchResolvers.resolveExperimentalVectorValueTarget == nullptr) {
    return false;
  }
  const std::string resolvedCandidatePath =
      canonicalizeLegacySoaToAosHelperPath(resolveCalleePath(candidate));
  if (isLegacyOrCanonicalSoaHelperPath(resolvedCandidatePath, "to" "_aos") ||
      isLegacyOrCanonicalSoaHelperPath(resolvedCandidatePath, "to" "_aos_ref") ||
      isSimpleCallName(candidate, "to_soa") ||
      isSimpleCallName(candidate, "to" "_aos") ||
      isSimpleCallName(candidate, "to" "_aos_ref")) {
    return false;
  }

  std::string canonicalPath;
  std::string helperName;
  Expr canonicalCandidate = candidate;
  if (candidate.isMethodCall) {
    std::string normalizedMethod = candidate.name;
    if (!normalizedMethod.empty() && normalizedMethod.front() == '/') {
      normalizedMethod.erase(normalizedMethod.begin());
    }
    if (!canonicalExperimentalVectorHelperPath(normalizedMethod,
                                               canonicalPath,
                                               helperName)) {
      return false;
    }
    const size_t lastSlash = canonicalPath.find_last_of('/');
    canonicalCandidate.isMethodCall = false;
    canonicalCandidate.name = helperName;
    canonicalCandidate.namespacePrefix =
        lastSlash == std::string::npos ? std::string() : canonicalPath.substr(0, lastSlash);
  } else if (!canonicalExperimentalVectorHelperPath(resolveCalleePath(candidate),
                                                    canonicalPath,
                                                    helperName)) {
    return false;
  }

  size_t receiverIndex = 0;
  if (!candidate.isMethodCall && hasNamedArguments(canonicalCandidate.argNames)) {
    for (size_t i = 0; i < canonicalCandidate.args.size(); ++i) {
      if (i < canonicalCandidate.argNames.size() &&
          canonicalCandidate.argNames[i].has_value() &&
          *canonicalCandidate.argNames[i] == "values") {
        receiverIndex = i;
        break;
      }
    }
  } else if (!candidate.isMethodCall) {
    auto resolvesVectorTarget = [&](const Expr &receiverCandidate) {
      std::string elemType;
      return (dispatchResolvers.resolveVectorTarget != nullptr &&
              dispatchResolvers.resolveVectorTarget(receiverCandidate, elemType)) ||
             (dispatchResolvers.resolveExperimentalVectorValueTarget != nullptr &&
              dispatchResolvers.resolveExperimentalVectorValueTarget(receiverCandidate,
                                                                    elemType)) ||
             (dispatchResolvers.resolveExperimentalVectorTarget != nullptr &&
              dispatchResolvers.resolveExperimentalVectorTarget(receiverCandidate,
                                                               elemType));
    };
    if (!resolvesVectorTarget(canonicalCandidate.args.front())) {
      for (size_t i = 1; i < canonicalCandidate.args.size(); ++i) {
        if (resolvesVectorTarget(canonicalCandidate.args[i])) {
          receiverIndex = i;
          break;
        }
      }
    }
  }
  if (receiverIndex >= canonicalCandidate.args.size()) {
    return false;
  }
  const Expr &receiverExpr = canonicalCandidate.args[receiverIndex];
  if (!candidate.isMethodCall &&
      !candidate.templateArgs.empty() &&
      receiverExpr.kind == Expr::Kind::Call &&
      !receiverExpr.isBinding &&
      !receiverExpr.isMethodCall) {
    return false;
  }
  auto hasVisibleCanonicalVectorHelperPath = [&](const std::string &path) {
    if (hasImportedDefinitionPath(path)) {
      return true;
    }
    if (defMap_.count(path) == 0) {
      return false;
    }
    if (isStdNamespacedVectorCompatibilityHelperPath(path, helperName)) {
      auto paramsIt = paramsByDef_.find(path);
      if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty()) {
        std::string experimentalElemType;
        if (extractExperimentalVectorElementType(paramsIt->second.front().binding, experimentalElemType)) {
          return false;
        }
      }
    }
    return true;
  };

  std::string elemType;
  if (!dispatchResolvers.resolveExperimentalVectorValueTarget(receiverExpr, elemType)) {
    return false;
  }
  if (!hasVisibleCanonicalVectorHelperPath(canonicalPath)) {
    return false;
  }
  if (candidate.isMethodCall) {
    rewrittenOut = canonicalCandidate;
    return true;
  }
  const bool alreadyCanonicalDirectCall =
      (resolvedCandidatePath == canonicalPath ||
       resolvedCandidatePath.rfind(canonicalPath + "__t", 0) == 0) &&
      !candidate.isMethodCall;
  if (alreadyCanonicalDirectCall) {
    return false;
  }
  rewrittenOut = canonicalCandidate;
  return true;
}

bool SemanticsValidator::tryRewriteCanonicalExperimentalKeyValueHelperCall(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    Expr &rewrittenOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.args.empty() ||
      dispatchResolvers.resolveExperimentalMapTarget == nullptr ||
      dispatchResolvers.resolveExperimentalMapValueTarget == nullptr) {
    return false;
  }

  std::string canonicalPath;
  std::string helperName;
  Expr canonicalCandidate = candidate;
  bool directExperimentalKeyValueHelperSpelling = false;
  if (candidate.isMethodCall) {
    std::string normalizedMethod = candidate.name;
    if (!normalizedMethod.empty() && normalizedMethod.front() == '/') {
      normalizedMethod.erase(normalizedMethod.begin());
    }
    if (!resolveKeyValueHelperMemberTokenForRewrite(normalizedMethod,
                                               helperName) ||
        !isPublishedKeyValueBaseHelperName(helperName)) {
      return false;
    }
    if (isBareKeyValueAccessHelperName(helperName) &&
        candidate.args.front().kind == Expr::Kind::Call) {
      return false;
    }
    if (helperName == "insert") {
      std::string keyType;
      std::string valueType;
      const bool resolvesBorrowedExperimentalKeyValue =
          dispatchResolvers.resolveExperimentalMapTarget(candidate.args.front(), keyType,
                                                         valueType) &&
          !dispatchResolvers.resolveExperimentalMapValueTarget(
              candidate.args.front(), keyType, valueType);
      if (!resolvesBorrowedExperimentalKeyValue) {
        return false;
      }
      rewrittenOut = candidate;
      rewrittenOut.isMethodCall = false;
      rewrittenOut.isFieldAccess = false;
      rewrittenOut.name = preferredKeyValueHelperLoweringPathForRewrite(
          "insert_ref", experimentalCollectionConstructorRootLocal("map"));
      if (rewrittenOut.name.empty()) {
        rewrittenOut.name = canonicalKeyValueHelperPathForRewrite("insert_ref");
      }
      if (rewrittenOut.name.empty()) {
        return false;
      }
      rewrittenOut.namespacePrefix.clear();
      if (rewrittenOut.templateArgs.empty()) {
        rewrittenOut.templateArgs = {keyType, valueType};
      }
      return true;
    }
    canonicalPath = canonicalKeyValueHelperPathForRewrite(helperName);
    if (canonicalPath.empty()) {
      helperName.clear();
      return false;
    }
    const size_t lastSlash = canonicalPath.find_last_of('/');
    canonicalCandidate.isMethodCall = false;
    canonicalCandidate.name = helperName;
    canonicalCandidate.namespacePrefix =
        lastSlash == std::string::npos ? std::string() : canonicalPath.substr(0, lastSlash);
  } else {
    const std::string explicitTarget = explicitCallPathForCandidate(candidate);
    const std::string resolvedOrExplicitPath = [&]() {
      const std::string resolvedPath = resolveCalleePath(candidate);
      if (!resolvedPath.empty()) {
        return resolvedPath;
      }
      return explicitTarget;
    }();
    directExperimentalKeyValueHelperSpelling =
        explicitTarget.rfind(experimentalCollectionConstructorRootLocal("map"), 0) == 0;
    if (!canonicalExperimentalKeyValueHelperPath(
            resolvedOrExplicitPath, canonicalPath, helperName)) {
      return false;
    }
    const bool alreadyCanonicalDirectCall =
        (resolvedOrExplicitPath == canonicalPath ||
         resolvedOrExplicitPath.rfind(canonicalPath + "__t", 0) == 0) &&
        !directExperimentalKeyValueHelperSpelling;
    if (alreadyCanonicalDirectCall) {
      return false;
    }
  }
  const size_t receiverIndex =
      candidate.isMethodCall ? 0 : keyValueHelperReceiverIndex(canonicalCandidate, dispatchResolvers);
  if (receiverIndex >= canonicalCandidate.args.size()) {
    return false;
  }
  const Expr &receiverExpr = canonicalCandidate.args[receiverIndex];
  auto isPublishedKeyValueConstructorReceiver = [](const Expr &candidateExpr) {
    if (candidateExpr.kind != Expr::Kind::Call || candidateExpr.isMethodCall ||
        candidateExpr.name.empty()) {
      return false;
    }
    std::string normalizedName = candidateExpr.name;
    if (!candidateExpr.namespacePrefix.empty() &&
        normalizedName.find('/') == std::string::npos) {
      std::string normalizedPrefix = candidateExpr.namespacePrefix;
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
    return isResolvedKeyValueConstructorPath(normalizedName);
  };
  if (!candidate.isMethodCall && !directExperimentalKeyValueHelperSpelling &&
      isBareKeyValueAccessHelperName(helperName) &&
      !isPublishedKeyValueConstructorReceiver(receiverExpr)) {
    return false;
  }
  if (!candidate.isMethodCall &&
      !candidate.templateArgs.empty() &&
      receiverExpr.kind == Expr::Kind::Call &&
      !receiverExpr.isBinding &&
      !receiverExpr.isMethodCall) {
    return false;
  }

  std::string keyType;
  std::string valueType;
  const bool resolvesExperimentalKeyValueValue =
      dispatchResolvers.resolveExperimentalMapValueTarget(receiverExpr, keyType, valueType);
  const bool resolvesExperimentalKeyValue =
      resolvesExperimentalKeyValueValue ||
      dispatchResolvers.resolveExperimentalMapTarget(receiverExpr, keyType, valueType);
  const bool resolvesCanonicalKeyValue =
      dispatchResolvers.resolveMapTarget != nullptr &&
      dispatchResolvers.resolveMapTarget(receiverExpr, keyType, valueType);
  if (!resolvesExperimentalKeyValue &&
      !(candidate.isMethodCall && resolvesCanonicalKeyValue)) {
    return false;
  }
  const bool isBorrowedCanonicalHelper =
      !resolvesExperimentalKeyValueValue &&
      helperName.size() >= std::string_view("_ref").size() &&
      helperName.rfind("_ref") == helperName.size() - std::string_view("_ref").size();
  if (!candidate.isMethodCall && resolvesExperimentalKeyValue &&
      !resolvesExperimentalKeyValueValue && !isBorrowedCanonicalHelper) {
    return false;
  }
  rewrittenOut = canonicalCandidate;
  if (!candidate.isMethodCall && isBorrowedCanonicalHelper) {
    return false;
  }
  if (candidate.isMethodCall) {
    rewrittenOut.templateArgs.clear();
  } else if (rewrittenOut.templateArgs.empty()) {
    rewrittenOut.templateArgs = {keyType, valueType};
  }
  if (!candidate.isMethodCall) {
    rewrittenOut.name =
        specializedExperimentalKeyValueHelperTarget(helperName, keyType, valueType);
    rewrittenOut.namespacePrefix.clear();
    if (rewrittenOut.name.find("__t") != std::string::npos) {
      rewrittenOut.templateArgs.clear();
    }
  }
  return true;
}

bool SemanticsValidator::explicitCanonicalExperimentalKeyValueBorrowedHelperPath(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    std::string &resolvedPathOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty() ||
      dispatchResolvers.resolveExperimentalMapTarget == nullptr ||
      dispatchResolvers.resolveExperimentalMapValueTarget == nullptr) {
    return false;
  }
  std::string helperName;
  const std::string resolvedOrExplicitPath = [&]() {
    const std::string resolvedPath = resolveCalleePath(candidate);
    if (!resolvedPath.empty()) {
      return resolvedPath;
    }
    return explicitCallPathForCandidate(candidate);
  }();
  if (!canonicalExperimentalKeyValueHelperPath(
          resolvedOrExplicitPath, resolvedPathOut, helperName)) {
    return false;
  }
  if (helperName.size() >= std::string_view("_ref").size() &&
      helperName.rfind("_ref") == helperName.size() - std::string_view("_ref").size()) {
    resolvedPathOut.clear();
    return false;
  }
  const size_t receiverIndex = keyValueHelperReceiverIndex(candidate, dispatchResolvers);
  if (receiverIndex >= candidate.args.size()) {
    return false;
  }
  auto isRootKeyValueConstructorCandidate = [](const Expr &receiverExpr) {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isMethodCall ||
        receiverExpr.name.empty()) {
      return false;
    }
    std::string normalizedName = receiverExpr.name;
    if (!receiverExpr.namespacePrefix.empty() &&
        normalizedName.find('/') == std::string::npos) {
      std::string normalizedPrefix = receiverExpr.namespacePrefix;
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
  };
  if (isRootKeyValueConstructorCandidate(candidate.args[receiverIndex])) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  return dispatchResolvers.resolveExperimentalMapTarget(candidate.args[receiverIndex], keyType, valueType) &&
         !dispatchResolvers.resolveExperimentalMapValueTarget(candidate.args[receiverIndex], keyType, valueType);
}

bool SemanticsValidator::hasResolvableKeyValueHelperPath(const std::string &path) const {
  return hasDeclaredDefinitionPath(path) || hasImportedDefinitionPath(path);
}

bool SemanticsValidator::resolveKeyValueKeyType(
    const Expr &target,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    std::string &keyTypeOut) const {
  keyTypeOut.clear();
  if (dispatchResolvers.resolveMapTarget == nullptr) {
    return false;
  }
  std::string valueType;
  return dispatchResolvers.resolveMapTarget(target, keyTypeOut, valueType);
}

bool SemanticsValidator::resolveKeyValueValueType(
    const Expr &target,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    std::string &valueTypeOut) const {
  valueTypeOut.clear();
  if (dispatchResolvers.resolveMapTarget == nullptr) {
    return false;
  }
  std::string keyType;
  return dispatchResolvers.resolveMapTarget(target, keyType, valueTypeOut);
}

} // namespace primec::semantics
