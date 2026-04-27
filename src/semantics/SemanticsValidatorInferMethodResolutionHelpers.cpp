#include "SemanticsValidator.h"

#include "primec/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <string_view>
#include <vector>

namespace primec::semantics {
namespace {

std::string_view pathLeaf(std::string_view path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

bool isRootedPath(std::string_view path) {
  return !path.empty() && path.front() == '/';
}

bool isSurfaceBasePath(const StdlibSurfaceMetadata &metadata, std::string_view path) {
  return isRootedPath(path) && pathLeaf(path) == pathLeaf(metadata.canonicalPath);
}

void appendUniqueRooted(std::vector<std::string_view> &out, std::string_view path) {
  if (!isRootedPath(path)) {
    return;
  }
  if (std::find(out.begin(), out.end(), path) == out.end()) {
    out.push_back(path);
  }
}

void appendUniquePath(std::vector<std::string> &out, std::string_view path) {
  if (std::find(out.begin(), out.end(), path) == out.end()) {
    out.emplace_back(path);
  }
}

void appendSurfaceBasePaths(std::vector<std::string_view> &out,
                            const StdlibSurfaceMetadata &metadata) {
  appendUniqueRooted(out, metadata.canonicalPath);
  for (const std::string_view spelling : metadata.importAliasSpellings) {
    if (isSurfaceBasePath(metadata, spelling)) {
      appendUniqueRooted(out, spelling);
    }
  }
  for (const std::string_view spelling : metadata.compatibilitySpellings) {
    if (isSurfaceBasePath(metadata, spelling)) {
      appendUniqueRooted(out, spelling);
    }
  }
  for (const std::string_view spelling : metadata.loweringSpellings) {
    if (isSurfaceBasePath(metadata, spelling)) {
      appendUniqueRooted(out, spelling);
    }
  }
}

void appendSurfaceHelperPaths(std::vector<std::string> &out,
                              const StdlibSurfaceMetadata &metadata,
                              std::string_view helperName) {
  std::vector<std::string_view> basePaths;
  appendSurfaceBasePaths(basePaths, metadata);
  for (const std::string_view basePath : basePaths) {
    appendUniquePath(out, std::string(basePath) + "/" + std::string(helperName));
  }
}

void appendSurfaceExactHelperFallbacks(std::vector<std::string> &out,
                                       const StdlibSurfaceMetadata &metadata,
                                       std::string_view helperName) {
  auto appendFrom = [&](std::span<const std::string_view> spellings) {
    for (const std::string_view spelling : spellings) {
      if (isRootedPath(spelling) && pathLeaf(spelling) == helperName) {
        appendUniquePath(out, spelling);
      }
    }
  };
  appendFrom(metadata.compatibilitySpellings);
  appendFrom(metadata.loweringSpellings);
}

std::string_view normalizeFileHelperName(std::string_view helperName) {
  if (helperName == "openRead") {
    return "open_read";
  }
  if (helperName == "openWrite") {
    return "open_write";
  }
  if (helperName == "openAppend") {
    return "open_append";
  }
  if (helperName == "readByte") {
    return "read_byte";
  }
  if (helperName == "writeLine") {
    return "write_line";
  }
  if (helperName == "writeByte") {
    return "write_byte";
  }
  if (helperName == "writeBytes") {
    return "write_bytes";
  }
  return helperName;
}

std::string_view normalizeFileErrorHelperName(std::string_view helperName) {
  if (helperName == "isEof") {
    return "is_eof";
  }
  return helperName;
}

template <typename Predicate>
std::string firstMatchingPath(const std::vector<std::string> &candidates,
                              Predicate &&predicate) {
  for (const auto &candidate : candidates) {
    if (predicate(candidate)) {
      return candidate;
    }
  }
  return {};
}

}  // namespace

std::string SemanticsValidator::inferMethodCollectionTypePathFromTypeText(
    const std::string &typeText) const {
  const std::string normalizedType = normalizeBindingTypeName(typeText);
  std::string normalizedCollectionType = normalizeCollectionTypePath(normalizedType);
  if (!normalizedCollectionType.empty()) {
    return normalizedCollectionType;
  }
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedType, base, argText)) {
    return {};
  }
  base = normalizeBindingTypeName(base);
  if (base == "Reference" || base == "Pointer") {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return {};
    }
    return inferMethodCollectionTypePathFromTypeText(args.front());
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args)) {
    return {};
  }
  if ((base == "array" || base == "vector" || base == "soa_vector" || base == "Buffer") &&
      args.size() == 1) {
    return "/" + base;
  }
  if (isExperimentalSoaVectorTypePath(base) && args.size() == 1) {
    return "/soa_vector";
  }
  if (isMapCollectionTypeName(base) && args.size() == 2) {
    return "/map";
  }
  return {};
}

