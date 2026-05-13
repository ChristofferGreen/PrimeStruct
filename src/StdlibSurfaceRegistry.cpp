#include "primec/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

constexpr auto CollectionsSoaVectorHelperMembers = std::to_array<std::string_view>({
    "count",
    "count_ref",
    "get",
    "get_ref",
    "ref",
    "ref_ref",
    "field_view",
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
    "/std/collections/count",
    "/std/collections/get",
    "/std/collections/ref",
    "/std/collections/reserve",
    "/std/collections/push",
    "/soa_vector/count",
    "/soa_vector/count_ref",
    "/soa_vector/get",
    "/soa_vector/get_ref",
    "/soa_vector/ref",
    "/soa_vector/ref_ref",
    "/soa_vector/field_view",
    "/soa_vector/reserve",
    "/soa_vector/push",
    "/soa_vector/to_aos",
    "/soa_vector/to_aos_ref",
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
    "/std/collections/soa_vector/field_view",
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
    "count",
    "empty",
    "is_valid",
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
    "/std/gfx/experimental/Buffer",
    "/std/gfx/experimental/Buffer/count",
    "/std/gfx/experimental/Buffer/empty",
    "/std/gfx/experimental/Buffer/is_valid",
    "/std/gfx/experimental/Buffer/readback",
    "/std/gfx/experimental/Buffer/load",
    "/std/gfx/experimental/Buffer/store",
    "/std/gfx/experimental/Buffer/allocate",
    "/std/gfx/experimental/Buffer/upload",
});

