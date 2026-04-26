#pragma once

#include "MapConstructorHelpers.h"
#include "primec/StdlibSurfaceRegistry.h"

#include <string>
#include <string_view>

namespace primec::semantics {
namespace {

enum class RemovedCollectionHelperFamily {
  VectorLike,
  Map,
};

[[maybe_unused]] std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

[[maybe_unused]] bool isDirectMapConstructorPath(std::string_view resolvedCandidate) {
  return isResolvedMapConstructorPath(std::string(resolvedCandidate));
}

[[maybe_unused]] bool matchesResolvedPath(std::string_view resolvedPath,
                                          std::string_view basePath) {
  return resolvedPath == basePath ||
         resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
}

[[maybe_unused]] std::string_view trimLeadingSlash(std::string_view text) {
  if (!text.empty() && text.front() == '/') {
    text.remove_prefix(1);
  }
  return text;
}

[[maybe_unused]] bool resolvePublishedCollectionHelperMemberToken(
    std::string_view memberToken,
    StdlibSurfaceId surfaceId,
    std::string &memberNameOut) {
  memberNameOut.clear();
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, trimLeadingSlash(memberToken));
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

[[maybe_unused]] bool resolvePublishedCollectionHelperResolvedPath(
    std::string_view resolvedPath,
    StdlibSurfaceId surfaceId,
    std::string &memberNameOut) {
  memberNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  if (metadata == nullptr || metadata->id != surfaceId) {
    return false;
  }
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, resolvedPath);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

[[maybe_unused]] inline bool isVectorCompatibilityHelperName(
    std::string_view helperName) {
  return isStdlibSurfaceMemberName(StdlibSurfaceId::CollectionsVectorHelpers,
                                   helperName);
}

[[maybe_unused]] inline bool isStdNamespacedVectorCompatibilityHelperPath(
    std::string_view path,
    std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata =
      findStdlibSurfaceMetadata(StdlibSurfaceId::CollectionsVectorHelpers);
  return metadata != nullptr &&
         isVectorCompatibilityHelperName(helperName) &&
         path.rfind(std::string(metadata->canonicalPath) + "/" +
                        std::string(helperName),
                    0) == 0;
}

[[maybe_unused]] inline bool isStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName) {
  return !isMethodCall &&
         isStdNamespacedVectorCompatibilityHelperPath(path, helperName);
}

[[maybe_unused]] inline bool isImportedStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasImportedHelper) {
  return hasImportedHelper &&
         isStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                      path,
                                                      helperName);
}

[[maybe_unused]] inline bool isUnimportedStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasImportedHelper) {
  return !hasImportedHelper &&
         isStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                      path,
                                                      helperName);
}

[[maybe_unused]] inline bool isUnavailableStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool helperAvailable) {
  return !helperAvailable &&
         isStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                      path,
                                                      helperName);
}

[[maybe_unused]] inline bool isInvisibleStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasVisibleHelper) {
  return isUnavailableStdNamespacedVectorCompatibilityDirectCall(
      isMethodCall, path, helperName, hasVisibleHelper);
}

[[maybe_unused]] inline bool isUndeclaredStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasDeclaredHelper) {
  return isUnavailableStdNamespacedVectorCompatibilityDirectCall(
      isMethodCall, path, helperName, hasDeclaredHelper);
}

[[maybe_unused]] inline bool isUnresolvableStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasResolvableHelper) {
  return isUnavailableStdNamespacedVectorCompatibilityDirectCall(
      isMethodCall, path, helperName, hasResolvableHelper);
}

[[maybe_unused]] inline bool isImportedResolvedStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasImportedHelper,
    bool resolvedMethod,
    size_t argCount,
    std::string_view resolvedPath) {
  return !resolvedMethod &&
         argCount == 1 &&
         isImportedStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                              path,
                                                              helperName,
                                                              hasImportedHelper) &&
         isStdNamespacedVectorCompatibilityHelperPath(resolvedPath, helperName);
}