std::string SemanticsValidator::resolveMethodStructTypePath(const std::string &typeName,
                                                            const std::string &namespacePrefix) const {
  if (typeName.empty()) {
    return {};
  }
  if (typeName[0] == '/') {
    return typeName;
  }
  std::string current = namespacePrefix;
  while (true) {
    if (!current.empty()) {
      std::string scoped = current + "/" + typeName;
      if (structNames_.count(scoped) > 0) {
        return scoped;
      }
      if (current.size() > typeName.size()) {
        const size_t start = current.size() - typeName.size();
        if (start > 0 && current[start - 1] == '/' &&
            current.compare(start, typeName.size(), typeName) == 0 &&
            structNames_.count(current) > 0) {
          return current;
        }
      }
    } else {
      std::string root = "/" + typeName;
      if (structNames_.count(root) > 0) {
        return root;
      }
    }
    if (current.empty()) {
      break;
    }
    const size_t slash = current.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      current.clear();
    } else {
      current.erase(slash);
    }
  }
  auto importIt = importAliases_.find(typeName);
  if (importIt != importAliases_.end()) {
    return importIt->second;
  }
  return {};
}

bool SemanticsValidator::hasDefinitionFamilyPath(std::string_view path) const {
  const std::string pathText(path);
  if (defMap_.count(pathText) > 0 || paramsByDef_.count(pathText) > 0) {
    return true;
  }
  auto matchesFamilyPath = [](std::string_view candidate,
                              const std::string &familyPath) {
    const std::string templatedPrefix = familyPath + "<";
    const std::string specializedPrefix = familyPath + "__t";
    const std::string overloadPrefix = familyPath + "__ov";
    return candidate == familyPath ||
           candidate.rfind(templatedPrefix, 0) == 0 ||
           candidate.rfind(specializedPrefix, 0) == 0 ||
           candidate.rfind(overloadPrefix, 0) == 0;
  };
  const std::string baseFamilyPath = [&]() {
    const size_t lastSlash = pathText.find_last_of('/');
    const size_t nameStart = lastSlash == std::string::npos ? 0 : lastSlash + 1;
    const size_t specializationSuffix = pathText.find("__t", nameStart);
    if (specializationSuffix == std::string::npos) {
      return pathText;
    }
    return pathText.substr(0, specializationSuffix);
  }();
  if (baseFamilyPath != pathText &&
      (defMap_.count(baseFamilyPath) > 0 ||
       paramsByDef_.count(baseFamilyPath) > 0)) {
    return true;
  }
  for (const auto &[resolvedPath, params] : paramsByDef_) {
    (void)params;
    if (matchesFamilyPath(resolvedPath, pathText) ||
        (baseFamilyPath != pathText &&
         matchesFamilyPath(resolvedPath, baseFamilyPath))) {
      return true;
    }
  }
  for (const auto &def : program_.definitions) {
    if (matchesFamilyPath(def.fullPath, pathText) ||
        (baseFamilyPath != pathText &&
         matchesFamilyPath(def.fullPath, baseFamilyPath))) {
      return true;
    }
  }
  return false;
}

std::string SemanticsValidator::preferredFileHelperTarget(
    std::string_view helperName,
    std::string_view currentDefinitionPath) const {
  helperName = normalizeFileHelperName(helperName);
  const std::string builtinPath = "/file/" + std::string(helperName);
  if (helperName != "write" && helperName != "write_line" && helperName != "close") {
    return builtinPath;
  }
  const StdlibSurfaceMetadata *metadata =
      findStdlibSurfaceMetadata(StdlibSurfaceId::FileHelpers);
  if (metadata == nullptr) {
    return builtinPath;
  }
  std::vector<std::string> surfacedHelperPaths;
  appendSurfaceHelperPaths(surfacedHelperPaths, *metadata, helperName);
  for (const auto &surfacedHelperPath : surfacedHelperPaths) {
    if (!currentDefinitionPath.empty() &&
        currentDefinitionPath.rfind(surfacedHelperPath, 0) == 0) {
      return builtinPath;
    }
  }
  const std::string preferred = firstMatchingPath(
      surfacedHelperPaths, [&](const std::string &candidate) {
        return hasDefinitionFamilyPath(candidate);
      });
  return preferred.empty() ? builtinPath : preferred;
}

