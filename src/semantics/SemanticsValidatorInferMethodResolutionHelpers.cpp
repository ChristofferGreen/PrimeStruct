#include "SemanticsValidator.h"

#include <string_view>

namespace primec::semantics {

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
  if (defMap_.count(std::string(path)) > 0) {
    return true;
  }
  const std::string templatedPrefix = std::string(path) + "<";
  const std::string specializedPrefix = std::string(path) + "__t";
  for (const auto &def : program_.definitions) {
    if (def.fullPath == path || def.fullPath.rfind(templatedPrefix, 0) == 0 ||
        def.fullPath.rfind(specializedPrefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

std::string SemanticsValidator::preferredFileErrorHelperTarget(std::string_view helperName) const {
  if (helperName == "why") {
    if (hasDefinitionFamilyPath("/std/file/FileError/why")) {
      return "/std/file/FileError/why";
    }
    if (hasDefinitionFamilyPath("/FileError/why")) {
      return "/FileError/why";
    }
    return "/file_error/why";
  }
  if (helperName == "is_eof") {
    if (hasDefinitionFamilyPath("/std/file/FileError/is_eof")) {
      return "/std/file/FileError/is_eof";
    }
    if (hasDefinitionFamilyPath("/FileError/is_eof")) {
      return "/FileError/is_eof";
    }
    if (hasDefinitionFamilyPath("/std/file/fileErrorIsEof")) {
      return "/std/file/fileErrorIsEof";
    }
    return {};
  }
  if (helperName == "eof") {
    if (hasDefinitionFamilyPath("/std/file/FileError/eof")) {
      return "/std/file/FileError/eof";
    }
    if (hasDefinitionFamilyPath("/FileError/eof")) {
      return "/FileError/eof";
    }
    if (hasDefinitionFamilyPath("/std/file/fileReadEof")) {
      return "/std/file/fileReadEof";
    }
    return {};
  }
  if (helperName == "status") {
    if (hasDefinitionFamilyPath("/std/file/FileError/status")) {
      return "/std/file/FileError/status";
    }
    return {};
  }
  if (helperName == "result") {
    if (hasDefinitionFamilyPath("/std/file/FileError/result")) {
      return "/std/file/FileError/result";
    }
    return {};
  }
  return {};
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
  if (helperName == "why") {
    if (defMap_.count("/std/collections/ContainerError/why") > 0) {
      return "/std/collections/ContainerError/why";
    }
    if (defMap_.count("/ContainerError/why") > 0) {
      return "/ContainerError/why";
    }
    return {};
  }
  if (helperName == "status") {
    if (defMap_.count("/std/collections/ContainerError/status") > 0) {
      return "/std/collections/ContainerError/status";
    }
    if (defMap_.count("/ContainerError/status") > 0) {
      return "/ContainerError/status";
    }
    if (defMap_.count("/std/collections/containerErrorStatus") > 0) {
      return "/std/collections/containerErrorStatus";
    }
    return {};
  }
  if (helperName == "result") {
    if (defMap_.count("/std/collections/ContainerError/result") > 0) {
      return "/std/collections/ContainerError/result";
    }
    if (defMap_.count("/ContainerError/result") > 0) {
      return "/ContainerError/result";
    }
    if (defMap_.count("/std/collections/containerErrorResult") > 0) {
      return "/std/collections/containerErrorResult";
    }
    return {};
  }
  return {};
}

std::string SemanticsValidator::preferredGfxErrorHelperTarget(
    std::string_view helperName,
    const std::string &resolvedStructPath) const {
  auto helperForBasePath = [&](std::string_view basePath) -> std::string {
    const std::string helperPath = std::string(basePath) + "/" + std::string(helperName);
    if (defMap_.count(helperPath) > 0) {
      return helperPath;
    }
    return {};
  };
  const std::string canonicalBase = "/std/gfx/GfxError";
  const std::string experimentalBase = "/std/gfx/experimental/GfxError";
  if (resolvedStructPath == canonicalBase) {
    return helperForBasePath(canonicalBase);
  }
  if (resolvedStructPath == experimentalBase) {
    return helperForBasePath(experimentalBase);
  }
  const bool hasCanonical = defMap_.count(canonicalBase + "/" + std::string(helperName)) > 0;
  const bool hasExperimental = defMap_.count(experimentalBase + "/" + std::string(helperName)) > 0;
  if (hasCanonical && !hasExperimental) {
    return helperForBasePath(canonicalBase);
  }
  if (!hasCanonical && hasExperimental) {
    return helperForBasePath(experimentalBase);
  }
  return {};
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
  if (hasDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
    return canonical;
  }
  if (hasDefinitionPath(alias)) {
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
