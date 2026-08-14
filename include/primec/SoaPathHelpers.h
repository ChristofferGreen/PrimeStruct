// soa-surface-audit: exempt
#pragma once

#include "primec/CompileArena.h"
#include "primec/StdlibCollectionPaths.h"

#include <string>
#include <string_view>

namespace primec::soa_paths {

// TODO-5235: built via systemHeapValue() so these magic statics' backing
// memory is never arena-allocated, regardless of whether any individual
// value happens to fit in std::string's small-string-optimization buffer
// today - see docs/CompilerArenaAllocator.md.
inline std::string publicSoaFolder() {
  static const std::string value = primec::systemHeapValue([] { return std::string("soa"); });
  return value;
}

inline std::string legacySoaFolder() {
  static const std::string value =
      primec::systemHeapValue([] { return std::string("soa") + "_" + "vector"; });
  return value;
}

inline std::string experimentalSoaFolder() {
  static const std::string value = primec::systemHeapValue(
      [] { return std::string("experimental") + "_" + publicSoaFolder() + "_" + "vector"; });
  return value;
}

inline std::string soaBackingTypeName() {
  static const std::string value =
      primec::systemHeapValue([] { return std::string("Soa") + "Vector"; });
  return value;
}

inline std::string collectionPath(std::string_view folder,
                                  std::string_view helperName = {}) {
  // TODO-5243: this is called ~1.16M times for a `mini_vec.prime`-sized
  // program that never touches SoA (6.89% of total instructions in that
  // profile), from every call/method-resolution candidate check across the
  // whole compile, not just SoA-specific ones. The previous body built the
  // "/std/collections/" prefix via a chain of operator+ on separate string
  // literals (each an implicit temporary std::string construction) and then
  // a second concatenation for the optional helper suffix - reserve the
  // final size up front and append directly instead, so this is one
  // allocation instead of several, with byte-identical output.
  static constexpr std::string_view kPrefix = "/std/collections/";
  std::string path;
  path.reserve(kPrefix.size() + folder.size() +
               (helperName.empty() ? 0 : helperName.size() + 1));
  path.append(kPrefix);
  path.append(folder);
  if (!helperName.empty()) {
    path.push_back('/');
    path.append(helperName);
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
  // Only strip a specialization suffix in the leaf segment; a "__t" in an
  // earlier segment belongs to a specialized owner type (e.g. a monomorphized
  // struct whose methods live at /Type__tHASH/method) and must be preserved.
  const size_t templateSuffix = canonicalPath.rfind("__t");
  if (templateSuffix != std::string::npos &&
      canonicalPath.find('/', templateSuffix) == std::string::npos) {
    canonicalPath.erase(templateSuffix);
  }
  return canonicalPath;
}

inline std::string canonicalizeLegacySoaRefHelperPath(std::string_view path) {
  std::string canonicalPath = stripTemplateSpecializationSuffix(path);
  // TODO-5243: same fix as isLegacyOrCanonicalSoaHelperPath() below - these
  // two comparison targets are compile-time constants, precompute once.
  static const std::string refPath = "/" + legacySoaFolder() + "/ref";
  static const std::string refRefPath = "/" + legacySoaFolder() + "/ref_ref";
  if (canonicalPath == refPath) {
    return collectionPath(legacySoaFolder(), "ref");
  }
  if (canonicalPath == refRefPath) {
    return collectionPath(legacySoaFolder(), "ref_ref");
  }
  return canonicalPath;
}

inline std::string canonicalizeLegacySoaGetHelperPath(std::string_view path) {
  const std::string canonicalPath = canonicalizeLegacySoaRefHelperPath(path);
  static const std::string getPath = "/" + legacySoaFolder() + "/get";
  static const std::string getRefPath = "/" + legacySoaFolder() + "/get_ref";
  if (canonicalPath == getPath) {
    return collectionPath(legacySoaFolder(), "get");
  }
  if (canonicalPath == getRefPath) {
    return collectionPath(legacySoaFolder(), "get_ref");
  }
  return canonicalPath;
}

inline bool isLegacyOrCanonicalSoaHelperPath(std::string_view path,
                                             std::string_view helperName) {
  // TODO-5243: these three prefixes are compile-time constants (they never
  // depend on anything per-call or per-compile), but this function is
  // called from ~200 call sites throughout call/method resolution for
  // every candidate path considered - including for compiles that never
  // touch SoA at all, where every call here is doomed to return false.
  // Profiling a program with zero SoA usage (importing only the plain
  // collections vector submodule) showed this rebuilding all three
  // prefix strings via allocation+concatenation on every single call
  // (millions of calls for a trivial program), collectively costing ~11%
  // of total instructions across this function and the sibling
  // soa_paths::* helpers it and isCanonicalStdlibSoaHelperPath call.
  // Building them once as function-local statics (thread-safe magic
  // statics, same pattern already used by legacySoaFolder()/etc. above)
  // turns the hot no-match path into pure string_view comparisons with no
  // allocation, while leaving every branch's actual match semantics
  // byte-for-byte identical.
  static const std::string legacyPrefix = "/" + legacySoaFolder() + "/";
  static const std::string canonicalPrefix = collectionPath(legacySoaFolder()) + "/";
  static const std::string publicPrefix = collectionPath(publicSoaFolder()) + "/";
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
  // TODO-5243: precompute the four (compile-time-constant) comparison
  // targets once instead of rebuilding all four via concatenation on every
  // call - same fix as the sibling helpers above.
  static const std::string legacyRef = collectionPath(legacySoaFolder(), "ref");
  static const std::string legacyRefRef = collectionPath(legacySoaFolder(), "ref_ref");
  static const std::string publicRef = collectionPath(publicSoaFolder(), "ref");
  static const std::string publicRefRef = collectionPath(publicSoaFolder(), "ref_ref");
  return path == legacyRef || path == legacyRefRef || path == publicRef ||
         path == publicRefRef;
}

inline bool isExperimentalSoaRefLikeHelperPath(std::string_view path) {
  const std::string canonicalPath = stripGeneratedSpecializationSuffix(path);
  // TODO-5244: same fix as the soa_paths helpers above (TODO-5243) - these
  // three comparison targets are compile-time constants, precompute once
  // instead of rebuilding them (two collectionPath() concatenations plus a
  // memberPath() concatenation) on every single call.
  static const std::string soaVectorRefPath = collectionPath(experimentalSoaFolder(), "soaVectorRef");
  static const std::string soaVectorRefRefPath = collectionPath(experimentalSoaFolder(), "soaVectorRefRef");
  static const std::string soaColumnRefPath = collection_paths::memberPath(
      collection_paths::kInternalSoaStorageFolder, "soaColumnRef");
  return canonicalPath == soaVectorRefPath || canonicalPath == soaVectorRefRefPath ||
         canonicalPath == soaColumnRefPath;
}

inline bool isExperimentalColumnarVectorSpecializedTypePath(std::string_view path) {
  // TODO-5244: same fix as isLegacyOrCanonicalSoaHelperPath()/etc. above
  // (TODO-5243) - these three prefixes are compile-time constants, but this
  // function is called from the same broad "is this a SoA-related path"
  // classification fan-out as its already-fixed siblings and was rebuilding
  // all three via allocation+concatenation on every call (1.00% of total
  // instructions on its own in a zero-SoA-usage profile). Precompute once.
  static const std::string specializedPrefix =
      collectionPath(publicSoaFolder(), soaBackingTypeName() + "__");
  static const std::string specializedPrefixNoSlash = specializedPrefix.substr(1);
  static const std::string specializedPrefixBare = soaBackingTypeName() + "__";
  return path.starts_with(specializedPrefix) ||
         path.starts_with(specializedPrefixNoSlash) ||
         path.starts_with(specializedPrefixBare);
}

inline bool isExperimentalColumnarVectorTypePath(std::string_view path) {
  // TODO-5244: same fix - typePrefix/typePrefixWithSlash and each
  // matchesTypePrefix() prefix pair are all compile-time constants,
  // precompute once instead of rebuilding on every call.
  static const std::string typePrefix =
      std::string("std") + "/" + "collections" + "/" +
      publicSoaFolder() + "/" + soaBackingTypeName();
  static const std::string typePrefixWithSlash = "/" + typePrefix;
  static const std::string backingTypeTemplatePrefix = soaBackingTypeName() + "<";
  static const std::string backingTypeSpecializedPrefix = soaBackingTypeName() + "__";
  static const std::string typePrefixTemplatePrefix = typePrefix + "<";
  static const std::string typePrefixSpecializedPrefix = typePrefix + "__";
  static const std::string typePrefixWithSlashTemplatePrefix = typePrefixWithSlash + "<";
  static const std::string typePrefixWithSlashSpecializedPrefix = typePrefixWithSlash + "__";
  const bool matchesBackingType = path == soaBackingTypeName() ||
      path.rfind(std::string_view(backingTypeTemplatePrefix), 0) == 0 ||
      path.rfind(std::string_view(backingTypeSpecializedPrefix), 0) == 0;
  const bool matchesTypePrefix = path == typePrefix ||
      path.rfind(std::string_view(typePrefixTemplatePrefix), 0) == 0 ||
      path.rfind(std::string_view(typePrefixSpecializedPrefix), 0) == 0;
  const bool matchesTypePrefixWithSlash = path == typePrefixWithSlash ||
      path.rfind(std::string_view(typePrefixWithSlashTemplatePrefix), 0) == 0 ||
      path.rfind(std::string_view(typePrefixWithSlashSpecializedPrefix), 0) == 0;
  return matchesBackingType || matchesTypePrefix || matchesTypePrefixWithSlash ||
         isExperimentalColumnarVectorSpecializedTypePath(path);
}

} // namespace primec::soa_paths
