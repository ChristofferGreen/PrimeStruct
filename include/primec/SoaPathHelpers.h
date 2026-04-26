#pragma once

#include <string>
#include <string_view>

namespace primec::soa_paths {

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
  if (canonicalPath == "/soa_vector/ref") {
    return "/std/collections/soa_vector/ref";
  }
  if (canonicalPath == "/soa_vector/ref_ref") {
    return "/std/collections/soa_vector/ref_ref";
  }
  return canonicalPath;
}

inline std::string canonicalizeLegacySoaGetHelperPath(std::string_view path) {
  const std::string canonicalPath = canonicalizeLegacySoaRefHelperPath(path);
  if (canonicalPath == "/soa_vector/get") {
    return "/std/collections/soa_vector/get";
  }
  if (canonicalPath == "/soa_vector/get_ref") {
    return "/std/collections/soa_vector/get_ref";
  }
  return canonicalPath;
}

inline bool isLegacyOrCanonicalSoaHelperPath(std::string_view path,
                                             std::string_view helperName) {
  constexpr std::string_view LegacyPrefix = "/soa_vector/";
  constexpr std::string_view CanonicalPrefix = "/std/collections/soa_vector/";
  if (path.starts_with(LegacyPrefix)) {
    return path.substr(LegacyPrefix.size()) == helperName;
  }
  if (path.starts_with(CanonicalPrefix)) {
    return path.substr(CanonicalPrefix.size()) == helperName;
  }
  return false;
}

inline bool isCanonicalSoaRefLikeHelperPath(std::string_view path) {
  return path == "/std/collections/soa_vector/ref" ||
         path == "/std/collections/soa_vector/ref_ref";
}

inline bool isExperimentalSoaRefLikeHelperPath(std::string_view path) {
  const std::string canonicalPath = stripGeneratedSpecializationSuffix(path);
  return canonicalPath == "/std/collections/experimental_soa_vector/soaVectorRef" ||
         canonicalPath == "/std/collections/experimental_soa_vector/soaVectorRefRef" ||
         canonicalPath == "/std/collections/internal_soa_storage/soaColumnRef";
}

inline bool isExperimentalSoaVectorSpecializedTypePath(std::string_view path) {
  constexpr std::string_view SpecializedPrefix =
      "/std/collections/experimental_soa_vector/SoaVector__";
  constexpr std::string_view SpecializedPrefixNoSlash =
      "std/collections/experimental_soa_vector/SoaVector__";
  constexpr std::string_view SpecializedPrefixBare = "SoaVector__";
  return path.starts_with(SpecializedPrefix) ||
         path.starts_with(SpecializedPrefixNoSlash) ||
         path.starts_with(SpecializedPrefixBare);
}

inline bool isExperimentalSoaVectorTypePath(std::string_view path) {
  constexpr std::string_view TypePrefix =
      "std/collections/experimental_soa_vector/SoaVector";
  constexpr std::string_view TypePrefixWithSlash =
      "/std/collections/experimental_soa_vector/SoaVector";
  const auto matchesTypePrefix = [&](std::string_view prefix) {
    const std::string templatePrefix = std::string(prefix) + "<";
    const std::string specializedPrefix = std::string(prefix) + "__";
    return path == prefix ||
           path.rfind(std::string_view(templatePrefix), 0) == 0 ||
           path.rfind(std::string_view(specializedPrefix), 0) == 0;
  };
  return matchesTypePrefix("SoaVector") || matchesTypePrefix(TypePrefix) ||
         matchesTypePrefix(TypePrefixWithSlash) ||
         isExperimentalSoaVectorSpecializedTypePath(path);
}

} // namespace primec::soa_paths
