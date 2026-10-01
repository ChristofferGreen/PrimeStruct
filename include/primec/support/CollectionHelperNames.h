// soa-surface-audit: exempt
// collection-surface-audit: exempt
// vector-surface-audit: exempt
// map-surface-audit: exempt
// Canonical owner of every collection helper spelling the compiler matches on
// (TODO-5350): the borrowed `_ref` helper names, the rooted same-namespace paths
// (`/vector/...`, `/soa/...`, `/map/...`, `/array/...`, `/string/...`), and the
// canonical `/std/collections/...` member paths. Production code must use these
// constants instead of repeating the literals, so a rename is a one-constant
// flip; `scripts/check_collection_helper_literals.py` (ctest) fails when a
// literal reappears outside this header, the stdlib surface registry, and the
// compat-spelling classifier.
//
// The constants are `constexpr char[]`, exactly like the string literals they
// replace, so they work wherever a literal does (std::string, string_view,
// const char *, concatenation).
#pragma once

#include <string_view>

namespace primec::collection_helpers {

// --- Borrowed helper names (receiver passed as Reference<...>) ---
inline constexpr char kAtRef[] = "at_ref";
inline constexpr char kAtUnsafeRef[] = "at_unsafe_ref";
inline constexpr char kContainsRef[] = "contains_ref";
inline constexpr char kCountRef[] = "count_ref";
inline constexpr char kGetRef[] = "get_ref";
inline constexpr char kInsertRef[] = "insert_ref";
inline constexpr char kRefRef[] = "ref_ref";
inline constexpr char kToAosRef[] = "to_aos_ref";
inline constexpr char kTryAtRef[] = "tryAt_ref";

// --- Rooted same-namespace paths and prefixes ---
inline constexpr char kRootedArray[] = "/array";
inline constexpr char kRootedArrayPrefix[] = "/array/";
inline constexpr char kRootedArrayAt[] = "/array/at";
inline constexpr char kRootedArrayAtUnsafe[] = "/array/at_unsafe";
inline constexpr char kRootedArrayCapacity[] = "/array/capacity";
inline constexpr char kRootedArrayCount[] = "/array/count";
inline constexpr char kRootedMap[] = "/map";
inline constexpr char kRootedMapPrefix[] = "/map/";
inline constexpr char kRootedMapSpecialized[] = "/map__";
inline constexpr char kRootedMapOverloadSpecialized[] = "/map__ov";
inline constexpr char kRootedMapTemplateSpecialized[] = "/map__t";
inline constexpr char kRootedSoa[] = "/soa";
inline constexpr char kRootedSoaPrefix[] = "/soa/";
inline constexpr char kRootedSoaCount[] = "/soa/count";
inline constexpr char kRootedSoaCountRef[] = "/soa/count_ref";
inline constexpr char kRootedSoaGet[] = "/soa/get";
inline constexpr char kRootedSoaGetRef[] = "/soa/get_ref";
inline constexpr char kRootedSoaPush[] = "/soa/push";
inline constexpr char kRootedSoaRef[] = "/soa/ref";
inline constexpr char kRootedSoaRefRef[] = "/soa/ref_ref";
inline constexpr char kRootedSoaReserve[] = "/soa/reserve";
inline constexpr char kRootedString[] = "/string";
inline constexpr char kRootedStringPrefix[] = "/string/";
inline constexpr char kRootedStringAt[] = "/string/at";
inline constexpr char kRootedStringAtUnsafe[] = "/string/at_unsafe";
inline constexpr char kRootedStringCount[] = "/string/count";
inline constexpr char kRootedVector[] = "/vector";
inline constexpr char kRootedVectorPrefix[] = "/vector/";
inline constexpr char kRootedVectorCapacity[] = "/vector/capacity";
inline constexpr char kRootedVectorCount[] = "/vector/count";
inline constexpr char kRootedVectorRemoveAt[] = "/vector/remove_at";

// --- Canonical /std/collections paths ---
inline constexpr char kCanonicalContainerErrorType[] = "/std/collections/ContainerError";
inline constexpr char kCanonicalContainerErrorTypePrefix[] = "/std/collections/ContainerError/";
inline constexpr char kCanonicalContainerErrorTypeCapacityExceeded[] = "/std/collections/ContainerError/capacity_exceeded";
inline constexpr char kCanonicalContainerErrorTypeEmpty[] = "/std/collections/ContainerError/empty";
inline constexpr char kCanonicalContainerErrorTypeIndexOutOfBounds[] = "/std/collections/ContainerError/index_out_of_bounds";
inline constexpr char kCanonicalContainerErrorTypeMissingKey[] = "/std/collections/ContainerError/missing_key";
inline constexpr char kCanonicalContainerErrorTypeResult[] = "/std/collections/ContainerError/result";
inline constexpr char kCanonicalContainerErrorTypeStatus[] = "/std/collections/ContainerError/status";
inline constexpr char kCanonicalContainerErrorTypeWhy[] = "/std/collections/ContainerError/why";
inline constexpr char kCanonicalMapType[] = "/std/collections/Map";
inline constexpr char kCanonicalContainerCapacityExceeded[] = "/std/collections/containerCapacityExceeded";
inline constexpr char kCanonicalContainerEmpty[] = "/std/collections/containerEmpty";
inline constexpr char kCanonicalContainerErrorResult[] = "/std/collections/containerErrorResult";
inline constexpr char kCanonicalContainerErrorStatus[] = "/std/collections/containerErrorStatus";
inline constexpr char kCanonicalContainerIndexOutOfBounds[] = "/std/collections/containerIndexOutOfBounds";
inline constexpr char kCanonicalContainerMissingKey[] = "/std/collections/containerMissingKey";
inline constexpr char kCanonicalExperimentalSoa[] = "/std/collections/experimental_soa";
inline constexpr char kCanonicalExperimentalSoaConversions[] = "/std/collections/experimental_soa_conversions";
inline constexpr char kCanonicalExperimentalSoaConversionsSoaVectorToAos[] = "/std/collections/experimental_soa_conversions/soaVectorToAos";
inline constexpr char kCanonicalExperimentalSoaConversionsSoaVectorToAosRef[] = "/std/collections/experimental_soa_conversions/soaVectorToAosRef";
inline constexpr char kCanonicalInternalSoa[] = "/std/collections/internal_soa";
inline constexpr char kCanonicalInternalSoaConversions[] = "/std/collections/internal_soa_conversions";
inline constexpr char kCanonicalMap[] = "/std/collections/map";
inline constexpr char kCanonicalMapPrefix[] = "/std/collections/map/";
inline constexpr char kCanonicalMapMapType[] = "/std/collections/map/Map";
inline constexpr char kCanonicalMapMapValueType[] = "/std/collections/map/MapValue";
inline constexpr char kCanonicalMapAt[] = "/std/collections/map/at";
inline constexpr char kCanonicalMapAtUnsafe[] = "/std/collections/map/at_unsafe";
inline constexpr char kCanonicalMapContains[] = "/std/collections/map/contains";
inline constexpr char kCanonicalMapMap[] = "/std/collections/map/map";
inline constexpr char kCanonicalMapTryAt[] = "/std/collections/map/tryAt";
inline constexpr char kCanonicalMapPair[] = "/std/collections/mapPair";
inline constexpr char kCanonicalSoa[] = "/std/collections/soa";
inline constexpr char kCanonicalSoaPrefix[] = "/std/collections/soa/";
inline constexpr char kCanonicalSoaCount[] = "/std/collections/soa/count";
inline constexpr char kCanonicalSoaCountSpecialized[] = "/std/collections/soa/count__";
inline constexpr char kCanonicalSoaCountRef[] = "/std/collections/soa/count_ref";
inline constexpr char kCanonicalSoaFieldView[] = "/std/collections/soa/field_view";
inline constexpr char kCanonicalSoaFromAos[] = "/std/collections/soa/from_aos";
inline constexpr char kCanonicalSoaGet[] = "/std/collections/soa/get";
inline constexpr char kCanonicalSoaGetRef[] = "/std/collections/soa/get_ref";
inline constexpr char kCanonicalSoaPush[] = "/std/collections/soa/push";
inline constexpr char kCanonicalSoaRef[] = "/std/collections/soa/ref";
inline constexpr char kCanonicalSoaRefRef[] = "/std/collections/soa/ref_ref";
inline constexpr char kCanonicalSoaReserve[] = "/std/collections/soa/reserve";
inline constexpr char kCanonicalSoaSingle[] = "/std/collections/soa/single";
inline constexpr char kCanonicalSoaSoa[] = "/std/collections/soa/soa";
inline constexpr char kCanonicalSoaSoaVectorFieldView[] = "/std/collections/soa/soaVectorFieldView";
inline constexpr char kCanonicalSoaSoaSpecialized[] = "/std/collections/soa/soa__";
inline constexpr char kCanonicalSoaToAos[] = "/std/collections/soa/to_aos";
inline constexpr char kCanonicalSoaToAosRef[] = "/std/collections/soa/to_aos_ref";
inline constexpr char kCanonicalSoaStorageSoaColumnCount[] = "/std/collections/soa_storage/soaColumnCount";
inline constexpr char kCanonicalSoaStorageSoaColumnCountSpecialized[] = "/std/collections/soa_storage/soaColumnCount__";
inline constexpr char kCanonicalSoaVectorPrefix[] = "/std/collections/soa_vector/";
inline constexpr char kCanonicalVector[] = "/std/collections/vector";
inline constexpr char kCanonicalVectorPrefix[] = "/std/collections/vector/";
inline constexpr char kCanonicalVectorVectorType[] = "/std/collections/vector/Vector";
inline constexpr char kCanonicalVectorAt[] = "/std/collections/vector/at";
inline constexpr char kCanonicalVectorAtUnsafe[] = "/std/collections/vector/at_unsafe";
inline constexpr char kCanonicalVectorCapacity[] = "/std/collections/vector/capacity";
inline constexpr char kCanonicalVectorCount[] = "/std/collections/vector/count";
inline constexpr char kCanonicalVectorRemoveAt[] = "/std/collections/vector/remove_at";
inline constexpr char kCanonicalVectorVector[] = "/std/collections/vector/vector";


// --- Base-or-borrowed helper name predicates --------------------------------
// `name` is the helper (`count`) or its borrowed `_ref` variant (`count_ref`).

constexpr bool isCountHelperName(std::string_view name) { return name == "count" || name == kCountRef; }
constexpr bool isGetHelperName(std::string_view name) { return name == "get" || name == kGetRef; }
constexpr bool isRefHelperName(std::string_view name) { return name == "ref" || name == kRefRef; }
constexpr bool isAtHelperName(std::string_view name) { return name == "at" || name == kAtRef; }
constexpr bool isAtUnsafeHelperName(std::string_view name) { return name == "at_unsafe" || name == kAtUnsafeRef; }
constexpr bool isToAosHelperName(std::string_view name) { return name == "to_aos" || name == kToAosRef; }
constexpr bool isTryAtHelperName(std::string_view name) { return name == "tryAt" || name == kTryAtRef; }
constexpr bool isContainsHelperName(std::string_view name) { return name == "contains" || name == kContainsRef; }
constexpr bool isInsertHelperName(std::string_view name) { return name == "insert" || name == kInsertRef; }

// --- Rooted namespace path predicates ----------------------------------------
// `path` lives under the rooted same-namespace folder (`/array/...`, `/soa/...`,
// `/string/...`); the folder itself (`/array`) does not count.

constexpr bool isRootedArrayPath(std::string_view path) { return path.starts_with(kRootedArrayPrefix); }
constexpr bool isRootedSoaPath(std::string_view path) { return path.starts_with(kRootedSoaPrefix); }
constexpr bool isRootedStringPath(std::string_view path) { return path.starts_with(kRootedStringPrefix); }

// True for any borrowed `_ref` helper name.
constexpr bool isBorrowedHelperName(std::string_view name) {
  return name == kCountRef || name == kGetRef || name == kRefRef || name == kAtRef || name == kAtUnsafeRef ||
         name == kToAosRef || name == kTryAtRef || name == kContainsRef || name == kInsertRef;
}

} // namespace primec::collection_helpers
