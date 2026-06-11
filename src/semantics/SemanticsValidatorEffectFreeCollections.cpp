// soa-surface-audit: exempt
#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <string_view>
#include <vector>

namespace primec::semantics {
namespace {

const StdlibSurfaceMetadata *
keyValueHelperSurfaceMetadataForEffectFreeCollections() {
  return keyValueHelperSurfaceMetadataLocal();
}

std::string canonicalKeyValueHelperPathLocal(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataForEffectFreeCollections();
  if (metadata == nullptr) {
    return "";
  }
  return canonicalCollectionHelperPath(metadata->id, helperName);
}

std::string unrootedCanonicalKeyValueHelperPrefixLocal() {
  const StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataForEffectFreeCollections();
  if (metadata == nullptr) {
    return "";
  }
  std::string prefix(metadata->canonicalPath);
  if (!prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix + "/";
}

} // namespace

std::string SemanticsValidator::normalizeEffectFreeCollectionMethodName(
    const std::string &receiverPath,
    std::string methodName) const {
  if (!methodName.empty() && methodName.front() == '/') {
    methodName.erase(methodName.begin());
  }
  if (receiverPath == "/vector" || receiverPath == "/array") {
    const std::string arrayPrefix = "array/";
    const std::string stdVectorPrefix =
        unrootedCanonicalVectorCompatibilityPrefixOrFallback() + "/";
    if (isUnrootedVectorHelperPath(methodName)) {
      return methodName.substr(unrootedVectorHelperPrefix().size());
    }
    if (methodName.rfind(arrayPrefix, 0) == 0) {
      return methodName.substr(arrayPrefix.size());
    }
    if (methodName.rfind(stdVectorPrefix, 0) == 0) {
      return methodName.substr(stdVectorPrefix.size());
    }
  }
  if (receiverPath == "/map") {
    const std::string stdKeyValueHelperPrefix =
        unrootedCanonicalKeyValueHelperPrefixLocal();
    if (!stdKeyValueHelperPrefix.empty() &&
        methodName.rfind(stdKeyValueHelperPrefix, 0) == 0) {
      return methodName.substr(stdKeyValueHelperPrefix.size());
    }
  }
  return methodName;
}

std::vector<std::string> SemanticsValidator::effectFreeMethodPathCandidatesForReceiver(
    const std::string &receiverPath,
    const std::string &methodName) const {
  if (receiverPath == "/vector") {
    if (methodName == "count") {
      return {canonicalVectorCompatibilityHelperPathOrFallback(methodName)};
    }
    return {canonicalVectorCompatibilityHelperPathOrFallback(methodName),
            "/array/" + methodName};
  }
  if (receiverPath == "/array") {
    if (methodName == "count") {
      return {"/array/" + methodName};
    }
    return {"/array/" + methodName,
            canonicalVectorCompatibilityHelperPathOrFallback(methodName)};
  }
  if (receiverPath == "/map") {
    return {canonicalKeyValueHelperPathLocal(methodName)};
  }
  return {receiverPath + "/" + methodName};
}

std::string SemanticsValidator::preferEffectFreeCollectionHelperPath(const std::string &path) const {
  auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
    return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
           suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
           suffix != "remove_at" && suffix != "remove_swap";
  };
  std::string preferred = path;
  if (preferred.rfind("/array/", 0) == 0 && defMap_.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      const std::string stdlibAlias =
          canonicalVectorCompatibilityHelperPathOrFallback(suffix);
      if (defMap_.count(stdlibAlias) > 0) {
        return stdlibAlias;
      }
    }
  }
  return preferred;
}

std::vector<std::string> SemanticsValidator::effectFreeCollectionHelperPathCandidates(const std::string &path) const {
  auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
    return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
           suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
           suffix != "remove_at" && suffix != "remove_swap";
  };
  std::vector<std::string> candidates;
  auto appendUnique = [&](const std::string &candidate) {
    if (candidate.empty()) {
      return;
    }
    for (const auto &existing : candidates) {
      if (existing == candidate) {
        return;
      }
    }
    candidates.push_back(candidate);
  };

  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    const std::string stdKeyValueHelperPrefix =
        unrootedCanonicalKeyValueHelperPrefixLocal();
    if (normalizedPath.rfind("array/", 0) == 0 ||
        isUnrootedVectorHelperPath(normalizedPath) ||
        isUnrootedCanonicalVectorCompatibilityPath(normalizedPath) ||
        (!stdKeyValueHelperPrefix.empty() &&
         normalizedPath.rfind(stdKeyValueHelperPrefix, 0) == 0)) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
  }

  appendUnique(path);
  appendUnique(normalizedPath);
  if (normalizedPath.rfind("/array/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique(canonicalVectorCompatibilityHelperPathOrFallback(suffix));
    }
  }
  return candidates;
}

