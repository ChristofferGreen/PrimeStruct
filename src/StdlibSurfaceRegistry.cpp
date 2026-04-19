#include "primec/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <array>

namespace primec {
namespace {

constexpr auto FileHelperMembers = std::to_array<std::string_view>({
    "open_read",
    "open_write",
    "open_append",
    "read_byte",
    "write",
    "write_line",
    "write_byte",
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
    "/file/read_byte",
    "/file/write_byte",
    "/file/write_bytes",
    "/file/flush",
});

constexpr auto FileErrorHelperMembers = std::to_array<std::string_view>({
    "why",
    "status",
    "eof",
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
    "/std/collections/mapContains",
    "/std/collections/mapTryAt",
    "/std/collections/mapAt",
    "/std/collections/mapAtUnsafe",
    "/std/collections/mapInsert",
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

const std::array<StdlibSurfaceMetadata, 8> Registry = {{
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

const StdlibSurfaceMetadata *findStdlibSurfaceMetadataBySpelling(std::string_view spelling) {
  const auto it = std::find_if(
      Registry.begin(), Registry.end(), [spelling](const StdlibSurfaceMetadata &metadata) {
        return stdlibSurfaceMatchesSpelling(metadata, spelling);
      });
  return it == Registry.end() ? nullptr : &*it;
}

} // namespace primec