[[maybe_unused]] inline std::string vectorCompatibilityRequiresVectorTargetDiagnostic(
    std::string_view helperName) {
  return std::string(helperName) + " requires vector target";
}

[[maybe_unused]] inline std::string vectorCompatibilityUnknownCallTargetDiagnostic(
    std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata =
      findStdlibSurfaceMetadata(StdlibSurfaceId::CollectionsVectorHelpers);
  const std::string_view canonicalPath =
      metadata != nullptr ? metadata->canonicalPath
                          : std::string_view("/std/collections/vector");
  return "unknown call target: " + std::string(canonicalPath) + "/" +
         std::string(helperName);
}

[[maybe_unused]] inline std::string classifyStdNamespacedVectorCountDiagnosticMessage(
    bool callsInvisibleHelper,
    bool isDirectWrapperMapTarget,
    bool callsUndeclaredHelper,
    bool resolvesMapTarget,
    bool callsUnresolvableHelper) {
  if (callsInvisibleHelper ||
      (isDirectWrapperMapTarget && callsUndeclaredHelper)) {
    return vectorCompatibilityUnknownCallTargetDiagnostic("count");
  }
  if (resolvesMapTarget && callsUnresolvableHelper) {
    return vectorCompatibilityRequiresVectorTargetDiagnostic("count");
  }
  return "";
}

[[maybe_unused]] bool isPublishedMapBaseHelperName(std::string_view helperName) {
  return isStdlibMapBaseHelperName(helperName);
}

[[maybe_unused]] bool isPublishedBorrowedMapHelperName(std::string_view helperName) {
  return isStdlibMapBorrowedHelperName(helperName);
}

[[maybe_unused]] bool isRemovedPublishedVectorStatementHelperName(
    std::string_view helperName) {
  return isStdlibVectorStatementHelperName(helperName);
}

[[maybe_unused]] std::string canonicalCollectionHelperPath(
    StdlibSurfaceId surfaceId,
    std::string_view helperName) {
  switch (surfaceId) {
    case StdlibSurfaceId::CollectionsVectorHelpers:
      return "/std/collections/vector/" + std::string(helperName);
    case StdlibSurfaceId::CollectionsMapHelpers:
      return "/std/collections/map/" + std::string(helperName);
    case StdlibSurfaceId::CollectionsSoaVectorHelpers:
      return "/std/collections/soa_vector/" + std::string(helperName);
    default:
      return "";
  }
}

[[maybe_unused]] std::string preferredPublishedCollectionLoweringPath(
    std::string_view helperName,
    StdlibSurfaceId surfaceId,
    std::string_view preferredPrefix) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return "";
  }
  std::string fallback;
  for (const std::string_view spelling : metadata->loweringSpellings) {
    if (resolveStdlibSurfaceMemberName(*metadata, spelling) != helperName) {
      continue;
    }
    if (spelling.rfind(preferredPrefix, 0) == 0) {
      return std::string(spelling);
    }
    if (fallback.empty()) {
      fallback.assign(spelling);
    }
  }
  return fallback;
}

[[maybe_unused]] bool resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(
    std::string_view resolvedPath,
    std::string &helperNameOut) {
  helperNameOut.clear();
  if (resolvedPath.rfind("/std/collections/experimental_map/", 0) == 0) {
    return false;
  }
  if (!resolvePublishedCollectionHelperResolvedPath(
          resolvedPath, StdlibSurfaceId::CollectionsMapHelpers, helperNameOut)) {
    return false;
  }
  return isPublishedMapBaseHelperName(helperNameOut) ||
         isPublishedBorrowedMapHelperName(helperNameOut);
}

[[maybe_unused]] bool resolveCanonicalVectorHelperNameFromResolvedPath(
    std::string_view resolvedPath,
    std::string &helperNameOut) {
  helperNameOut.clear();
  if (resolvedPath.rfind("/std/collections/vector/", 0) != 0) {
    return false;
  }
  return resolvePublishedCollectionHelperResolvedPath(
      resolvedPath, StdlibSurfaceId::CollectionsVectorHelpers, helperNameOut);
}