std::string SemanticsValidator::effectFreeCollectionPathFromType(const std::string &typeName,
                                                                 const std::string &typeTemplateArg) const {
  if (typeName == "string") {
    return "/string";
  }
  if ((typeName == "array" || typeName == "vector" || typeName == "soa_vector") && !typeTemplateArg.empty()) {
    return "/" + typeName;
  }
  if (isKeyValueCollectionTypeName(typeName) && !typeTemplateArg.empty()) {
    return "/map";
  }
  std::string base;
  std::string argsText;
  if (!splitTemplateTypeName(typeName, base, argsText)) {
    return "";
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argsText, args)) {
    return "";
  }
  if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
    return "/" + base;
  }
  if (isKeyValueCollectionTypeName(base) && args.size() == 2) {
    return "/map";
  }
  if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
    return effectFreeCollectionPathFromType(normalizeBindingTypeName(args.front()), "");
  }
  return "";
}

std::string SemanticsValidator::effectFreeCollectionPathFromBinding(const BindingInfo &binding) const {
  if ((binding.typeName == "Reference" || binding.typeName == "Pointer") && !binding.typeTemplateArg.empty()) {
    return effectFreeCollectionPathFromType(normalizeBindingTypeName(binding.typeTemplateArg), "");
  }
  return effectFreeCollectionPathFromType(normalizeBindingTypeName(binding.typeName), binding.typeTemplateArg);
}

std::string SemanticsValidator::effectFreeCollectionPathFromCallExpr(const Expr &callExpr) const {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return "";
  }
  std::string builtinCollection;
  if (getBuiltinCollectionName(callExpr, builtinCollection)) {
    if (builtinCollection == "string") {
      return "/string";
    }
    if ((builtinCollection == "array" || builtinCollection == "vector" || builtinCollection == "soa_vector") &&
        callExpr.templateArgs.size() == 1) {
      return "/" + builtinCollection;
    }
    if (builtinCollection == "map" && callExpr.templateArgs.size() == 2) {
      return "/map";
    }
  }

  auto resolvedCandidates = effectFreeCollectionHelperPathCandidates(resolveCalleePath(callExpr));
  if (resolvedCandidates.empty()) {
    resolvedCandidates.push_back(resolveCalleePath(callExpr));
  }
  for (const auto &candidate : resolvedCandidates) {
    auto defIt = defMap_.find(candidate);
    if (defIt == defMap_.end() || !defIt->second) {
      continue;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string collectionPath =
          effectFreeCollectionPathFromType(normalizeBindingTypeName(transform.templateArgs.front()), "");
      if (!collectionPath.empty()) {
        return collectionPath;
      }
    }
  }
  return "";
}

std::string SemanticsValidator::resolveEffectFreeBareMapCallPath(const Expr &callExpr,
                                                                 const EffectFreeContext &ctx) const {
  if (callExpr.isMethodCall || callExpr.args.empty()) {
    return "";
  }
  std::string helperName;
  std::string normalizedName = callExpr.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  if (normalizedName == "count") {
    helperName = "count";
  } else if (normalizedName == "at") {
    helperName = "at";
  } else if (normalizedName == "at_unsafe") {
    helperName = "at_unsafe";
  } else {
    const std::string resolved = resolveCalleePath(callExpr);
    if (resolved == "/count") {
      helperName = "count";
    } else if (resolved == "/at") {
      helperName = "at";
    } else if (resolved == "/at_unsafe") {
      helperName = "at_unsafe";
    } else {
      return "";
    }
  }
  if (defMap_.count("/" + helperName) > 0) {
    return "";
  }
  auto tryResolveReceiverIndex = [&](size_t index) -> bool {
    if (index >= callExpr.args.size()) {
      return false;
    }
    const Expr &receiver = callExpr.args[index];
    if (receiver.kind == Expr::Kind::Name) {
      auto it = ctx.locals.find(receiver.name);
      if (it != ctx.locals.end() && effectFreeCollectionPathFromBinding(it->second) == "/map") {
        return true;
      }
    }
    return effectFreeCollectionPathFromCallExpr(receiver) == "/map";
  };
  if (hasNamedArguments(callExpr.argNames)) {
    bool foundValues = false;
    for (size_t i = 0; i < callExpr.args.size(); ++i) {
      if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value() &&
          *callExpr.argNames[i] == "values") {
        foundValues = true;
        if (tryResolveReceiverIndex(i)) {
          return canonicalKeyValueHelperPathLocal(helperName);
        }
        break;
      }
    }
    if (!foundValues) {
      for (size_t i = 0; i < callExpr.args.size(); ++i) {
        if (tryResolveReceiverIndex(i)) {
          return canonicalKeyValueHelperPathLocal(helperName);
        }
      }
    }
  } else {
    for (size_t i = 0; i < callExpr.args.size(); ++i) {
      if (tryResolveReceiverIndex(i)) {
        return canonicalKeyValueHelperPathLocal(helperName);
      }
    }
  }
  return "";
}

} // namespace primec::semantics