constexpr auto GfxBufferLoweringSpellings = std::to_array<std::string_view>({
    "/std/gfx/Buffer/count",
    "/std/gfx/Buffer/empty",
    "/std/gfx/Buffer/is_valid",
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

struct StringListStore {
  std::vector<std::string> values;
  std::vector<std::string_view> views;

  void refreshViews() {
    views.clear();
    views.reserve(values.size());
    for (const std::string &value : values) {
      views.push_back(value);
    }
  }
};

struct MemberAliasStore {
  std::vector<std::pair<std::string, std::string>> values;
  std::vector<StdlibSurfaceMemberAlias> views;

  void refreshViews() {
    views.clear();
    views.reserve(values.size());
    for (const auto &[spelling, memberName] : values) {
      views.push_back({.spelling = spelling, .memberName = memberName});
    }
  }
};

struct ManifestSurfaceRecord {
  std::optional<StdlibSurfaceId> id;
  std::string bridgeKey;
  std::string canonicalImportRoot;
  std::string canonicalPath;
  StringListStore memberNames;
  StringListStore statementMemberNames;
  StringListStore importAliasSpellings;
  StringListStore compatibilitySpellings;
  StringListStore loweringSpellings;
  MemberAliasStore memberAliases;
};

struct ManifestSurfaceData {
  std::string bridgeKey;
  std::string canonicalImportRoot;
  std::string canonicalPath;
  StringListStore memberNames;
  StringListStore statementMemberNames;
  StringListStore importAliasSpellings;
  StringListStore compatibilitySpellings;
  StringListStore loweringSpellings;
  MemberAliasStore memberAliases;

  void refreshViews() {
    memberNames.refreshViews();
    statementMemberNames.refreshViews();
    importAliasSpellings.refreshViews();
    compatibilitySpellings.refreshViews();
    loweringSpellings.refreshViews();
    memberAliases.refreshViews();
  }
};

struct CollectionsManifestSurfaces {
  ManifestSurfaceData vectorHelpers;
  ManifestSurfaceData vectorConstructors;
  ManifestSurfaceData mapHelpers;
  ManifestSurfaceData mapConstructors;
};

std::string trimAscii(std::string_view value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.remove_prefix(1);
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.remove_suffix(1);
  }
  return std::string(value);
}

std::optional<StdlibSurfaceId> parseManifestSurfaceId(std::string_view value) {
  const std::string legacyVectorHelperSurfaceId =
      std::string("Collections") + "Vector" + "Helpers";
  if (value == legacyVectorHelperSurfaceId) {
    return StdlibSurfaceId::CollectionsVectorHelperSurface;
  }
  if (value == "CollectionsVectorConstructors") {
    return StdlibSurfaceId::CollectionsVectorConstructors;
  }
  if (value == "CollectionsMapHelpers") {
    return StdlibSurfaceId::CollectionsMapHelpers;
  }
  if (value == "CollectionsMapConstructors") {
    return StdlibSurfaceId::CollectionsMapConstructors;
  }
  return std::nullopt;
}

void appendManifestValue(ManifestSurfaceRecord &record,
                         std::string_view key,
                         std::string value) {
  if (key == "id") {
    record.id = parseManifestSurfaceId(value);
  } else if (key == "bridge_key") {
    record.bridgeKey = std::move(value);
  } else if (key == "canonical_import_root") {
    record.canonicalImportRoot = std::move(value);
  } else if (key == "canonical_path") {
    record.canonicalPath = std::move(value);
  } else if (key == "member_name") {
    record.memberNames.values.push_back(std::move(value));
  } else if (key == "statement_member_name") {
    record.statementMemberNames.values.push_back(std::move(value));
  } else if (key == "import_alias_spelling") {
    record.importAliasSpellings.values.push_back(std::move(value));
  } else if (key == "compatibility_spelling") {
    record.compatibilitySpellings.values.push_back(std::move(value));
  } else if (key == "lowering_spelling") {
    record.loweringSpellings.values.push_back(std::move(value));
  } else if (key == "member_alias") {
    const std::string separator = "->";
    const std::size_t separatorPos = value.find(separator);
    if (separatorPos != std::string::npos) {
      record.memberAliases.values.push_back({
          trimAscii(std::string_view(value).substr(0, separatorPos)),
          trimAscii(std::string_view(value).substr(separatorPos + separator.size())),
      });
    }
  }
}

std::optional<std::filesystem::path> findStdlibSurfaceManifestPath() {
  const std::filesystem::path relativePath =
      std::filesystem::path("stdlib") / "std" / "collections" / "surfaces.psmeta";
  auto findFromRoot = [&](std::filesystem::path root) -> std::optional<std::filesystem::path> {
    std::error_code ec;
    for (std::size_t depth = 0; depth < 8 && !root.empty(); ++depth) {
      const std::filesystem::path candidate = root / relativePath;
      if (std::filesystem::exists(candidate, ec) && !ec) {
        return candidate;
      }
      root = root.parent_path();
    }
    return std::nullopt;
  };

  if (auto fromSource = findFromRoot(std::filesystem::path(__FILE__).parent_path().parent_path());
      fromSource.has_value()) {
    return fromSource;
  }

  std::error_code ec;
  const std::filesystem::path cwd = std::filesystem::current_path(ec);
  if (!ec) {
    return findFromRoot(cwd);
  }
  return std::nullopt;
}

std::vector<ManifestSurfaceRecord> readStdlibSurfaceManifest() {
  const std::optional<std::filesystem::path> manifestPath = findStdlibSurfaceManifestPath();
  if (!manifestPath.has_value()) {
    return {};
  }

  std::ifstream input(*manifestPath);
  if (!input) {
    return {};
  }

  std::vector<ManifestSurfaceRecord> records;
  ManifestSurfaceRecord *currentRecord = nullptr;
  std::string line;
  while (std::getline(input, line)) {
    const std::size_t commentPos = line.find('#');
    if (commentPos != std::string::npos) {
      line.resize(commentPos);
    }
    const std::string trimmed = trimAscii(line);
    if (trimmed.empty()) {
      continue;
    }
    if (trimmed == "[surface]") {
      records.emplace_back();
      currentRecord = &records.back();
      continue;
    }
    if (currentRecord == nullptr) {
      continue;
    }
    const std::size_t equalsPos = trimmed.find('=');
    if (equalsPos == std::string::npos) {
      continue;
    }
    appendManifestValue(*currentRecord,
                        trimAscii(std::string_view(trimmed).substr(0, equalsPos)),
                        trimAscii(std::string_view(trimmed).substr(equalsPos + 1)));
  }
  return records;
}

void applyManifestSurfaceRecord(ManifestSurfaceData &surface,
                                ManifestSurfaceRecord &&record) {
  surface.bridgeKey = std::move(record.bridgeKey);
  surface.canonicalImportRoot = std::move(record.canonicalImportRoot);
  surface.canonicalPath = std::move(record.canonicalPath);
  surface.memberNames.values = std::move(record.memberNames.values);
  surface.statementMemberNames.values = std::move(record.statementMemberNames.values);
  surface.importAliasSpellings.values = std::move(record.importAliasSpellings.values);
  surface.compatibilitySpellings.values = std::move(record.compatibilitySpellings.values);
  surface.loweringSpellings.values = std::move(record.loweringSpellings.values);
  surface.memberAliases.values = std::move(record.memberAliases.values);
  surface.refreshViews();
}

CollectionsManifestSurfaces loadCollectionsManifestSurfaces() {
  CollectionsManifestSurfaces surfaces;
  for (ManifestSurfaceRecord &record : readStdlibSurfaceManifest()) {
    if (!record.id.has_value()) {
      continue;
    }
    switch (*record.id) {
      case StdlibSurfaceId::CollectionsVectorHelperSurface:
        applyManifestSurfaceRecord(surfaces.vectorHelpers, std::move(record));
        break;
      case StdlibSurfaceId::CollectionsVectorConstructors:
        applyManifestSurfaceRecord(surfaces.vectorConstructors, std::move(record));
        break;
      case StdlibSurfaceId::CollectionsMapHelpers:
        applyManifestSurfaceRecord(surfaces.mapHelpers, std::move(record));
        break;
      case StdlibSurfaceId::CollectionsMapConstructors:
        applyManifestSurfaceRecord(surfaces.mapConstructors, std::move(record));
        break;
      default:
        break;
    }
  }
  return surfaces;
}

const CollectionsManifestSurfaces CollectionsSurfaces = loadCollectionsManifestSurfaces();

const std::array<StdlibSurfaceMetadata, 11> Registry = {{
    {
        .id = StdlibSurfaceId::FileHelpers,
        .domain = StdlibSurfaceDomain::File,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = "file.file_helpers",
        .canonicalImportRoot = "/std/file",
        .canonicalPath = "/std/file/File",
        .memberNames = FileHelperMembers,
        .memberAliases = {},
        .statementMemberNames = {},
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
        .memberAliases = {},
        .statementMemberNames = {},
        .importAliasSpellings = FileErrorImportAliases,
        .compatibilitySpellings = FileErrorCompatibilitySpellings,
        .loweringSpellings = FileErrorLoweringSpellings,
    },
    {
        .id = StdlibSurfaceId::CollectionsVectorHelperSurface,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = CollectionsSurfaces.vectorHelpers.bridgeKey,
        .canonicalImportRoot = CollectionsSurfaces.vectorHelpers.canonicalImportRoot,
        .canonicalPath = CollectionsSurfaces.vectorHelpers.canonicalPath,
        .memberNames = CollectionsSurfaces.vectorHelpers.memberNames.views,
        .memberAliases = CollectionsSurfaces.vectorHelpers.memberAliases.views,
        .statementMemberNames = CollectionsSurfaces.vectorHelpers.statementMemberNames.views,
        .importAliasSpellings = CollectionsSurfaces.vectorHelpers.importAliasSpellings.views,
        .compatibilitySpellings = CollectionsSurfaces.vectorHelpers.compatibilitySpellings.views,
        .loweringSpellings = CollectionsSurfaces.vectorHelpers.loweringSpellings.views,
    },
    {
        .id = StdlibSurfaceId::CollectionsVectorConstructors,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::ConstructorFamily,
        .bridgeKey = CollectionsSurfaces.vectorConstructors.bridgeKey,
        .canonicalImportRoot = CollectionsSurfaces.vectorConstructors.canonicalImportRoot,
        .canonicalPath = CollectionsSurfaces.vectorConstructors.canonicalPath,
        .memberNames = CollectionsSurfaces.vectorConstructors.memberNames.views,
        .memberAliases = CollectionsSurfaces.vectorConstructors.memberAliases.views,
        .statementMemberNames = CollectionsSurfaces.vectorConstructors.statementMemberNames.views,
        .importAliasSpellings = CollectionsSurfaces.vectorConstructors.importAliasSpellings.views,
        .compatibilitySpellings = CollectionsSurfaces.vectorConstructors.compatibilitySpellings.views,
        .loweringSpellings = CollectionsSurfaces.vectorConstructors.loweringSpellings.views,
    },
    {
        .id = StdlibSurfaceId::CollectionsMapHelpers,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = CollectionsSurfaces.mapHelpers.bridgeKey,
        .canonicalImportRoot = CollectionsSurfaces.mapHelpers.canonicalImportRoot,
        .canonicalPath = CollectionsSurfaces.mapHelpers.canonicalPath,
        .memberNames = CollectionsSurfaces.mapHelpers.memberNames.views,
        .memberAliases = CollectionsSurfaces.mapHelpers.memberAliases.views,
        .statementMemberNames = CollectionsSurfaces.mapHelpers.statementMemberNames.views,
        .importAliasSpellings = CollectionsSurfaces.mapHelpers.importAliasSpellings.views,
        .compatibilitySpellings = CollectionsSurfaces.mapHelpers.compatibilitySpellings.views,
        .loweringSpellings = CollectionsSurfaces.mapHelpers.loweringSpellings.views,
    },
    {
        .id = StdlibSurfaceId::CollectionsMapConstructors,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::ConstructorFamily,
        .bridgeKey = CollectionsSurfaces.mapConstructors.bridgeKey,
        .canonicalImportRoot = CollectionsSurfaces.mapConstructors.canonicalImportRoot,
        .canonicalPath = CollectionsSurfaces.mapConstructors.canonicalPath,
        .memberNames = CollectionsSurfaces.mapConstructors.memberNames.views,
        .memberAliases = CollectionsSurfaces.mapConstructors.memberAliases.views,
        .statementMemberNames = CollectionsSurfaces.mapConstructors.statementMemberNames.views,
        .importAliasSpellings = CollectionsSurfaces.mapConstructors.importAliasSpellings.views,
        .compatibilitySpellings = CollectionsSurfaces.mapConstructors.compatibilitySpellings.views,
        .loweringSpellings = CollectionsSurfaces.mapConstructors.loweringSpellings.views,
    },
    {
        .id = StdlibSurfaceId::CollectionsSoaVectorHelpers,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = "collections.soa_vector_helpers",
        .canonicalImportRoot = "/std/collections",
        .canonicalPath = "/std/collections/soa_vector",
        .memberNames = CollectionsSoaVectorHelperMembers,
        .memberAliases = {},
        .statementMemberNames = {},
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
        .memberAliases = {},
        .statementMemberNames = {},
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
        .memberAliases = {},
        .statementMemberNames = {},
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
        .memberAliases = {},
        .statementMemberNames = {},
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
        .memberAliases = {},
        .statementMemberNames = {},
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

std::string_view resolveMetadataMemberName(const StdlibSurfaceMetadata &metadata,
                                           std::string_view memberName) {
  if (matchesAny(metadata.memberNames, memberName)) {
    return memberName;
  }
  const auto it = std::find_if(metadata.memberAliases.begin(),
                               metadata.memberAliases.end(),
                               [memberName](const StdlibSurfaceMemberAlias &alias) {
                                 return alias.spelling == memberName;
                               });
  if (it != metadata.memberAliases.end()) {
    return it->memberName;
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
    case StdlibSurfaceId::CollectionsVectorHelperSurface:
    case StdlibSurfaceId::CollectionsVectorConstructors:
    case StdlibSurfaceId::CollectionsMapHelpers:
    case StdlibSurfaceId::CollectionsMapConstructors:
      return resolveMetadataMemberName(metadata, memberName);
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

std::string stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId id, std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(id);
  if (metadata == nullptr || metadata->shape == StdlibSurfaceShape::ConstructorFamily) {
    return {};
  }
  if (helperName.find('/') != std::string_view::npos) {
    const StdlibSurfaceMetadata *helperMetadata =
        findStdlibSurfaceMetadataByResolvedPath(helperName);
    if (helperMetadata == nullptr || helperMetadata->id != id) {
      return {};
    }
  }
  const std::string_view resolvedMemberName = resolveStdlibSurfaceMemberName(*metadata, helperName);
  if (resolvedMemberName.empty()) {
    return {};
  }
  return std::string(metadata->canonicalPath) + "/" + std::string(resolvedMemberName);
}

std::string stdlibSurfacePreferredSpellingForMember(StdlibSurfaceId id,
                                                    std::string_view spelling,
                                                    std::string_view preferredPrefix) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(id);
  if (metadata == nullptr) {
    return {};
  }
  if (spelling.find('/') != std::string_view::npos) {
    const StdlibSurfaceMetadata *spellingMetadata =
        findStdlibSurfaceMetadataByResolvedPath(spelling);
    if (spellingMetadata == nullptr || spellingMetadata->id != id) {
      return {};
    }
  }
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, spelling);
  if (memberName.empty()) {
    return {};
  }
  auto findPreferred = [&](std::span<const std::string_view> spellings) {
    for (const std::string_view candidate : spellings) {
      if (candidate.rfind(preferredPrefix, 0) == 0 &&
          resolveStdlibSurfaceMemberName(*metadata, candidate) == memberName) {
        return std::string(candidate);
      }
    }
    return std::string{};
  };
  if (std::string preferred = findPreferred(metadata->loweringSpellings);
      !preferred.empty()) {
    return preferred;
  }
  return findPreferred(metadata->compatibilitySpellings);
}

bool isStdlibSurfaceMemberName(StdlibSurfaceId id, std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(id);
  return metadata != nullptr && matchesAny(metadata->memberNames, memberName);
}

bool isStdlibVectorStatementHelperName(std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(StdlibSurfaceId::CollectionsVectorHelperSurface);
  return metadata != nullptr && matchesAny(metadata->statementMemberNames, memberName);
}

bool isStdlibMapBaseHelperName(std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(StdlibSurfaceId::CollectionsMapHelpers);
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view resolvedMemberName =
      resolveMetadataMemberName(*metadata, memberName);
  return !resolvedMemberName.empty() && resolvedMemberName != "entry" &&
         resolvedMemberName != "map" && !resolvedMemberName.ends_with("_ref");
}

bool isStdlibMapBorrowedHelperName(std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(StdlibSurfaceId::CollectionsMapHelpers);
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view resolvedMemberName =
      resolveMetadataMemberName(*metadata, memberName);
  return resolvedMemberName.ends_with("_ref");
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
