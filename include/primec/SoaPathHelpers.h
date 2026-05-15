#pragma once

#include <string>
#include <string_view>

namespace primec::soa_paths {

inline std::string publicSoaFolder() {
  return std::string("soa");
}

inline std::string legacySoaFolder() {
  return std::string("soa") + "_" + "vector";
}

inline std::string experimentalSoaFolder() {
  return std::string("experimental") + "_" + publicSoaFolder() + "_" + "vector";
}

inline std::string soaBackingTypeName() {
  return std::string("Soa") + "Vector";
}

inline std::string collectionPath(std::string_view folder,
                                  std::string_view helperName = {}) {
  std::string path = "/" + std::string("std") + "/" + "collections" + "/" +
                     std::string(folder);
  if (!helperName.empty()) {
    path += "/" + std::string(helperName);
  }
  return path;
}

inline std::string stripGeneratedSpecializationSuffix(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath;
}

inline std::string stripTemplateSpecializationSuffix(std::string_view path) {
  std::string canonicalPath(path);
  const size_t templateSuffix = canonicalPath.find("__t");
  if (templateSuffix != std::string::npos) {
    canonicalPath.erase(templateSuffix);
  }
  return canonicalPath;
}

inline std::string canonicalizeLegacySoaRefHelperPath(std::string_view path) {
  std::string canonicalPath = stripTemplateSpecializationSuffix(path);
  if (canonicalPath == "/" + legacySoaFolder() + "/ref") {
    return collectionPath(legacySoaFolder(), "ref");
  }
  if (canonicalPath == "/" + legacySoaFolder() + "/ref_ref") {
    return collectionPath(legacySoaFolder(), "ref_ref");
  }
  return canonicalPath;
}

inline std::string canonicalizeLegacySoaGetHelperPath(std::string_view path) {
  const std::string canonicalPath = canonicalizeLegacySoaRefHelperPath(path);
  if (canonicalPath == "/" + legacySoaFolder() + "/get") {
    return collectionPath(legacySoaFolder(), "get");
  }
  if (canonicalPath == "/" + legacySoaFolder() + "/get_ref") {
    return collectionPath(legacySoaFolder(), "get_ref");
  }
  return canonicalPath;
}

inline bool isLegacyOrCanonicalSoaHelperPath(std::string_view path,
                                             std::string_view helperName) {
  const std::string legacyPrefix = "/" + legacySoaFolder() + "/";
  const std::string canonicalPrefix = collectionPath(legacySoaFolder()) + "/";
  const std::string publicPrefix = collectionPath(publicSoaFolder()) + "/";
  if (path.starts_with(legacyPrefix)) {
    return path.substr(legacyPrefix.size()) == helperName;
  }
  if (path.starts_with(canonicalPrefix)) {
    return path.substr(canonicalPrefix.size()) == helperName;
  }
  if (path.starts_with(publicPrefix)) {
    return path.substr(publicPrefix.size()) == helperName;
  }
  return false;
}

inline bool isCanonicalSoaRefLikeHelperPath(std::string_view path) {
  return path == collectionPath(legacySoaFolder(), "ref") ||
         path == collectionPath(legacySoaFolder(), "ref_ref") ||
         path == collectionPath(publicSoaFolder(), "ref") ||
         path == collectionPath(publicSoaFolder(), "ref_ref");
}

inline bool isExperimentalSoaRefLikeHelperPath(std::string_view path) {
  const std::string canonicalPath = stripGeneratedSpecializationSuffix(path);
  return canonicalPath == collectionPath(experimentalSoaFolder(), "soa" "VectorRef") ||
         canonicalPath == collectionPath(experimentalSoaFolder(), "soa" "VectorRefRef") ||
         canonicalPath == "/std/collections/internal_soa_storage/soaColumnRef";
}

inline bool isExperimentalColumnarVectorSpecializedTypePath(std::string_view path) {
  const std::string specializedPrefix =
      collectionPath(experimentalSoaFolder(), soaBackingTypeName() + "__");
  const std::string specializedPrefixNoSlash = specializedPrefix.substr(1);
  const std::string specializedPrefixBare = soaBackingTypeName() + "__";
  return path.starts_with(specializedPrefix) ||
         path.starts_with(specializedPrefixNoSlash) ||
         path.starts_with(specializedPrefixBare);
}

inline bool isExperimentalColumnarVectorTypePath(std::string_view path) {
  const std::string typePrefix =
      std::string("std") + "/" + "collections" + "/" +
      experimentalSoaFolder() + "/" + soaBackingTypeName();
  const std::string typePrefixWithSlash = "/" + typePrefix;
  const auto matchesTypePrefix = [&](std::string_view prefix) {
    const std::string templatePrefix = std::string(prefix) + "<";
    const std::string specializedPrefix = std::string(prefix) + "__";
    return path == prefix ||
           path.rfind(std::string_view(templatePrefix), 0) == 0 ||
           path.rfind(std::string_view(specializedPrefix), 0) == 0;
  };
  return matchesTypePrefix(soaBackingTypeName()) || matchesTypePrefix(typePrefix) ||
         matchesTypePrefix(typePrefixWithSlash) ||
         isExperimentalColumnarVectorSpecializedTypePath(path);
}

} // namespace primec::soa_paths
