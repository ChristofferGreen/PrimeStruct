// soa-surface-audit: exempt
// Canonical owner of every /std/collections module spelling used by the
// compiler. All production code must route collection module roots, member
// paths, and mangled-type prefixes through these helpers instead of repeating
// path literals (TODO-4624 through TODO-4627); later collection renames then
// become single-constant flips (TODO-4628 through TODO-4634).
#pragma once

#include <string>
#include <string_view>

namespace primec::collection_paths {

// --- Module roots -----------------------------------------------------------

inline constexpr std::string_view kCollectionsRoot = "/std/collections";
inline constexpr std::string_view kCollectionsRootBare = "std/collections";

// --- Canonical public module folders ---------------------------------------

inline constexpr std::string_view kVectorFolder = "vector";
inline constexpr std::string_view kMapFolder = "map";
inline constexpr std::string_view kSoaFolder = "soa";
inline constexpr std::string_view kEqualityFolder = "equality";
inline constexpr std::string_view kErrorsFolder = "errors";

// --- Compatibility shim folders (retirement: TODO-4628..TODO-4630) ---------

inline constexpr std::string_view kExperimentalFolderPrefix = "experimental_";
inline constexpr std::string_view kExperimentalVectorFolder = "experimental_vector";
inline constexpr std::string_view kExperimentalMapFolder = "experimental_map";
inline constexpr std::string_view kExperimentalSoaVectorFolder = "experimental_soa_vector";
inline constexpr std::string_view kExperimentalSoaVectorConversionsFolder =
    "experimental_soa_vector_conversions";
inline constexpr std::string_view kLegacySoaVectorFolder = "soa_vector";
inline constexpr std::string_view kLegacySoaVectorConversionsFolder =
    "soa_vector_conversions";

// --- Internal implementation folders (collapse: TODO-4631..TODO-4634) ------

inline constexpr std::string_view kInternalVectorFolder = "internal_vector";
inline constexpr std::string_view kInternalMapFolder = "internal_map";
inline constexpr std::string_view kInternalSoaVectorFolder = "internal_soa_vector";
inline constexpr std::string_view kInternalSoaVectorConversionsFolder =
    "internal_soa_vector_conversions";
inline constexpr std::string_view kInternalSoaStorageFolder = "internal_soa_storage";
inline constexpr std::string_view kInternalBufferCheckedFolder = "internal_buffer_checked";
inline constexpr std::string_view kInternalBufferUncheckedFolder = "internal_buffer_unchecked";

// --- Backing type names ------------------------------------------------------

inline constexpr std::string_view kVectorTypeName = "Vector";
inline constexpr std::string_view kSoaVectorTypeName = "SoaVector";
inline constexpr std::string_view kSoaColumnTypeName = "SoaColumn";
inline constexpr std::string_view kMapTypeName = "Map";
inline constexpr std::string_view kMapValueTypeName = "MapValue";
inline constexpr std::string_view kEntryTypeName = "Entry";

// --- Monomorphization suffixes ----------------------------------------------

inline constexpr std::string_view kSpecializationSuffix = "__";
inline constexpr std::string_view kTemplateSpecializationSuffix = "__t";

// --- Path builders ------------------------------------------------------------

// "/std/collections/<folder>"
inline std::string moduleRoot(std::string_view folder) {
  return std::string(kCollectionsRoot) + "/" + std::string(folder);
}

// "std/collections/<folder>"
inline std::string moduleRootBare(std::string_view folder) {
  return std::string(kCollectionsRootBare) + "/" + std::string(folder);
}

// "/std/collections/<folder>/"
inline std::string modulePrefix(std::string_view folder) {
  return moduleRoot(folder) + "/";
}

// "std/collections/<folder>/"
inline std::string modulePrefixBare(std::string_view folder) {
  return moduleRootBare(folder) + "/";
}

// "/std/collections/<folder>/<member>"
inline std::string memberPath(std::string_view folder, std::string_view member) {
  return modulePrefix(folder) + std::string(member);
}

// "std/collections/<folder>/<member>"
inline std::string memberPathBare(std::string_view folder, std::string_view member) {
  return modulePrefixBare(folder) + std::string(member);
}

// "experimental_<collectionName>"
inline std::string experimentalFolder(std::string_view collectionName) {
  return std::string(kExperimentalFolderPrefix) + std::string(collectionName);
}

// Folder that owns a collection's canonical type identity. The Vector
// identity lives in the canonical vector module (TODO-4628) and the
// SoaVector identity lives in the canonical soa module (TODO-4629); the Map
// identity still lives in its experimental_* module until TODO-4630/4632.
inline std::string typeIdentityFolder(std::string_view collectionName) {
  if (collectionName == kVectorFolder) {
    return std::string(kVectorFolder);
  }
  if (collectionName == kLegacySoaVectorFolder || collectionName == kSoaFolder) {
    return std::string(kSoaFolder);
  }
  return experimentalFolder(collectionName);
}

// "/std/collections/<folder>/<TypeName>__"
inline std::string specializedTypePrefix(std::string_view folder,
                                         std::string_view typeName) {
  return memberPath(folder, typeName) + std::string(kSpecializationSuffix);
}

// "std/collections/<folder>/<TypeName>__"
inline std::string specializedTypePrefixBare(std::string_view folder,
                                             std::string_view typeName) {
  return memberPathBare(folder, typeName) + std::string(kSpecializationSuffix);
}

// "/std/collections/<folder>/<TypeName>__t"
inline std::string templateSpecializedTypePrefix(std::string_view folder,
                                                 std::string_view typeName) {
  return memberPath(folder, typeName) + std::string(kTemplateSpecializationSuffix);
}

// True when path is "<base>" or a "<base>__..." monomorphized specialization.
inline bool isTypePathOrSpecialization(std::string_view path, std::string_view basePath) {
  if (path == basePath) {
    return true;
  }
  const std::string specializedPrefix =
      std::string(basePath) + std::string(kSpecializationSuffix);
  return path.rfind(specializedPrefix, 0) == 0;
}

} // namespace primec::collection_paths