std::string SemanticsValidator::preferredFileErrorHelperTarget(std::string_view helperName) const {
  helperName = normalizeFileErrorHelperName(helperName);
  const StdlibSurfaceMetadata *metadata =
      findStdlibSurfaceMetadata(StdlibSurfaceId::FileErrorHelpers);
  if (metadata == nullptr) {
    return helperName == "why" ? "/file_error/why" : std::string{};
  }
  std::vector<std::string> helperPaths;
  appendSurfaceHelperPaths(helperPaths, *metadata, helperName);
  appendSurfaceExactHelperFallbacks(helperPaths, *metadata, helperName);
  const std::string preferred = firstMatchingPath(
      helperPaths, [&](const std::string &candidate) {
        return hasDefinitionFamilyPath(candidate);
      });
  if (!preferred.empty()) {
    return preferred;
  }
  const std::string importedPreferred = firstMatchingPath(
      helperPaths, [&](const std::string &candidate) {
        return hasImportedDefinitionPath(candidate);
      });
  if (!importedPreferred.empty()) {
    return importedPreferred;
  }
  return helperName == "why" ? "/file_error/why" : std::string{};
}

std::string SemanticsValidator::preferredImageErrorHelperTarget(std::string_view helperName) const {
  if (helperName == "why") {
    if (defMap_.count("/std/image/ImageError/why") > 0) {
      return "/std/image/ImageError/why";
    }
    if (defMap_.count("/ImageError/why") > 0) {
      return "/ImageError/why";
    }
    return {};
  }
  if (helperName == "status") {
    if (defMap_.count("/std/image/ImageError/status") > 0) {
      return "/std/image/ImageError/status";
    }
    if (defMap_.count("/ImageError/status") > 0) {
      return "/ImageError/status";
    }
    if (defMap_.count("/std/image/imageErrorStatus") > 0) {
      return "/std/image/imageErrorStatus";
    }
    return {};
  }
  if (helperName == "result") {
    if (defMap_.count("/std/image/ImageError/result") > 0) {
      return "/std/image/ImageError/result";
    }
    if (defMap_.count("/ImageError/result") > 0) {
      return "/ImageError/result";
    }
    if (defMap_.count("/std/image/imageErrorResult") > 0) {
      return "/std/image/imageErrorResult";
    }
    return {};
  }
  return {};
}

std::string SemanticsValidator::preferredContainerErrorHelperTarget(std::string_view helperName) const {
  const StdlibSurfaceMetadata *metadata =
      findStdlibSurfaceMetadata(StdlibSurfaceId::CollectionsContainerErrorHelpers);
  if (metadata == nullptr) {
    return {};
  }
  std::vector<std::string> helperPaths;
  appendSurfaceHelperPaths(helperPaths, *metadata, helperName);
  appendSurfaceExactHelperFallbacks(helperPaths, *metadata, helperName);
  return firstMatchingPath(helperPaths, [&](const std::string &candidate) {
    return hasDefinitionFamilyPath(candidate);
  });
}

