#pragma once

std::string experimentalMapConstructorInferencePath(const std::string &resolvedPath) {
  const std::string normalizedPath = stripMapConstructorSuffixes(resolvedPath);
  auto matchesPath = [&](std::string_view basePath) {
    return normalizedPath == basePath;
  };
  if (matchesPath("/std/collections/map/map") || matchesPath("/std/collections/mapNew")) {
    return "/std/collections/experimental_map/mapNew";
  }
  if (matchesPath("/std/collections/mapSingle")) {
    return "/std/collections/experimental_map/mapSingle";
  }
  if (matchesPath("/std/collections/mapDouble")) {
    return "/std/collections/experimental_map/mapDouble";
  }
  if (matchesPath("/std/collections/mapPair")) {
    return "/std/collections/experimental_map/mapPair";
  }
  if (matchesPath("/std/collections/mapTriple")) {
    return "/std/collections/experimental_map/mapTriple";
  }
  if (matchesPath("/std/collections/mapQuad")) {
    return "/std/collections/experimental_map/mapQuad";
  }
  if (matchesPath("/std/collections/mapQuint")) {
    return "/std/collections/experimental_map/mapQuint";
  }
  if (matchesPath("/std/collections/mapSext")) {
    return "/std/collections/experimental_map/mapSext";
  }
  if (matchesPath("/std/collections/mapSept")) {
    return "/std/collections/experimental_map/mapSept";
  }
  if (matchesPath("/std/collections/mapOct")) {
    return "/std/collections/experimental_map/mapOct";
  }
  return {};
}

bool isExperimentalMapConstructorHelperPath(const std::string &resolvedPath) {
  const std::string normalizedPath = stripMapConstructorSuffixes(resolvedPath);
  auto matchesPath = [&](std::string_view basePath) {
    return normalizedPath == basePath;
  };
  return matchesPath("/std/collections/experimental_map/mapNew") ||
         matchesPath("/std/collections/experimental_map/mapSingle") ||
         matchesPath("/std/collections/experimental_map/mapDouble") ||
         matchesPath("/std/collections/experimental_map/mapPair") ||
         matchesPath("/std/collections/experimental_map/mapTriple") ||
         matchesPath("/std/collections/experimental_map/mapQuad") ||
         matchesPath("/std/collections/experimental_map/mapQuint") ||
         matchesPath("/std/collections/experimental_map/mapSext") ||
         matchesPath("/std/collections/experimental_map/mapSept") ||
         matchesPath("/std/collections/experimental_map/mapOct");
}

std::string experimentalVectorConstructorInferencePath(const std::string &resolvedPath) {
  auto matchesPath = [&](std::string_view basePath) {
    return resolvedPath == basePath || resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  if (matchesPath("/std/collections/vectorNew")) {
    return "/std/collections/experimental_vector/vectorNew";
  }
  if (matchesPath("/std/collections/vectorSingle")) {
    return "/std/collections/experimental_vector/vectorSingle";
  }
  if (matchesPath("/std/collections/vectorPair")) {
    return "/std/collections/experimental_vector/vectorPair";
  }
  if (matchesPath("/std/collections/vectorTriple")) {
    return "/std/collections/experimental_vector/vectorTriple";
  }
  if (matchesPath("/std/collections/vectorQuad")) {
    return "/std/collections/experimental_vector/vectorQuad";
  }
  if (matchesPath("/std/collections/vectorQuint")) {
    return "/std/collections/experimental_vector/vectorQuint";
  }
  if (matchesPath("/std/collections/vectorSext")) {
    return "/std/collections/experimental_vector/vectorSext";
  }
  if (matchesPath("/std/collections/vectorSept")) {
    return "/std/collections/experimental_vector/vectorSept";
  }
  if (matchesPath("/std/collections/vectorOct")) {
    return "/std/collections/experimental_vector/vectorOct";
  }
  return {};
}

bool isExperimentalVectorConstructorHelperPath(const std::string &resolvedPath) {
  auto matchesPath = [&](std::string_view basePath) {
    return resolvedPath == basePath || resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  return matchesPath("/std/collections/experimental_vector/vectorNew") ||
         matchesPath("/std/collections/experimental_vector/vectorSingle") ||
         matchesPath("/std/collections/experimental_vector/vectorPair") ||
         matchesPath("/std/collections/experimental_vector/vectorTriple") ||
         matchesPath("/std/collections/experimental_vector/vectorQuad") ||
         matchesPath("/std/collections/experimental_vector/vectorQuint") ||
         matchesPath("/std/collections/experimental_vector/vectorSext") ||
         matchesPath("/std/collections/experimental_vector/vectorSept") ||
         matchesPath("/std/collections/experimental_vector/vectorOct");
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
         normalizedBase == "std/collections/experimental_vector/Vector") &&
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
  return normalizedResolvedPath.rfind("std/collections/experimental_vector/Vector__", 0) == 0;
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
      normalizedBase != "/std/collections/experimental_vector/Vector" &&
      normalizedBase != "std/collections/experimental_vector/Vector") {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
    return false;
  }
  valueTypeOut = args.front();
  return true;
}
