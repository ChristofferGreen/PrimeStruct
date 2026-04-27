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

} // namespace

size_t SemanticsValidator::mapHelperReceiverIndex(
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

bool SemanticsValidator::bareMapHelperOperandIndices(
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
  receiverIndexOut = mapHelperReceiverIndex(candidate, dispatchResolvers);
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

std::string SemanticsValidator::preferredBareMapHelperTarget(std::string_view helperName) const {
  auto hasVisibleMapHelperFamily = [&](const std::string &path) {
    return hasDeclaredDefinitionPath(path) ||
           hasImportedDefinitionPath(path) ||
           hasDefinitionFamilyPath(path);
  };
  const std::string canonical = canonicalCollectionHelperPath(
      StdlibSurfaceId::CollectionsMapHelpers, helperName);
  if (hasVisibleMapHelperFamily(canonical)) {
    return canonical;
  }
  if (isPublishedMapBaseHelperName(helperName) ||
      isPublishedBorrowedMapHelperName(helperName)) {
    return canonical;
  }
  const std::string alias = "/map/" + std::string(helperName);
  if (hasVisibleMapHelperFamily(alias)) {
    return alias;
  }
  return canonical;
}

std::string SemanticsValidator::specializedExperimentalMapHelperTarget(
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

  const std::string basePath = preferredCanonicalExperimentalMapHelperTarget(helperName);
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
  auto hasSamePathVectorHelperFamily = [&](const std::string &samePath) {
    return hasDeclaredDefinitionPath(samePath) ||
           hasImportedDefinitionPath(samePath) ||
           hasDefinitionFamilyPath(samePath);
  };
  if (helperName == "at" || helperName == "at_unsafe") {
    const std::string samePath = "/vector/" + std::string(helperName);
    if (hasSamePathVectorHelperFamily(samePath)) {
      return samePath;
    }
    return canonicalCollectionHelperPath(
        StdlibSurfaceId::CollectionsVectorHelpers, helperName);
  }
  const std::string samePath = "/vector/" + std::string(helperName);
  if (hasSamePathVectorHelperFamily(samePath)) {
    return samePath;
  }
  const std::string canonical = canonicalCollectionHelperPath(
      StdlibSurfaceId::CollectionsVectorHelpers, helperName);
  if (hasDeclaredDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
    return canonical;
  }
  return canonical;
}

bool SemanticsValidator::tryRewriteBareMapHelperCall(
    const Expr &candidate,
    std::string_view helperName,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    Expr &rewrittenOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty() ||
      dispatchResolvers.resolveMapTarget == nullptr) {
    return false;
  }
  if (candidate.name == "/std/collections/map/" + std::string(helperName) ||
      candidate.name == "/map/" + std::string(helperName)) {
    return false;
  }
  const size_t receiverIndex = mapHelperReceiverIndex(candidate, dispatchResolvers);
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
    rewrittenOut.name = specializedExperimentalMapHelperTarget(helperName, keyType, valueType);
    if (rewrittenOut.name.find("__t") != std::string::npos) {
      rewrittenOut.templateArgs.clear();
    } else if (rewrittenOut.templateArgs.empty()) {
      rewrittenOut.templateArgs = {keyType, valueType};
    }
  } else {
    rewrittenOut.name = preferredBareMapHelperTarget(helperName);
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
  if (resolvedCandidatePath == "/std/collections/soa_vector/to_aos" ||
      resolvedCandidatePath == "/std/collections/soa_vector/to_aos_ref" ||
      isSimpleCallName(candidate, "to_soa") ||
      isSimpleCallName(candidate, "to_aos") ||
      isSimpleCallName(candidate, "to_aos_ref")) {
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
    if (!resolvePublishedCollectionHelperMemberToken(
            normalizedMethod,
            StdlibSurfaceId::CollectionsVectorHelpers,
            helperName) ||
        helperName == "vector") {
      return false;
    }
    canonicalPath = canonicalCollectionHelperPath(
        StdlibSurfaceId::CollectionsVectorHelpers, helperName);
    if (canonicalPath.empty()) {
      helperName.clear();
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
    if (path.rfind("/std/collections/vector/", 0) == 0) {
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

bool SemanticsValidator::tryRewriteCanonicalExperimentalMapHelperCall(
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
  if (candidate.isMethodCall) {
    std::string normalizedMethod = candidate.name;
    if (!normalizedMethod.empty() && normalizedMethod.front() == '/') {
      normalizedMethod.erase(normalizedMethod.begin());
    }
    if (!resolvePublishedCollectionHelperMemberToken(
            normalizedMethod,
            StdlibSurfaceId::CollectionsMapHelpers,
            helperName) ||
        !isPublishedMapBaseHelperName(helperName)) {
      return false;
    }
    if (helperName == "insert") {
      std::string keyType;
      std::string valueType;
      const bool resolvesBorrowedExperimentalMap =
          dispatchResolvers.resolveExperimentalMapTarget(candidate.args.front(), keyType, valueType) &&
          !dispatchResolvers.resolveExperimentalMapValueTarget(candidate.args.front(), keyType, valueType);
      if (!resolvesBorrowedExperimentalMap) {
        return false;
      }
      rewrittenOut = candidate;
      rewrittenOut.isMethodCall = false;
      rewrittenOut.isFieldAccess = false;
      rewrittenOut.name = preferredPublishedCollectionLoweringPath(
          "insert_ref",
          StdlibSurfaceId::CollectionsMapHelpers,
          "/std/collections/experimental_map/");
      if (rewrittenOut.name.empty()) {
        rewrittenOut.name = "/std/collections/experimental_map/mapInsertRef";
      }
      rewrittenOut.namespacePrefix.clear();
      if (rewrittenOut.templateArgs.empty()) {
        rewrittenOut.templateArgs = {keyType, valueType};
      }
      return true;
    }
    canonicalPath = canonicalCollectionHelperPath(
        StdlibSurfaceId::CollectionsMapHelpers, helperName);
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
    const std::string resolvedOrExplicitPath = [&]() {
      const std::string resolvedPath = resolveCalleePath(candidate);
      if (!resolvedPath.empty()) {
        return resolvedPath;
      }
      return explicitCallPathForCandidate(candidate);
    }();
    if (!canonicalExperimentalMapHelperPath(resolvedOrExplicitPath, canonicalPath, helperName)) {
      return false;
    }
  }

  const size_t receiverIndex =
      candidate.isMethodCall ? 0 : mapHelperReceiverIndex(canonicalCandidate, dispatchResolvers);
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

  std::string keyType;
  std::string valueType;
  const bool resolvesExperimentalMapValue =
      dispatchResolvers.resolveExperimentalMapValueTarget(receiverExpr, keyType, valueType);
  const bool resolvesExperimentalMap =
      resolvesExperimentalMapValue ||
      dispatchResolvers.resolveExperimentalMapTarget(receiverExpr, keyType, valueType);
  const bool resolvesCanonicalMap =
      dispatchResolvers.resolveMapTarget != nullptr &&
      dispatchResolvers.resolveMapTarget(receiverExpr, keyType, valueType);
  if (!resolvesExperimentalMap &&
      !(candidate.isMethodCall && resolvesCanonicalMap)) {
    return false;
  }
  const bool isBorrowedCanonicalHelper =
      !resolvesExperimentalMapValue &&
      helperName.size() >= std::string_view("_ref").size() &&
      helperName.rfind("_ref") == helperName.size() - std::string_view("_ref").size();
  if (!candidate.isMethodCall && resolvesExperimentalMap &&
      !resolvesExperimentalMapValue && !isBorrowedCanonicalHelper) {
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
    rewrittenOut.name = specializedExperimentalMapHelperTarget(helperName, keyType, valueType);
    rewrittenOut.namespacePrefix.clear();
    if (rewrittenOut.name.find("__t") != std::string::npos) {
      rewrittenOut.templateArgs.clear();
    }
  }
  return true;
}

bool SemanticsValidator::explicitCanonicalExperimentalMapBorrowedHelperPath(
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
  if (!canonicalExperimentalMapHelperPath(resolvedOrExplicitPath, resolvedPathOut, helperName)) {
    return false;
  }
  if (helperName.size() >= std::string_view("_ref").size() &&
      helperName.rfind("_ref") == helperName.size() - std::string_view("_ref").size()) {
    resolvedPathOut.clear();
    return false;
  }
  const size_t receiverIndex = mapHelperReceiverIndex(candidate, dispatchResolvers);
  if (receiverIndex >= candidate.args.size()) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  return dispatchResolvers.resolveExperimentalMapTarget(candidate.args[receiverIndex], keyType, valueType) &&
         !dispatchResolvers.resolveExperimentalMapValueTarget(candidate.args[receiverIndex], keyType, valueType);
}

bool SemanticsValidator::hasResolvableMapHelperPath(const std::string &path) const {
  return hasDeclaredDefinitionPath(path) || hasImportedDefinitionPath(path);
}

bool SemanticsValidator::resolveMapKeyType(
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

bool SemanticsValidator::resolveMapValueType(
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