std::string SemanticsValidator::preferredGfxErrorHelperTarget(
    std::string_view helperName,
    const std::string &resolvedStructPath) const {
  const StdlibSurfaceMetadata *metadata =
      findStdlibSurfaceMetadata(StdlibSurfaceId::GfxErrorHelpers);
  if (metadata == nullptr) {
    return {};
  }
  std::vector<std::string_view> basePaths;
  appendSurfaceBasePaths(basePaths, *metadata);
  auto helperForBasePath = [&](std::string_view basePath) -> std::string {
    const std::string helperPath = std::string(basePath) + "/" + std::string(helperName);
    if (hasDefinitionFamilyPath(helperPath)) {
      return helperPath;
    }
    return {};
  };
  if (!resolvedStructPath.empty()) {
    const std::string resolvedHelperPath = helperForBasePath(resolvedStructPath);
    if (!resolvedHelperPath.empty()) {
      return resolvedHelperPath;
    }
    if (std::find(basePaths.begin(), basePaths.end(), std::string_view(resolvedStructPath)) !=
        basePaths.end()) {
      return {};
    }
  }
  const std::string canonicalHelperPath = helperForBasePath(metadata->canonicalPath);
  std::string experimentalHelperPath;
  std::string rootedAliasHelperPath;
  for (const std::string_view basePath : basePaths) {
    if (basePath == metadata->canonicalPath) {
      continue;
    }
    const std::string candidate = helperForBasePath(basePath);
    if (candidate.empty()) {
      continue;
    }
    if (basePath.rfind("/std/gfx/experimental/", 0) == 0) {
      experimentalHelperPath = candidate;
      continue;
    }
    if (basePath == "/GfxError") {
      rootedAliasHelperPath = candidate;
    }
  }
  if (!canonicalHelperPath.empty() && experimentalHelperPath.empty()) {
    return canonicalHelperPath;
  }
  if (canonicalHelperPath.empty() && !experimentalHelperPath.empty()) {
    return experimentalHelperPath;
  }
  if (canonicalHelperPath.empty() && experimentalHelperPath.empty() &&
      !rootedAliasHelperPath.empty()) {
    return rootedAliasHelperPath;
  }
  std::vector<std::string> helperPaths;
  appendSurfaceExactHelperFallbacks(helperPaths, *metadata, helperName);
  return firstMatchingPath(helperPaths, [&](const std::string &candidate) {
    return hasDefinitionFamilyPath(candidate);
  });
}

std::string SemanticsValidator::preferredMapMethodTargetForCall(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiverExpr,
    const std::string &helperName) {
  std::string keyType;
  std::string valueType;
  if (resolveInferExperimentalMapTarget(params, locals, receiverExpr, keyType, valueType)) {
    const std::string canonical = "/std/collections/map/" + helperName;
    if (hasDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
      return canonical;
    }
    return preferredCanonicalExperimentalMapHelperTarget(helperName);
  }
  const std::string canonical = "/std/collections/map/" + helperName;
  const std::string alias = "/map/" + helperName;
  if (hasDefinitionPath(alias)) {
    return alias;
  }
  if (hasDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
    return canonical;
  }
  if (hasImportedDefinitionPath(alias)) {
    return alias;
  }
  return canonical;
}

std::string SemanticsValidator::preferredBufferMethodTargetForCall(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiverExpr,
    const std::string &helperName) {
  auto helperForBasePath = [&](std::string_view basePath) -> std::string {
    const std::string helperPath = std::string(basePath) + "/" + helperName;
    if (defMap_.count(helperPath) > 0) {
      return helperPath;
    }
    return {};
  };
  auto resolveBufferStructPath = [&](const Expr &candidate) -> std::string {
    std::string candidateTypeText;
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        candidateTypeText = paramBinding->typeName;
      } else {
        auto localIt = locals.find(candidate.name);
        if (localIt != locals.end()) {
          candidateTypeText = localIt->second.typeName;
        }
      }
    }
    if (candidateTypeText.empty() &&
        inferQueryExprTypeText(candidate, params, locals, candidateTypeText) &&
        !candidateTypeText.empty()) {
      candidateTypeText = normalizeBindingTypeName(candidateTypeText);
    }
    if (candidateTypeText.empty()) {
      return {};
    }
    return resolveMethodStructTypePath(normalizeBindingTypeName(candidateTypeText),
                                       candidate.namespacePrefix);
  };
  const std::string canonicalBase = "/std/gfx/Buffer";
  const std::string experimentalBase = "/std/gfx/experimental/Buffer";
  const std::string resolvedStructPath = resolveBufferStructPath(receiverExpr);
  if (resolvedStructPath == canonicalBase) {
    return helperForBasePath(canonicalBase);
  }
  if (resolvedStructPath == experimentalBase) {
    return helperForBasePath(experimentalBase);
  }
  const bool hasCanonical = defMap_.count(canonicalBase + "/" + helperName) > 0;
  const bool hasExperimental = defMap_.count(experimentalBase + "/" + helperName) > 0;
  if (hasCanonical && !hasExperimental) {
    return helperForBasePath(canonicalBase);
  }
  if (!hasCanonical && hasExperimental) {
    return helperForBasePath(experimentalBase);
  }
  return {};
}

} // namespace primec::semantics
