#include "IrLowererSetupTypeHelpers.h"

#include "IrLowererSetupTypeCollectionHelpers.h"

namespace primec::ir_lowerer {

const Definition *resolveMethodDefinitionFromReceiverTarget(
    const std::string &methodName,
    const std::string &typeName,
    const std::string &resolvedTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
  const bool isExplicitRemovedVectorMethodAlias = isExplicitRemovedVectorMethodAliasPath(methodName);
  const bool isExplicitMapMethodAlias = isExplicitMapMethodAliasPath(methodName);
  const bool isExplicitMapContainsOrTryAtCompatibilityMethodAlias =
      isExplicitMapContainsOrTryAtCompatibilityMethodAliasPath(methodName);
  std::string normalizedMethodName = methodName;
  if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
    normalizedMethodName.erase(normalizedMethodName.begin());
  }
  if (normalizedMethodName.rfind("vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("vector/").size());
  } else if (normalizedMethodName.rfind("array/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
  } else if (normalizedMethodName.rfind("std/collections/vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/vector/").size());
  } else if (normalizedMethodName.rfind("map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("map/").size());
  } else if (normalizedMethodName.rfind("std/collections/map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/map/").size());
  }
  std::string normalizedOriginalMethodName = methodName;
  if (!normalizedOriginalMethodName.empty() && normalizedOriginalMethodName.front() == '/') {
    normalizedOriginalMethodName.erase(normalizedOriginalMethodName.begin());
  }
  const bool isExplicitCanonicalMapMethodAlias =
      normalizedOriginalMethodName.rfind("std/collections/map/", 0) == 0;
  const bool isExplicitCompatibilityMapMethodAlias =
      isExplicitMapMethodAlias && !isExplicitCanonicalMapMethodAlias;
  const bool isExplicitVectorAliasMethod =
      normalizedOriginalMethodName.rfind("vector/", 0) == 0;
  const bool isExplicitCanonicalVectorMethod =
      normalizedOriginalMethodName.rfind("std/collections/vector/", 0) == 0;
  std::string normalizedTypeName = typeName;
  if (!normalizedTypeName.empty() && normalizedTypeName.front() == '/') {
    normalizedTypeName.erase(normalizedTypeName.begin());
  }
  auto stripReceiverPrefix = [&](std::string candidate) {
    if (!candidate.empty() && candidate.front() == '/') {
      candidate.erase(candidate.begin());
    }
    return candidate;
  };
  auto isVectorReceiverTarget = [&](const std::string &candidate) {
    const std::string normalized = stripReceiverPrefix(candidate);
    return normalized == "vector" || normalized.rfind("vector<", 0) == 0 ||
           normalized == "std/collections/vector" || normalized.rfind("std/collections/vector<", 0) == 0 ||
           normalized == "Vector" || normalized.rfind("Vector<", 0) == 0 ||
           normalized == "std/collections/experimental_vector/Vector" ||
           normalized.rfind("std/collections/experimental_vector/Vector<", 0) == 0;
  };
  auto isSoaVectorReceiverTarget = [&](const std::string &candidate) {
    const std::string normalized = stripReceiverPrefix(candidate);
    return normalized == "soa_vector" || normalized.rfind("soa_vector<", 0) == 0 ||
           normalized == "std/collections/soa_vector" ||
           normalized.rfind("std/collections/soa_vector<", 0) == 0;
  };
  auto isMapReceiverTarget = [&](const std::string &candidate) {
    const std::string normalized = stripReceiverPrefix(candidate);
    return normalized == "map" || normalized.rfind("map<", 0) == 0 ||
           normalized == "std/collections/map" || normalized.rfind("std/collections/map<", 0) == 0 ||
           normalized == "Map" || normalized.rfind("Map<", 0) == 0 ||
           normalized == "std/collections/experimental_map/Map" ||
           normalized.rfind("std/collections/experimental_map/Map<", 0) == 0;
  };
  auto isBufferReceiverTarget = [&](const std::string &candidate) {
    return candidate == "Buffer" || candidate == "std/gfx/Buffer" || candidate == "std/gfx/experimental/Buffer";
  };
  auto shouldPreferCanonicalMapPath = [&](const std::string &candidate) {
    return !isExplicitCompatibilityMapMethodAlias && !isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
           isMapReceiverTarget(candidate) &&
           (isExplicitCanonicalMapMethodAlias || normalizedMethodName == "count" ||
            normalizedMethodName == "contains" || normalizedMethodName == "tryAt" ||
            normalizedMethodName == "at" || normalizedMethodName == "at_unsafe");
  };
  auto findMethodDefinitionByPath = [&](const std::string &path) -> const Definition * {
    auto defIt = defMap.find(path);
    if (defIt != defMap.end()) {
      return defIt->second;
    }
    if (path.rfind("/array/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string vectorAlias = "/vector/" + suffix;
        defIt = defMap.find(vectorAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
        const std::string stdlibAlias = "/std/collections/vector/" + suffix;
        defIt = defMap.find(stdlibAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
    }
    if (path.rfind("/vector/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        const std::string stdlibAlias = "/std/collections/vector/" + suffix;
        defIt = defMap.find(stdlibAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        defIt = defMap.find(arrayAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
    }
    if (path.rfind("/std/collections/vector/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/std/collections/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        const std::string vectorAlias = "/vector/" + suffix;
        defIt = defMap.find(vectorAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        defIt = defMap.find(arrayAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
    }
    if (path.rfind("/map/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/map/").size());
      if (isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
          (suffix == "contains" || suffix == "tryAt")) {
        return nullptr;
      }
      const std::string stdlibAlias = "/std/collections/map/" + path.substr(std::string("/map/").size());
      defIt = defMap.find(stdlibAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
    }
    if (path.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/std/collections/map/").size());
      if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
          suffix != "at" && suffix != "at_unsafe") {
        const std::string mapAlias = "/map/" + suffix;
        defIt = defMap.find(mapAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
    }
    if (path.rfind("/Buffer/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/Buffer/").size());
      const std::string canonicalAlias = "/std/gfx/Buffer/" + suffix;
      defIt = defMap.find(canonicalAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
      const std::string experimentalAlias = "/std/gfx/experimental/Buffer/" + suffix;
      defIt = defMap.find(experimentalAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
    }
    if (path.rfind("/std/gfx/Buffer/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/std/gfx/Buffer/").size());
      const std::string alias = "/Buffer/" + suffix;
      defIt = defMap.find(alias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
      const std::string experimentalAlias = "/std/gfx/experimental/Buffer/" + suffix;
      defIt = defMap.find(experimentalAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
    }
    if (path.rfind("/std/gfx/experimental/Buffer/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/std/gfx/experimental/Buffer/").size());
      const std::string canonicalAlias = "/std/gfx/Buffer/" + suffix;
      defIt = defMap.find(canonicalAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
      const std::string alias = "/Buffer/" + suffix;
      defIt = defMap.find(alias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
    }
    return nullptr;
  };
  if (isExplicitVectorAliasMethod || isExplicitCanonicalVectorMethod) {
    const std::string explicitPath =
        std::string("/") +
        (isExplicitCanonicalVectorMethod ? "std/collections/vector/" : "vector/") +
        normalizedMethodName;
    if (const Definition *resolved = findMethodDefinitionByPath(explicitPath)) {
      errorOut.clear();
      return resolved;
    }
  }
  if (!resolvedTypePath.empty()) {
    std::string normalizedResolvedTypePath = resolvedTypePath;
    if (!normalizedResolvedTypePath.empty() && normalizedResolvedTypePath.front() != '/') {
      normalizedResolvedTypePath.insert(normalizedResolvedTypePath.begin(), '/');
    }
    std::string resolvedTypeWithoutSlash = normalizedResolvedTypePath;
    if (!resolvedTypeWithoutSlash.empty() && resolvedTypeWithoutSlash.front() == '/') {
      resolvedTypeWithoutSlash.erase(resolvedTypeWithoutSlash.begin());
    }
    const std::string resolvedBase =
        (isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
         isMapReceiverTarget(resolvedTypeWithoutSlash))
            ? "/map"
            : (shouldPreferCanonicalMapPath(resolvedTypeWithoutSlash)
                   ? "/std/collections/map"
                   : normalizedResolvedTypePath);
    const std::string resolved = resolvedBase + "/" + normalizedMethodName;
    if ((normalizedMethodName == "to_soa" && isVectorReceiverTarget(resolvedTypeWithoutSlash)) ||
        (normalizedMethodName == "to_aos" && isSoaVectorReceiverTarget(resolvedTypeWithoutSlash))) {
      errorOut.clear();
      return nullptr;
    }
    if (isExplicitRemovedVectorMethodAlias && isVectorReceiverTarget(resolvedTypeWithoutSlash)) {
      errorOut = "unknown method: " + resolved;
      return nullptr;
    }
    if (isExplicitMapMethodAlias && isMapReceiverTarget(resolvedTypeWithoutSlash)) {
      errorOut = "unknown method: " + resolved;
      return nullptr;
    }
    const Definition *resolvedDef = findMethodDefinitionByPath(resolved);
    if (resolvedDef == nullptr) {
      if (!isExplicitCanonicalMapMethodAlias && !isExplicitCompatibilityMapMethodAlias &&
          !isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
          isMapReceiverTarget(resolvedTypeWithoutSlash) &&
          (normalizedMethodName == "contains" || normalizedMethodName == "tryAt")) {
        errorOut.clear();
        return nullptr;
      }
      errorOut = "unknown method: " + resolved;
      return nullptr;
    }
    return resolvedDef;
  }

  if (normalizedTypeName.empty()) {
    errorOut = "unknown method target for " + normalizedMethodName;
    return nullptr;
  }

  if (isBufferReceiverTarget(normalizedTypeName)) {
    if (const Definition *resolved = findMethodDefinitionByPath("/std/gfx/Buffer/" + normalizedMethodName)) {
      errorOut.clear();
      return resolved;
    }
    if (const Definition *resolved = findMethodDefinitionByPath("/Buffer/" + normalizedMethodName)) {
      errorOut.clear();
      return resolved;
    }
  }

  const std::string resolvedBase =
      (isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
       isMapReceiverTarget(normalizedTypeName))
          ? "/map"
          : (shouldPreferCanonicalMapPath(normalizedTypeName)
                 ? "/std/collections/map"
                 : "/" + normalizedTypeName);
  const std::string resolved = resolvedBase + "/" + normalizedMethodName;
  if ((normalizedMethodName == "to_soa" && isVectorReceiverTarget(normalizedTypeName)) ||
      (normalizedMethodName == "to_aos" && isSoaVectorReceiverTarget(normalizedTypeName))) {
    errorOut.clear();
    return nullptr;
  }
  if (isExplicitRemovedVectorMethodAlias && isVectorReceiverTarget(normalizedTypeName)) {
    errorOut = "unknown method: " + resolved;
    return nullptr;
  }
  if (isExplicitMapMethodAlias && isMapReceiverTarget(normalizedTypeName)) {
    errorOut = "unknown method: " + resolved;
    return nullptr;
  }
  const Definition *resolvedDef = findMethodDefinitionByPath(resolved);
  if (resolvedDef == nullptr) {
    if (!isExplicitCanonicalMapMethodAlias && !isExplicitCompatibilityMapMethodAlias &&
        !isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
        isMapReceiverTarget(normalizedTypeName) &&
        (normalizedMethodName == "contains" || normalizedMethodName == "tryAt")) {
      errorOut.clear();
      return nullptr;
    }
    errorOut = "unknown method: " + resolved;
    return nullptr;
  }
  return resolvedDef;
}

} // namespace primec::ir_lowerer