[[maybe_unused]] bool resolveExplicitPublishedMapHelperExprMemberName(
    std::string_view rawMethodName,
    std::string_view namespacePrefix,
    std::string &helperNameOut) {
  helperNameOut.clear();
  rawMethodName = trimLeadingSlash(rawMethodName);
  namespacePrefix = trimLeadingSlash(namespacePrefix);

  if (namespacePrefix == "map" || namespacePrefix == "std/collections/map") {
    if (!resolvePublishedCollectionHelperMemberToken(
            rawMethodName, StdlibSurfaceId::CollectionsMapHelpers, helperNameOut)) {
      return false;
    }
  } else if (rawMethodName.rfind("map/", 0) == 0 ||
             rawMethodName.rfind("std/collections/map/", 0) == 0) {
    if (!resolvePublishedCollectionHelperResolvedPath(
            "/" + std::string(rawMethodName),
            StdlibSurfaceId::CollectionsMapHelpers,
            helperNameOut)) {
      return false;
    }
  } else {
    return false;
  }

  return isPublishedMapBaseHelperName(helperNameOut) ||
         isPublishedBorrowedMapHelperName(helperNameOut);
}

[[maybe_unused]] bool resolveRemovedCollectionHelperReference(
    std::string_view rawMethodName,
    std::string_view namespacePrefix,
    RemovedCollectionHelperFamily &familyOut,
    std::string_view &helperNameOut,
    bool &preserveArrayPathOut) {
  rawMethodName = trimLeadingSlash(rawMethodName);
  namespacePrefix = trimLeadingSlash(namespacePrefix);
  preserveArrayPathOut = false;

  auto setVectorLike = [&](std::string_view helperName, bool preserveArrayPath) {
    if (!isRemovedPublishedVectorStatementHelperName(helperName)) {
      return false;
    }
    helperNameOut = helperName;
    familyOut = RemovedCollectionHelperFamily::VectorLike;
    preserveArrayPathOut = preserveArrayPath;
    return true;
  };
  auto setMap = [&](std::string_view helperName) {
    if (!isPublishedMapBaseHelperName(helperName)) {
      return false;
    }
    helperNameOut = helperName;
    familyOut = RemovedCollectionHelperFamily::Map;
    preserveArrayPathOut = false;
    return true;
  };

  if (namespacePrefix == "array") {
    return setVectorLike(rawMethodName, true);
  }
  if (namespacePrefix == "std/collections/vector") {
    return setVectorLike(rawMethodName, false);
  }
  if (namespacePrefix == "map" || namespacePrefix == "std/collections/map") {
    return setMap(rawMethodName);
  }
  if (rawMethodName.rfind("array/", 0) == 0) {
    return setVectorLike(
        rawMethodName.substr(std::string_view("array/").size()), true);
  }
  if (rawMethodName.rfind("std/collections/vector/", 0) == 0) {
    return setVectorLike(
        rawMethodName.substr(
            std::string_view("std/collections/vector/").size()),
        false);
  }
  if (rawMethodName.rfind("map/", 0) == 0 ||
      rawMethodName.rfind("std/collections/map/", 0) == 0) {
    std::string helperName;
    if (!resolvePublishedCollectionHelperResolvedPath(
            "/" + std::string(rawMethodName),
            StdlibSurfaceId::CollectionsMapHelpers,
            helperName)) {
      return false;
    }
    return setMap(helperName);
  }
  return false;
}

[[maybe_unused]] std::string removedCollectionMethodPath(
    RemovedCollectionHelperFamily family,
    std::string_view helperName,
    bool preserveArrayPath) {
  if (family == RemovedCollectionHelperFamily::VectorLike) {
    if (!isRemovedPublishedVectorStatementHelperName(helperName)) {
      return "";
    }
    if (preserveArrayPath) {
      return "/array/" + std::string(helperName);
    }
    return canonicalCollectionHelperPath(
        StdlibSurfaceId::CollectionsVectorHelpers, helperName);
  }
  if (!isPublishedMapBaseHelperName(helperName)) {
    return "";
  }
  return "/map/" + std::string(helperName);
}

} // namespace
} // namespace primec::semantics
