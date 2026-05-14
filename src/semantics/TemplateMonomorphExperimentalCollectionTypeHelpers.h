#pragma once

#include "MapConstructorHelpers.h"

std::string experimentalMapConstructorInferencePath(const std::string &resolvedPath) {
  return metadataBackedExperimentalMapConstructorRewritePath(resolvedPath, 0);
}

bool isExperimentalMapConstructorHelperPath(const std::string &resolvedPath) {
  const std::string normalizedPath = stripMapConstructorSuffixes(resolvedPath);
  const std::string experimentalMapRoot =
      experimentalCollectionConstructorRootLocal("map");
  std::string memberName;
  return normalizedPath.rfind(experimentalMapRoot, 0) == 0 &&
         resolveMapConstructorMemberPath(normalizedPath, memberName) &&
         memberName != "map";
}

std::string experimentalVectorConstructorInferencePath(const std::string &resolvedPath) {
  return metadataBackedExperimentalVectorConstructorCompatibilityPath(
      resolvedPath);
}

bool isExperimentalVectorConstructorHelperPath(const std::string &resolvedPath) {
  const std::string normalizedPath =
      stripCollectionConstructorSuffixes(resolvedPath);
  return normalizedPath.rfind(legacyExperimentalVectorCompatibilityPrefix(), 0) == 0 &&
         isResolvedVectorConstructorHelperPath(normalizedPath);
}

bool resolvesExperimentalVectorValueTypeText(const std::string &typeText) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    std::string normalizedBase = normalizeBindingTypeName(base);
    if (!normalizedBase.empty() && normalizedBase.front() == '/') {
      normalizedBase.erase(normalizedBase.begin());
    }
    if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
      return false;
    }
    if ((normalizedBase == "Vector" ||
         isLegacyExperimentalVectorCompatibilityPath("/" + normalizedBase)) &&
        !argText.empty()) {
      std::vector<std::string> args;
      return splitTopLevelTemplateArgs(argText, args) && args.size() == 1;
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
  return isLegacyExperimentalVectorCompatibilitySpecializedTypePath(
      "/" + normalizedResolvedPath);
}

bool extractVectorValueTypeFromTypeText(const std::string &typeText, std::string &valueTypeOut) {
  valueTypeOut.clear();
  std::string normalizedType = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedType, base, argText) || argText.empty()) {
    return false;
  }
  const std::string normalizedBase = normalizeBindingTypeName(base);
  if (normalizedBase != "vector" &&
      normalizedBase != "Vector" &&
      !isLegacyExperimentalVectorCompatibilityPath(normalizedBase) &&
      !isLegacyExperimentalVectorCompatibilityPath("/" + normalizedBase)) {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
    return false;
  }
  valueTypeOut = args.front();
  return true;
}
