#include "primec/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace primec {
namespace {

constexpr auto FileHelperMembers = std::to_array<std::string_view>({
    "openRead",
    "openWrite",
    "openAppend",
    "open_read",
    "open_write",
    "open_append",
    "readByte",
    "read_byte",
    "write",
    "writeLine",
    "write_line",
    "writeByte",
    "write_byte",
    "writeBytes",
    "write_bytes",
    "flush",
    "close",
});

constexpr auto FileHelperImportAliases = std::to_array<std::string_view>({
    "/std/file/File",
    "/File",
    "File",
});

constexpr auto FileHelperCompatibilitySpellings = std::to_array<std::string_view>({
    "/file/read_byte",
    "/file/write_byte",
    "/file/write_bytes",
    "/file/flush",
});

constexpr auto FileHelperLoweringSpellings = std::to_array<std::string_view>({
    "/std/file/File/open_read",
    "/std/file/File/open_write",
    "/std/file/File/open_append",
    "/std/file/File/write",
    "/std/file/File/write_line",
    "/file/write",
    "/file/write_line",
    "/file/read_byte",
    "/file/write_byte",
    "/file/write_bytes",
    "/file/flush",
    "/file/close",
});

constexpr auto FileErrorHelperMembers = std::to_array<std::string_view>({
    "why",
    "status",
    "eof",
    "isEof",
    "is_eof",
    "result",
});

constexpr auto FileErrorImportAliases = std::to_array<std::string_view>({
    "/std/file/FileError",
    "/FileError",
    "FileError",
});

constexpr auto FileErrorCompatibilitySpellings = std::to_array<std::string_view>({
    "/std/file/fileReadEof",
    "/std/file/fileErrorIsEof",
    "/std/file/fileErrorStatus",
    "/std/file/fileErrorResult",
});

constexpr auto FileErrorLoweringSpellings = std::to_array<std::string_view>({
    "/std/file/FileError/why",
    "/std/file/FileError/status",
    "/std/file/FileError/eof",
    "/std/file/FileError/is_eof",
    "/std/file/FileError/result",
    "/file_error/why",
});

constexpr auto CollectionsVectorHelperMembers = std::to_array<std::string_view>({
    "vector",
    "count",
    "capacity",
    "push",
    "pop",
    "reserve",
    "clear",
    "remove_at",
    "remove_swap",
    "at",
    "at_unsafe",
});

constexpr auto CollectionsVectorImportAliases = std::to_array<std::string_view>({
    "/std/collections/vector",
    "vector",
});

constexpr auto CollectionsVectorCompatibilitySpellings = std::to_array<std::string_view>({
    "/std/collections/experimental_vector/vector",
    "/std/collections/experimental_vector/vectorPair",
    "/std/collections/experimental_vector/vectorCount",
    "/std/collections/experimental_vector/vectorCapacity",
    "/std/collections/experimental_vector/vectorPush",
    "/std/collections/experimental_vector/vectorPop",
    "/std/collections/experimental_vector/vectorReserve",
    "/std/collections/experimental_vector/vectorClear",
    "/std/collections/experimental_vector/vectorRemoveAt",
    "/std/collections/experimental_vector/vectorRemoveSwap",
    "/std/collections/experimental_vector/vectorAt",
    "/std/collections/experimental_vector/vectorAtUnsafe",
});

constexpr auto CollectionsVectorLoweringSpellings = std::to_array<std::string_view>({
    "/std/collections/vector/count",
    "/std/collections/vector/capacity",
    "/std/collections/vector/push",
    "/std/collections/vector/pop",
    "/std/collections/vector/reserve",
    "/std/collections/vector/clear",
    "/std/collections/vector/remove_at",
    "/std/collections/vector/remove_swap",
    "/std/collections/vector/at",
    "/std/collections/vector/at_unsafe",
});

constexpr auto CollectionsVectorStatementHelperMembers =
    std::to_array<std::string_view>({
        "push",
        "pop",
        "reserve",
        "clear",
        "remove_at",
        "remove_swap",
    });

constexpr auto CollectionsVectorConstructorMembers = std::to_array<std::string_view>({
    "vector",
    "vectorNew",
    "vectorSingle",
    "vectorPair",
    "vectorTriple",
    "vectorQuad",
    "vectorQuint",
    "vectorSext",
    "vectorSept",
    "vectorOct",
});

constexpr auto CollectionsVectorConstructorImportAliases = std::to_array<std::string_view>({
    "/std/collections/vector",
    "vector",
});

constexpr auto CollectionsVectorConstructorCompatibilitySpellings = std::to_array<std::string_view>({
    "/std/collections/experimental_vector/vector",
    "/std/collections/experimental_vector/vectorNew",
    "/std/collections/experimental_vector/vectorSingle",
    "/std/collections/experimental_vector/vectorPair",
    "/std/collections/experimental_vector/vectorTriple",
    "/std/collections/experimental_vector/vectorQuad",
    "/std/collections/experimental_vector/vectorQuint",
    "/std/collections/experimental_vector/vectorSext",
    "/std/collections/experimental_vector/vectorSept",
    "/std/collections/experimental_vector/vectorOct",
});

constexpr auto CollectionsVectorConstructorLoweringSpellings = std::to_array<std::string_view>({
    "/std/collections/vector/vector",
    "/std/collections/vectorNew",
    "/std/collections/vectorSingle",
    "/std/collections/vectorPair",
    "/std/collections/vectorTriple",
    "/std/collections/vectorQuad",
    "/std/collections/vectorQuint",
    "/std/collections/vectorSext",
    "/std/collections/vectorSept",
    "/std/collections/vectorOct",
});

constexpr auto CollectionsMapHelperMembers = std::to_array<std::string_view>({
    "entry",
    "map",
    "count",
    "count_ref",
    "contains",
    "contains_ref",
    "tryAt",
    "tryAt_ref",
    "at",
    "at_ref",
    "at_unsafe",
    "at_unsafe_ref",
    "insert",
    "insert_ref",
});

constexpr auto CollectionsMapImportAliases = std::to_array<std::string_view>({
    "/std/collections/map",
    "/map",
    "map",
});

constexpr auto CollectionsMapCompatibilitySpellings = std::to_array<std::string_view>({
    "/map/count",
    "/map/count_ref",
    "/map/contains",
    "/map/contains_ref",
    "/map/tryAt",
    "/map/tryAt_ref",
    "/map/at",
    "/map/at_ref",
    "/map/at_unsafe",
    "/map/at_unsafe_ref",
    "/map/insert",
    "/map/insert_ref",
});

constexpr auto CollectionsMapLoweringSpellings = std::to_array<std::string_view>({
    "/std/collections/map/count",
    "/std/collections/map/count_ref",
    "/std/collections/map/contains",
    "/std/collections/map/contains_ref",
    "/std/collections/map/tryAt",
    "/std/collections/map/tryAt_ref",
    "/std/collections/map/at",
    "/std/collections/map/at_ref",
    "/std/collections/map/at_unsafe",
    "/std/collections/map/at_unsafe_ref",
    "/std/collections/map/insert",
    "/std/collections/map/insert_ref",
    "/std/collections/mapCount",
    "/std/collections/experimental_map/mapCount",
    "/std/collections/experimental_map/mapCountRef",
    "/std/collections/mapContains",
    "/std/collections/experimental_map/mapContains",
    "/std/collections/experimental_map/mapContainsRef",
    "/std/collections/mapTryAt",
    "/std/collections/experimental_map/mapTryAt",
    "/std/collections/experimental_map/mapTryAtRef",
    "/std/collections/mapAt",
    "/std/collections/experimental_map/mapAt",
    "/std/collections/experimental_map/mapAtRef",
    "/std/collections/mapAtUnsafe",
    "/std/collections/experimental_map/mapAtUnsafe",
    "/std/collections/experimental_map/mapAtUnsafeRef",
    "/std/collections/mapInsert",
    "/std/collections/experimental_map/mapInsert",
    "/std/collections/experimental_map/mapInsertRef",
});

constexpr auto CollectionsMapBaseHelperMembers = std::to_array<std::string_view>({
    "count",
    "contains",
    "tryAt",
    "at",
    "at_unsafe",
    "insert",
});

constexpr auto CollectionsMapBorrowedHelperMembers = std::to_array<std::string_view>({
    "count_ref",
    "contains_ref",
    "tryAt_ref",
    "at_ref",
    "at_unsafe_ref",
    "insert_ref",
});

constexpr auto CollectionsMapConstructorMembers = std::to_array<std::string_view>({
    "map",
    "mapNew",
    "mapSingle",
    "mapDouble",
    "mapPair",
    "mapTriple",
    "mapQuad",
    "mapQuint",
    "mapSext",
    "mapSept",
    "mapOct",
});

constexpr auto CollectionsMapConstructorImportAliases = std::to_array<std::string_view>({
    "/std/collections/map",
    "/map",
    "map",
});

constexpr auto CollectionsMapConstructorCompatibilitySpellings = std::to_array<std::string_view>({
    "/std/collections/experimental_map/map",
    "/std/collections/experimental_map/mapNew",
    "/std/collections/experimental_map/mapSingle",
    "/std/collections/experimental_map/mapDouble",
    "/std/collections/experimental_map/mapPair",
    "/std/collections/experimental_map/mapTriple",
    "/std/collections/experimental_map/mapQuad",
    "/std/collections/experimental_map/mapQuint",
    "/std/collections/experimental_map/mapSext",
    "/std/collections/experimental_map/mapSept",
    "/std/collections/experimental_map/mapOct",
});

constexpr auto CollectionsMapConstructorLoweringSpellings = std::to_array<std::string_view>({
    "/std/collections/map/map",
    "/std/collections/mapNew",
    "/std/collections/mapSingle",
    "/std/collections/mapDouble",
    "/std/collections/mapPair",
    "/std/collections/mapTriple",
    "/std/collections/mapQuad",
    "/std/collections/mapQuint",
    "/std/collections/mapSext",
    "/std/collections/mapSept",
    "/std/collections/mapOct",
});

constexpr auto CollectionsSoaVectorHelperMembers = std::to_array<std::string_view>({
    "count",
    "count_ref",
    "get",
    "get_ref",
    "ref",
    "ref_ref",
    "reserve",
    "push",
    "to_aos",
    "to_aos_ref",
});

constexpr auto CollectionsSoaVectorImportAliases = std::to_array<std::string_view>({
    "/std/collections/soa_vector",
    "/soa_vector",
    "soa_vector",
});

constexpr auto CollectionsSoaVectorCompatibilitySpellings = std::to_array<std::string_view>({
    "/soa_vector/count",
    "/soa_vector/count_ref",
    "/soa_vector/get",
    "/soa_vector/get_ref",
    "/soa_vector/ref",
    "/soa_vector/ref_ref",
    "/soa_vector/reserve",
    "/soa_vector/push",
    "/std/collections/experimental_soa_vector/soaVectorCount",
    "/std/collections/experimental_soa_vector/soaVectorCountRef",
    "/std/collections/experimental_soa_vector/soaVectorGet",
    "/std/collections/experimental_soa_vector/soaVectorGetRef",
    "/std/collections/experimental_soa_vector/soaVectorRef",
    "/std/collections/experimental_soa_vector/soaVectorRefRef",
    "/std/collections/experimental_soa_vector/soaVectorFieldView",
    "/std/collections/experimental_soa_vector/soaVectorReserve",
    "/std/collections/experimental_soa_vector/soaVectorPush",
    "/std/collections/experimental_soa_vector_conversions/soaVectorToAos",
    "/std/collections/experimental_soa_vector_conversions/soaVectorToAosRef",
});

constexpr auto CollectionsSoaVectorLoweringSpellings = std::to_array<std::string_view>({
    "/std/collections/soa_vector/count",
    "/std/collections/soa_vector/count_ref",
    "/std/collections/soa_vector/get",
    "/std/collections/soa_vector/get_ref",
    "/std/collections/soa_vector/ref",
    "/std/collections/soa_vector/ref_ref",
    "/std/collections/soa_vector/reserve",
    "/std/collections/soa_vector/push",
    "/std/collections/soa_vector/soaVectorCount",
    "/std/collections/soa_vector/soaVectorCountRef",
    "/std/collections/soa_vector/soaVectorGet",
    "/std/collections/soa_vector/soaVectorGetRef",
    "/std/collections/soa_vector/soaVectorRef",
    "/std/collections/soa_vector/soaVectorRefRef",
    "/std/collections/soa_vector/soaVectorFieldView",
    "/std/collections/soa_vector/soaVectorReserve",
    "/std/collections/soa_vector/soaVectorPush",
    "/std/collections/soa_vector/to_aos",
    "/std/collections/soa_vector/to_aos_ref",
    "/std/collections/soa_vector_conversions/soaVectorToAos",
    "/std/collections/soa_vector_conversions/soaVectorToAosRef",
});

constexpr auto CollectionsSoaVectorConstructorMembers = std::to_array<std::string_view>({
    "soa_vector",
    "SoaVector",
    "soaVectorNew",
    "soaVectorSingle",
    "soaVectorFromAos",
});

constexpr auto CollectionsSoaVectorConstructorImportAliases = std::to_array<std::string_view>({
    "/std/collections/soa_vector",
    "/soa_vector",
    "soa_vector",
});

constexpr auto CollectionsSoaVectorConstructorCompatibilitySpellings = std::to_array<std::string_view>({
    "/std/collections/experimental_soa_vector/SoaVector",
    "/std/collections/experimental_soa_vector/soaVectorNew",
    "/std/collections/experimental_soa_vector/soaVectorSingle",
    "/std/collections/experimental_soa_vector/soaVectorFromAos",
});

constexpr auto CollectionsSoaVectorConstructorLoweringSpellings = std::to_array<std::string_view>({
    "/std/collections/soa_vector/soa_vector",
    "/std/collections/soa_vector/soaVectorNew",
    "/std/collections/soa_vector/soaVectorSingle",
    "/std/collections/soa_vector/soaVectorFromAos",
    "/std/collections/experimental_soa_vector/SoaVector",
    "/std/collections/experimental_soa_vector/soaVectorNew",
    "/std/collections/experimental_soa_vector/soaVectorSingle",
    "/std/collections/experimental_soa_vector/soaVectorFromAos",
});

constexpr auto CollectionsContainerErrorMembers = std::to_array<std::string_view>({
    "why",
    "status",
    "missing_key",
    "index_out_of_bounds",
    "empty",
    "capacity_exceeded",
    "result",
});

constexpr auto CollectionsContainerErrorImportAliases = std::to_array<std::string_view>({
    "/std/collections/ContainerError",
    "/ContainerError",
    "ContainerError",
});

constexpr auto CollectionsContainerErrorCompatibilitySpellings = std::to_array<std::string_view>({
    "/ContainerError/why",
    "/ContainerError/status",
    "/ContainerError/missing_key",
    "/ContainerError/index_out_of_bounds",
    "/ContainerError/empty",
    "/ContainerError/capacity_exceeded",
    "/ContainerError/result",
    "/std/collections/containerMissingKey",
    "/std/collections/containerIndexOutOfBounds",
    "/std/collections/containerEmpty",
    "/std/collections/containerCapacityExceeded",
    "/std/collections/containerErrorStatus",
    "/std/collections/containerErrorResult",
});

constexpr auto CollectionsContainerErrorLoweringSpellings = std::to_array<std::string_view>({
    "/std/collections/ContainerError/why",
    "/std/collections/ContainerError/status",
    "/std/collections/ContainerError/missing_key",
    "/std/collections/ContainerError/index_out_of_bounds",
    "/std/collections/ContainerError/empty",
    "/std/collections/ContainerError/capacity_exceeded",
    "/std/collections/ContainerError/result",
});

constexpr auto GfxBufferHelperMembers = std::to_array<std::string_view>({
    "readback",
    "load",
    "store",
    "allocate",
    "upload",
});

constexpr auto GfxBufferImportAliases = std::to_array<std::string_view>({
    "/std/gfx/Buffer",
    "/Buffer",
    "Buffer",
});

constexpr auto GfxBufferCompatibilitySpellings = std::to_array<std::string_view>({
    "/std/gfx/experimental/Buffer/readback",
    "/std/gfx/experimental/Buffer/load",
    "/std/gfx/experimental/Buffer/store",
    "/std/gfx/experimental/Buffer/allocate",
    "/std/gfx/experimental/Buffer/upload",
});

constexpr auto GfxBufferLoweringSpellings = std::to_array<std::string_view>({
    "/std/gfx/Buffer/readback",
    "/std/gfx/Buffer/load",
    "/std/gfx/Buffer/store",
    "/std/gfx/Buffer/allocate",
    "/std/gfx/Buffer/upload",
    "/std/gpu/readback",
    "/std/gpu/buffer_load",
    "/std/gpu/buffer_store",
    "/std/gpu/buffer",
    "/std/gpu/upload",
});

constexpr auto GfxErrorHelperMembers = std::to_array<std::string_view>({
    "why",
    "status",
    "window_create_failed",
    "device_create_failed",
    "swapchain_create_failed",
    "mesh_create_failed",
    "pipeline_create_failed",
    "material_create_failed",
    "frame_acquire_failed",
    "queue_submit_failed",
    "frame_present_failed",
    "result",
});

constexpr auto GfxErrorImportAliases = std::to_array<std::string_view>({
    "/std/gfx/GfxError",
    "/GfxError",
    "GfxError",
});

constexpr auto GfxErrorCompatibilitySpellings = std::to_array<std::string_view>({
    "/std/gfx/experimental/GfxError",
    "/std/gfx/experimental/ignoreGfxError",
});

constexpr auto GfxErrorLoweringSpellings = std::to_array<std::string_view>({
    "/std/gfx/GfxError/why",
    "/std/gfx/GfxError/status",
    "/std/gfx/GfxError/result",
    "/std/gfx/ignoreGfxError",
    "/std/gfx/experimental/GfxError/result",
});

const std::array<StdlibSurfaceMetadata, 11> Registry = {{
    {
        .id = StdlibSurfaceId::FileHelpers,
        .domain = StdlibSurfaceDomain::File,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = "file.file_helpers",
        .canonicalImportRoot = "/std/file",
        .canonicalPath = "/std/file/File",
        .memberNames = FileHelperMembers,
        .importAliasSpellings = FileHelperImportAliases,
        .compatibilitySpellings = FileHelperCompatibilitySpellings,
        .loweringSpellings = FileHelperLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::FileErrorHelpers,
        .domain = StdlibSurfaceDomain::File,
        .shape = StdlibSurfaceShape::ErrorFamily,
        .bridgeKey = "file.file_error",
        .canonicalImportRoot = "/std/file",
        .canonicalPath = "/std/file/FileError",
        .memberNames = FileErrorHelperMembers,
        .importAliasSpellings = FileErrorImportAliases,
        .compatibilitySpellings = FileErrorCompatibilitySpellings,
        .loweringSpellings = FileErrorLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::CollectionsVectorHelpers,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = "collections.vector_helpers",
        .canonicalImportRoot = "/std/collections",
        .canonicalPath = "/std/collections/vector",
        .memberNames = CollectionsVectorHelperMembers,
        .importAliasSpellings = CollectionsVectorImportAliases,
        .compatibilitySpellings = CollectionsVectorCompatibilitySpellings,
        .loweringSpellings = CollectionsVectorLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::CollectionsVectorConstructors,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::ConstructorFamily,
        .bridgeKey = "collections.vector_constructors",
        .canonicalImportRoot = "/std/collections",
        .canonicalPath = "/std/collections/vector/vector",
        .memberNames = CollectionsVectorConstructorMembers,
        .importAliasSpellings = CollectionsVectorConstructorImportAliases,
        .compatibilitySpellings = CollectionsVectorConstructorCompatibilitySpellings,
        .loweringSpellings = CollectionsVectorConstructorLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::CollectionsMapHelpers,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = "collections.map_helpers",
        .canonicalImportRoot = "/std/collections",
        .canonicalPath = "/std/collections/map",
        .memberNames = CollectionsMapHelperMembers,
        .importAliasSpellings = CollectionsMapImportAliases,
        .compatibilitySpellings = CollectionsMapCompatibilitySpellings,
        .loweringSpellings = CollectionsMapLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::CollectionsMapConstructors,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::ConstructorFamily,
        .bridgeKey = "collections.map_constructors",
        .canonicalImportRoot = "/std/collections",
        .canonicalPath = "/std/collections/map/map",
        .memberNames = CollectionsMapConstructorMembers,
        .importAliasSpellings = CollectionsMapConstructorImportAliases,
        .compatibilitySpellings = CollectionsMapConstructorCompatibilitySpellings,
        .loweringSpellings = CollectionsMapConstructorLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::CollectionsSoaVectorHelpers,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = "collections.soa_vector_helpers",
        .canonicalImportRoot = "/std/collections",
        .canonicalPath = "/std/collections/soa_vector",
        .memberNames = CollectionsSoaVectorHelperMembers,
        .importAliasSpellings = CollectionsSoaVectorImportAliases,
        .compatibilitySpellings = CollectionsSoaVectorCompatibilitySpellings,
        .loweringSpellings = CollectionsSoaVectorLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::CollectionsSoaVectorConstructors,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::ConstructorFamily,
        .bridgeKey = "collections.soa_vector_constructors",
        .canonicalImportRoot = "/std/collections",
        .canonicalPath = "/std/collections/soa_vector/soa_vector",
        .memberNames = CollectionsSoaVectorConstructorMembers,
        .importAliasSpellings = CollectionsSoaVectorConstructorImportAliases,
        .compatibilitySpellings = CollectionsSoaVectorConstructorCompatibilitySpellings,
        .loweringSpellings = CollectionsSoaVectorConstructorLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::CollectionsContainerErrorHelpers,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::ErrorFamily,
        .bridgeKey = "collections.container_error",
        .canonicalImportRoot = "/std/collections",
        .canonicalPath = "/std/collections/ContainerError",
        .memberNames = CollectionsContainerErrorMembers,
        .importAliasSpellings = CollectionsContainerErrorImportAliases,
        .compatibilitySpellings = CollectionsContainerErrorCompatibilitySpellings,
        .loweringSpellings = CollectionsContainerErrorLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::GfxBufferHelpers,
        .domain = StdlibSurfaceDomain::Gfx,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = "gfx.buffer_helpers",
        .canonicalImportRoot = "/std/gfx",
        .canonicalPath = "/std/gfx/Buffer",
        .memberNames = GfxBufferHelperMembers,
        .importAliasSpellings = GfxBufferImportAliases,
        .compatibilitySpellings = GfxBufferCompatibilitySpellings,
        .loweringSpellings = GfxBufferLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::GfxErrorHelpers,
        .domain = StdlibSurfaceDomain::Gfx,
        .shape = StdlibSurfaceShape::ErrorFamily,
        .bridgeKey = "gfx.gfx_error",
        .canonicalImportRoot = "/std/gfx",
        .canonicalPath = "/std/gfx/GfxError",
        .memberNames = GfxErrorHelperMembers,
        .importAliasSpellings = GfxErrorImportAliases,
        .compatibilitySpellings = GfxErrorCompatibilitySpellings,
        .loweringSpellings = GfxErrorLoweringSpellings,
    },
}};

bool matchesAny(std::span<const std::string_view> spellings, std::string_view spelling) {
  return std::find(spellings.begin(), spellings.end(), spelling) != spellings.end();
}

std::string_view stripResolvedPathSpecializationSuffix(std::string_view path) {
  const std::size_t lastSlash = path.rfind('/');
  const std::size_t specializationMarker = path.rfind("__t");
  const std::size_t overloadMarker = path.rfind("__ov");
  std::size_t marker = std::string_view::npos;
  if (specializationMarker != std::string_view::npos &&
      (overloadMarker == std::string_view::npos || specializationMarker > overloadMarker)) {
    marker = specializationMarker;
  } else if (overloadMarker != std::string_view::npos) {
    marker = overloadMarker;
  }
  if (marker == std::string_view::npos || lastSlash == std::string_view::npos || marker <= lastSlash) {
    return path;
  }
  return path.substr(0, marker);
}

bool matchesResolvedRootedMemberPath(std::string_view path,
                                     std::string_view rootPath,
                                     std::span<const std::string_view> memberNames) {
  if (rootPath.empty() || path.size() <= rootPath.size() || !path.starts_with(rootPath) ||
      path[rootPath.size()] != '/') {
    return false;
  }
  const std::string_view memberName =
      stripResolvedPathSpecializationSuffix(path.substr(rootPath.size() + 1));
  if (memberName.empty() || memberName.find('/') != std::string_view::npos) {
    return false;
  }
  return matchesAny(memberNames, memberName);
}

std::string_view pathLeaf(std::string_view path) {
  const std::size_t slash = path.find_last_of('/');
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string_view resolveCollectionsVectorMemberName(std::string_view memberName) {
  if (matchesAny(CollectionsVectorHelperMembers, memberName)) {
    return memberName;
  }
  if (memberName == "vectorPair") {
    return "vector";
  }
  if (memberName == "vectorCount") {
    return "count";
  }
  if (memberName == "vectorCapacity") {
    return "capacity";
  }
  if (memberName == "vectorPush") {
    return "push";
  }
  if (memberName == "vectorPop") {
    return "pop";
  }
  if (memberName == "vectorReserve") {
    return "reserve";
  }
  if (memberName == "vectorClear") {
    return "clear";
  }
  if (memberName == "vectorRemoveAt") {
    return "remove_at";
  }
  if (memberName == "vectorRemoveSwap") {
    return "remove_swap";
  }
  if (memberName == "vectorAt") {
    return "at";
  }
  if (memberName == "vectorAtUnsafe") {
    return "at_unsafe";
  }
  return {};
}

std::string_view resolveCollectionsMapHelperMemberName(std::string_view memberName) {
  if (matchesAny(CollectionsMapHelperMembers, memberName)) {
    return memberName;
  }
  if (memberName == "Count" || memberName == "mapCount") {
    return "count";
  }
  if (memberName == "CountRef" || memberName == "mapCountRef") {
    return "count_ref";
  }
  if (memberName == "Contains" || memberName == "mapContains") {
    return "contains";
  }
  if (memberName == "ContainsRef" || memberName == "mapContainsRef") {
    return "contains_ref";
  }
  if (memberName == "TryAt" || memberName == "mapTryAt") {
    return "tryAt";
  }
  if (memberName == "TryAtRef" || memberName == "mapTryAtRef") {
    return "tryAt_ref";
  }
  if (memberName == "At" || memberName == "mapAt") {
    return "at";
  }
  if (memberName == "AtRef" || memberName == "mapAtRef") {
    return "at_ref";
  }
  if (memberName == "AtUnsafe" || memberName == "mapAtUnsafe") {
    return "at_unsafe";
  }
  if (memberName == "AtUnsafeRef" || memberName == "mapAtUnsafeRef") {
    return "at_unsafe_ref";
  }
  if (memberName == "Insert" || memberName == "mapInsert") {
    return "insert";
  }
  if (memberName == "InsertRef" || memberName == "mapInsertRef" ||
      memberName == "MapInsertRef") {
    return "insert_ref";
  }
  return {};
}

std::string_view resolveCollectionsVectorConstructorMemberName(std::string_view memberName) {
  if (matchesAny(CollectionsVectorConstructorMembers, memberName)) {
    return memberName;
  }
  return {};
}

std::string_view resolveCollectionsMapConstructorMemberName(std::string_view memberName) {
  if (matchesAny(CollectionsMapConstructorMembers, memberName)) {
    return memberName;
  }
  return {};
}

std::string_view resolveCollectionsSoaVectorHelperMemberName(std::string_view memberName) {
  if (matchesAny(CollectionsSoaVectorHelperMembers, memberName)) {
    return memberName;
  }
  if (memberName == "soaVectorCount") {
    return "count";
  }
  if (memberName == "soaVectorCountRef") {
    return "count_ref";
  }
  if (memberName == "soaVectorGet") {
    return "get";
  }
  if (memberName == "soaVectorGetRef") {
    return "get_ref";
  }
  if (memberName == "soaVectorRef") {
    return "ref";
  }
  if (memberName == "soaVectorRefRef") {
    return "ref_ref";
  }
  if (memberName == "soaVectorFieldView") {
    return "field_view";
  }
  if (memberName == "soaVectorReserve") {
    return "reserve";
  }
  if (memberName == "soaVectorPush") {
    return "push";
  }
  if (memberName == "soaVectorToAos") {
    return "to_aos";
  }
  if (memberName == "soaVectorToAosRef") {
    return "to_aos_ref";
  }
  return {};
}

std::string_view resolveCollectionsSoaVectorConstructorMemberName(std::string_view memberName) {
  if (matchesAny(CollectionsSoaVectorConstructorMembers, memberName)) {
    return memberName;
  }
  return {};
}

std::string_view resolveSurfaceMemberNameImpl(const StdlibSurfaceMetadata &metadata,
                                              std::string_view memberName) {
  switch (metadata.id) {
    case StdlibSurfaceId::CollectionsVectorHelpers:
      return resolveCollectionsVectorMemberName(memberName);
    case StdlibSurfaceId::CollectionsVectorConstructors:
      return resolveCollectionsVectorConstructorMemberName(memberName);
    case StdlibSurfaceId::CollectionsMapHelpers:
      return resolveCollectionsMapHelperMemberName(memberName);
    case StdlibSurfaceId::CollectionsMapConstructors:
      return resolveCollectionsMapConstructorMemberName(memberName);
    case StdlibSurfaceId::CollectionsSoaVectorHelpers:
      return resolveCollectionsSoaVectorHelperMemberName(memberName);
    case StdlibSurfaceId::CollectionsSoaVectorConstructors:
      return resolveCollectionsSoaVectorConstructorMemberName(memberName);
    default:
      if (matchesAny(metadata.memberNames, memberName)) {
        return memberName;
      }
      return {};
  }
}

} // namespace

std::span<const StdlibSurfaceMetadata> stdlibSurfaceRegistry() {
  return Registry;
}

const StdlibSurfaceMetadata *findStdlibSurfaceMetadata(StdlibSurfaceId id) {
  const auto it = std::find_if(Registry.begin(), Registry.end(), [id](const StdlibSurfaceMetadata &metadata) {
    return metadata.id == id;
  });
  return it == Registry.end() ? nullptr : &*it;
}

const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByCanonicalPath(std::string_view canonicalPath) {
  const auto it = std::find_if(
      Registry.begin(), Registry.end(), [canonicalPath](const StdlibSurfaceMetadata &metadata) {
        return metadata.canonicalPath == canonicalPath;
      });
  return it == Registry.end() ? nullptr : &*it;
}

const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByBridgeKey(std::string_view bridgeKey) {
  const auto it =
      std::find_if(Registry.begin(), Registry.end(), [bridgeKey](const StdlibSurfaceMetadata &metadata) {
        return metadata.bridgeKey == bridgeKey;
      });
  return it == Registry.end() ? nullptr : &*it;
}

bool stdlibSurfaceMatchesSpelling(const StdlibSurfaceMetadata &metadata, std::string_view spelling) {
  return metadata.canonicalPath == spelling || matchesAny(metadata.importAliasSpellings, spelling) ||
         matchesAny(metadata.compatibilitySpellings, spelling) || matchesAny(metadata.loweringSpellings, spelling);
}

bool isStdlibSurfaceMemberName(StdlibSurfaceId id, std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(id);
  return metadata != nullptr && matchesAny(metadata->memberNames, memberName);
}

bool isStdlibVectorStatementHelperName(std::string_view memberName) {
  return matchesAny(CollectionsVectorStatementHelperMembers, memberName);
}

bool isStdlibMapBaseHelperName(std::string_view memberName) {
  return matchesAny(CollectionsMapBaseHelperMembers, memberName);
}

bool isStdlibMapBorrowedHelperName(std::string_view memberName) {
  return matchesAny(CollectionsMapBorrowedHelperMembers, memberName);
}

const StdlibSurfaceMetadata *findStdlibSurfaceMetadataBySpelling(std::string_view spelling) {
  const auto it = std::find_if(
      Registry.begin(), Registry.end(), [spelling](const StdlibSurfaceMetadata &metadata) {
        return stdlibSurfaceMatchesSpelling(metadata, spelling);
      });
  return it == Registry.end() ? nullptr : &*it;
}

const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByResolvedPath(std::string_view path) {
  if (const auto *metadata = findStdlibSurfaceMetadataBySpelling(path); metadata != nullptr) {
    return metadata;
  }
  const std::string_view normalizedPath = stripResolvedPathSpecializationSuffix(path);
  if (normalizedPath != path) {
    if (const auto *metadata = findStdlibSurfaceMetadataBySpelling(normalizedPath);
        metadata != nullptr) {
      return metadata;
    }
  }
  const auto it = std::find_if(
      Registry.begin(), Registry.end(), [normalizedPath](const StdlibSurfaceMetadata &metadata) {
        if (metadata.shape == StdlibSurfaceShape::ConstructorFamily) {
          return false;
        }
        if (matchesResolvedRootedMemberPath(normalizedPath, metadata.canonicalPath, metadata.memberNames)) {
          return true;
        }
        return std::any_of(metadata.importAliasSpellings.begin(),
                           metadata.importAliasSpellings.end(),
                           [&](std::string_view aliasSpelling) {
                             return matchesResolvedRootedMemberPath(
                                 normalizedPath, aliasSpelling, metadata.memberNames);
                           });
      });
  return it == Registry.end() ? nullptr : &*it;
}

std::string_view resolveStdlibSurfaceMemberName(const StdlibSurfaceMetadata &metadata,
                                                std::string_view path) {
  const std::string_view normalizedPath = stripResolvedPathSpecializationSuffix(path);
  const std::string_view memberName = pathLeaf(normalizedPath);
  return resolveSurfaceMemberNameImpl(metadata, memberName);
}

} // namespace primec
