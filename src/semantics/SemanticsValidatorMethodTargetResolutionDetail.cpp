// collection-surface-audit: exempt
#include "SemanticsValidatorMethodTargetResolutionDetail.h"

#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <string>
#include <string_view>
#include <utility>

namespace primec::semantics {
namespace method_target_detail {

// Removed-name membership delegates to the single authoritative sets in
// primec/support/CollectionSpellingClassifier.h (decision D2). This site was one
// of the two registry-membership variants; the D2 agreement unit test
// pins that the registry manifest surfaces cover the same names.
bool isRemovedVectorCompatibilityHelper(std::string_view helperName) {
  return classifierRemovedVectorCompatibilityHelper(helperName);
}

bool isRemovedKeyValueCompatibilityHelper(std::string_view helperName) {
  return classifierRemovedKeyValueCompatibilityHelper(helperName);
}

std::string canonicalKeyValueHelperPathLocal(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  return canonicalCollectionHelperPath(metadata->id, helperName);
}

std::string canonicalKeyValueHelperNamespaceLocal() {
  const StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return "";
  }
  std::string namespacePath(metadata->canonicalPath);
  if (!namespacePath.empty() && namespacePath.front() == '/') {
    namespacePath.erase(namespacePath.begin());
  }
  return namespacePath;
}

bool resolveCanonicalKeyValueHelperNameFromSpelling(
    std::string path,
    std::string &helperNameOut) {
  helperNameOut.clear();
  if (!path.empty() && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  return resolvePublishedCollectionHelperResolvedPath(
      path, metadata->id, helperNameOut);
}

bool isKeyValueHelperImportAliasNamespaceForMethodTargets(
    std::string_view namespacePrefix) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  std::string normalizedPrefix(namespacePrefix);
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (alias.empty() || alias.find('/') != std::string_view::npos) {
      continue;
    }
    if (normalizedPrefix == alias) {
      return true;
    }
  }
  return false;
}

std::string rootedKeyValueHelperAliasPathForMethodTargets(
    std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr || helperName.empty()) {
    return {};
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (alias.empty() || alias.find('/') != std::string_view::npos) {
      continue;
    }
    return "/" + std::string(alias) + "/" + std::string(helperName);
  }
  return {};
}

std::string rootAliasKeyValueHelperNameForMethodTargets(
    std::string_view rawPath,
    std::string_view namespacePrefix) {
  std::string helperName = metadataBackedKeyValueHelperRootAliasMethodName(rawPath);
  if (!helperName.empty()) {
    return helperName;
  }
  if (!isKeyValueHelperImportAliasNamespaceForMethodTargets(namespacePrefix)) {
    return {};
  }
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, rawPath);
  return std::string(memberName);
}

bool isRootedKeyValueHelperAliasPathForMethodTargets(std::string_view rawPath) {
  return !metadataBackedKeyValueHelperRootAliasMethodName(rawPath).empty();
}

bool isCanonicalMapBuiltinMethodHelper(std::string_view helperName) {
  return isStdlibSurfaceMemberName(StdlibSurfaceId::CollectionsManifestSurface2, helperName);
}

std::string experimentalKeyValueBackingLeafForMethodTargets(std::string typeName) {
  typeName = normalizeBindingTypeName(std::move(typeName));
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  const size_t leafStart = typeName.find_last_of('/');
  return leafStart == std::string::npos ? typeName : typeName.substr(leafStart + 1);
}

bool isUnspecializedExperimentalKeyValueBackingTypeForMethodTargets(std::string typeName) {
  typeName = normalizeBindingTypeName(std::move(typeName));
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  return experimentalKeyValueBackingLeafForMethodTargets(typeName) == "Map" &&
         isExperimentalCollectionBackingTypeName("map", "Map", typeName);
}

bool isSpecializedExperimentalKeyValueBackingTypeForMethodTargets(std::string typeName) {
  typeName = normalizeBindingTypeName(std::move(typeName));
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  return experimentalKeyValueBackingLeafForMethodTargets(typeName) != "Map" &&
         isExperimentalCollectionBackingTypeName("map", "Map", typeName);
}

bool isFileMethodName(std::string_view methodName) {
  return methodName == "write" || methodName == "writeLine" ||
         methodName == "write_line" || methodName == "writeByte" ||
         methodName == "write_byte" || methodName == "readByte" ||
         methodName == "read_byte" || methodName == "writeBytes" ||
         methodName == "write_bytes" || methodName == "flush" ||
         methodName == "close";
}

bool isRetiredMaybeMutableHelperName(std::string_view methodName) {
  return methodName == "set" || methodName == "clear" || methodName == "take";
}

bool isMaybeSumTypePath(std::string_view typePath) {
  if (typePath.empty()) {
    return false;
  }
  const size_t leafStart = typePath.find_last_of('/');
  std::string leaf = leafStart == std::string_view::npos
                         ? std::string(typePath)
                         : std::string(typePath.substr(leafStart + 1));
  const size_t generatedSuffix = leaf.find("__");
  if (generatedSuffix != std::string::npos) {
    leaf.erase(generatedSuffix);
  }
  return leaf == "Maybe";
}

}  // namespace method_target_detail
}  // namespace primec::semantics
