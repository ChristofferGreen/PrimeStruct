// collection-surface-audit: exempt
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

struct ManifestSurfaceData {
  std::string bridgeKey;
  std::string canonicalImportRoot;
  std::string canonicalPath;
  std::string backingTypeName;
  StringListStore memberNames;
  StringListStore statementMemberNames;
  StringListStore importAliasSpellings;
  StringListStore compatibilitySpellings;
  StringListStore loweringSpellings;
  MemberAliasStore memberAliases;
  MemberAliasStore borrowedVariants;

  void refreshViews() {
    memberNames.refreshViews();
    statementMemberNames.refreshViews();
    importAliasSpellings.refreshViews();
    compatibilitySpellings.refreshViews();
    loweringSpellings.refreshViews();
    memberAliases.refreshViews();
    borrowedVariants.refreshViews();
  }
};

struct CollectionsManifestSurfaces {
  ManifestSurfaceData vectorHelpers;
  ManifestSurfaceData vectorConstructors;
  ManifestSurfaceData mapHelpers;
  ManifestSurfaceData mapConstructors;
  ManifestSurfaceData soaHelpers;
  ManifestSurfaceData soaConstructors;
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

// ---------------------------------------------------------------------------
// Derivation of collection surface metadata from [public] stdlib declarations.
// ---------------------------------------------------------------------------

struct ScannedFunctionRecord {
  std::string name;           // member name (leaf for rooted paths, bare otherwise)
  bool isStatementMember;     // return<void> + first param has " mut]"
  bool isConstructor;         // return type contains the collection type name
  bool takesCollectionParam;  // first param type contains the collection type name
};

std::optional<std::filesystem::path> findStdlibCollectionFilePath(std::string_view filename) {
  const std::filesystem::path relativePath =
      std::filesystem::path("stdlib") / "std" / "collections" / filename;
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

// Returns the leaf name from a rooted path (e.g. "/std/collections/soa/count" → "count").
// Returns the whole string if there is no slash.
static std::string_view pathLeafOf(std::string_view path) {
  const auto pos = path.rfind('/');
  return pos == std::string_view::npos ? path : path.substr(pos + 1);
}

// Trim leading whitespace and return the indentation depth (number of leading spaces).
static std::size_t leadingSpaces(std::string_view line) {
  std::size_t count = 0;
  for (char c : line) {
    if (c == ' ') {
      ++count;
    } else if (c == '\t') {
      count += 2;
    } else {
      break;
    }
  }
  return count;
}

// Returns the first [param type] content from a possibly-multi-line signature buffer.
// The buffer is assembled from the function name line plus any continuation lines.
// Returns the text between the first '[' and its matching ']' after '('.
static std::string firstParamType(std::string_view signatureBuffer) {
  const auto openParen = signatureBuffer.find('(');
  if (openParen == std::string_view::npos) {
    return {};
  }
  const auto openBracket = signatureBuffer.find('[', openParen);
  if (openBracket == std::string_view::npos) {
    return {};
  }
  const auto closeBracket = signatureBuffer.find(']', openBracket + 1);
  if (closeBracket == std::string_view::npos) {
    return {};
  }
  return std::string(signatureBuffer.substr(openBracket + 1, closeBracket - openBracket - 1));
}

// Extract the bare function name from a declaration line.
// Handles both rooted paths ("/std/collections/soa/count<T>(...)") and bare
// names ("count<K, V>(...)"). Returns the leaf name in both cases.
static std::string extractFunctionName(std::string_view line) {
  std::string_view trimmed = line;
  while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) {
    trimmed.remove_prefix(1);
  }
  // Rooted path: starts with '/'
  const bool isRooted = !trimmed.empty() && trimmed.front() == '/';
  const std::size_t templateStart = trimmed.find('<');
  const std::size_t parenStart = trimmed.find('(');
  const std::size_t end = std::min(templateStart, parenStart);
  const std::string_view full = (end == std::string_view::npos) ? trimmed : trimmed.substr(0, end);
  if (isRooted) {
    return std::string(pathLeafOf(full));
  }
  return std::string(full);
}

// Scan a stdlib .prime file for public function declarations.
// skipLongNamePrefix: skip names starting with this prefix followed by an
//   uppercase letter (e.g., "vector" skips "vectorCount" but not "vector").
// collectionTypeName: used to detect constructor and collection-param functions
//   (e.g., "Vector<", "MapValue<", "SoaVector<").
// detectStatementMembers: if true, check return<void> + mut-param heuristic.
static std::vector<ScannedFunctionRecord> scanStdlibPublicFunctions(
    const std::filesystem::path &filepath,
    std::string_view skipLongNamePrefix,
    std::string_view collectionTypeName,
    bool detectStatementMembers) {
  std::ifstream input(filepath);
  if (!input) {
    return {};
  }

  // Pre-read all lines so we can do lookahead for multi-line function signatures.
  std::vector<std::string> lines;
  {
    std::string line;
    while (std::getline(input, line)) {
      lines.push_back(std::move(line));
    }
  }

  std::vector<ScannedFunctionRecord> results;
  bool expectingFunctionName = false;
  bool savedReturnVoid = false;
  bool savedReturnsCollectionType = false;

  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::string_view sv(lines[i]);
    const std::string trimmed = trimAscii(sv);

    if (expectingFunctionName) {
      if (trimmed.empty()) {
        continue;
      }
      // Skip comment lines (// ...) but not rooted paths (/ ...)
      if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/') {
        continue;
      }
      if (trimmed.front() == '[') {
        // Another annotation — reset and parse this one instead
        expectingFunctionName = false;
        // Fall through to parse this line as a potential annotation
      } else {
        // This is the function declaration line (possibly first line of a multi-line sig).
        const std::string name = extractFunctionName(lines[i]);
        if (!name.empty()) {
          // Apply long-name filter: skip "prefix[A-Z]..." patterns
          bool skip = false;
          if (!skipLongNamePrefix.empty() && name.size() > skipLongNamePrefix.size() &&
              name.rfind(skipLongNamePrefix, 0) == 0) {
            const char nextChar = name[skipLongNamePrefix.size()];
            if (nextChar >= 'A' && nextChar <= 'Z') {
              skip = true;
            }
          }
          if (!skip) {
            // Build signature buffer: join continuation lines if params weren't on this line.
            std::string sigBuffer = lines[i];
            if (firstParamType(sigBuffer).empty() && i + 1 < lines.size()) {
              // Signature continues onto the next line (e.g. field_view in soa.prime).
              sigBuffer += " " + lines[i + 1];
            }
            const std::string paramType = firstParamType(sigBuffer);
            const bool takesCollectionParam =
                !collectionTypeName.empty() &&
                paramType.find(collectionTypeName) != std::string::npos;
            // Statement member: return<void> annotation AND first param ends with " mut"
            // (the param type content between '[' and ']' ends with space+mut).
            const bool isStatementMember =
                detectStatementMembers && savedReturnVoid &&
                paramType.size() >= 4 &&
                paramType.rfind(" mut") == paramType.size() - 4;
            results.push_back({
                .name = name,
                .isStatementMember = isStatementMember,
                .isConstructor = savedReturnsCollectionType,
                .takesCollectionParam = takesCollectionParam,
            });
          }
        }
        expectingFunctionName = false;
        continue;
      }
    }

    // Check if this is a public function annotation at acceptable indent depth.
    // Struct-method annotations are indented 4+ spaces; skip them.
    if (leadingSpaces(sv) >= 4) {
      continue;
    }
    if (trimmed.find("[public") == std::string_view::npos) {
      continue;
    }
    if (trimmed.find("return<") == std::string_view::npos) {
      continue;
    }
    // Looks like a public function annotation. Save attributes.
    savedReturnVoid = trimmed.find("return<void>") != std::string_view::npos;
    savedReturnsCollectionType =
        !collectionTypeName.empty() &&
        trimmed.find("return<" + std::string(collectionTypeName)) != std::string_view::npos;
    if (!savedReturnsCollectionType && !collectionTypeName.empty()) {
      // Also accept "return</" + collection path prefix (e.g. return</std/.../>)
      const std::string returnPathPrefix = "return</";
      const auto pos = trimmed.find(returnPathPrefix);
      if (pos != std::string_view::npos) {
        const std::string typeText = std::string(trimmed.substr(pos + returnPathPrefix.size() - 1));
        if (typeText.find(std::string(collectionTypeName)) != std::string::npos) {
          savedReturnsCollectionType = true;
        }
      }
    }
    expectingFunctionName = true;
  }
  return results;
}

// Build a ManifestSurfaceData from the given list of member names plus
// derived import_alias_spellings and lowering_spellings.
// loweredMembers: subset of memberNames that get lowering_spellings entries
//   (canonical_path + "/" + name).
// importCanonicalPath / importShortAlias: used for import_alias_spellings.
// canonicalPathBase: prefix for lowering_spellings (may differ from canonical_path
//   for constructor surfaces).
static ManifestSurfaceData buildSurfaceData(
    std::string bridgeKey,
    std::string canonicalImportRoot,
    std::string canonicalPath,
    std::string backingTypeName,
    std::vector<std::string> memberNames,
    std::vector<std::string> statementMemberNames,
    std::vector<std::string> loweredMemberNames,
    std::string_view importCanonicalPath,
    std::string_view importShortAlias,
    std::string_view loweringPathBase) {
  ManifestSurfaceData d;
  d.bridgeKey = std::move(bridgeKey);
  d.canonicalImportRoot = std::move(canonicalImportRoot);
  d.canonicalPath = std::move(canonicalPath);
  d.backingTypeName = std::move(backingTypeName);
  d.memberNames.values = std::move(memberNames);
  d.statementMemberNames.values = std::move(statementMemberNames);
  // import_alias_spellings: [canonical_path, short_alias]
  if (!importCanonicalPath.empty()) {
    d.importAliasSpellings.values.emplace_back(importCanonicalPath);
  }
  if (!importShortAlias.empty()) {
    d.importAliasSpellings.values.emplace_back(importShortAlias);
  }
  // lowering_spellings: loweringPathBase + "/" + member for each lowered member
  for (const std::string &member : loweredMemberNames) {
    d.loweringSpellings.values.push_back(std::string(loweringPathBase) + "/" + member);
  }
  d.refreshViews();
  return d;
}

static CollectionsManifestSurfaces deriveCollectionsSurfaces() {
  CollectionsManifestSurfaces derived;

  // --- Vector ---
  {
    const auto path = findStdlibCollectionFilePath("vector.prime");
    const auto records = path.has_value()
        ? scanStdlibPublicFunctions(*path, "vector", "Vector<", /*detectStatement=*/true)
        : std::vector<ScannedFunctionRecord>{};

    std::vector<std::string> helperMembers;
    std::vector<std::string> helperStatements;
    std::vector<std::string> helperLowered;
    std::vector<std::string> ctorMembers;

    for (const auto &rec : records) {
      if (rec.isConstructor) {
        // "vector" constructor goes to BOTH helper and constructor surfaces
        if (std::find(ctorMembers.begin(), ctorMembers.end(), rec.name) == ctorMembers.end()) {
          ctorMembers.push_back(rec.name);
        }
        if (std::find(helperMembers.begin(), helperMembers.end(), rec.name) == helperMembers.end()) {
          helperMembers.push_back(rec.name);
        }
        // constructors are NOT in lowering_spellings of the helper surface
      } else {
        if (std::find(helperMembers.begin(), helperMembers.end(), rec.name) == helperMembers.end()) {
          helperMembers.push_back(rec.name);
        }
        if (rec.isStatementMember) {
          helperStatements.push_back(rec.name);
        }
        if (rec.takesCollectionParam) {
          helperLowered.push_back(rec.name);
        }
      }
    }

    derived.vectorHelpers = buildSurfaceData(
        "collections.vector_helpers", "/std/collections", "/std/collections/vector", "",
        helperMembers, helperStatements, helperLowered,
        "/std/collections/vector", "vector", "/std/collections/vector");

    std::vector<std::string> ctorLowered;
    for (const auto &n : ctorMembers) {
      ctorLowered.push_back(n);
    }
    derived.vectorConstructors = buildSurfaceData(
        "collections.vector_constructors", "/std/collections", "/std/collections/vector/vector", "",
        ctorMembers, {}, ctorLowered,
        "/std/collections/vector", "vector", "/std/collections/vector");
  }

  // --- Map ---
  {
    const auto path = findStdlibCollectionFilePath("map.prime");
    const auto records = path.has_value()
        ? scanStdlibPublicFunctions(*path, "map", "MapValue<", /*detectStatement=*/false)
        : std::vector<ScannedFunctionRecord>{};

    std::vector<std::string> helperMembers;
    std::vector<std::string> helperLowered;
    std::vector<std::string> ctorMembers;

    for (const auto &rec : records) {
      if (rec.isConstructor) {
        if (std::find(ctorMembers.begin(), ctorMembers.end(), rec.name) == ctorMembers.end()) {
          ctorMembers.push_back(rec.name);
        }
        // Constructor also goes into helper members (but NOT into helper lowering_spellings)
        if (std::find(helperMembers.begin(), helperMembers.end(), rec.name) == helperMembers.end()) {
          helperMembers.push_back(rec.name);
        }
      } else {
        if (std::find(helperMembers.begin(), helperMembers.end(), rec.name) == helperMembers.end()) {
          helperMembers.push_back(rec.name);
        }
        // "entry" produces Entry<K,V> not MapValue — its first param is not MapValue
        // so takesCollectionParam is false, and it won't appear in helperLowered.
        if (rec.takesCollectionParam) {
          helperLowered.push_back(rec.name);
        }
      }
    }

    derived.mapHelpers = buildSurfaceData(
        "collections.map_helpers", "/std/collections", "/std/collections/map", "MapValue",
        helperMembers, {}, helperLowered,
        "/std/collections/map", "map", "/std/collections/map");

    std::vector<std::string> ctorLowered;
    for (const auto &n : ctorMembers) {
      ctorLowered.push_back(n);
    }
    derived.mapConstructors = buildSurfaceData(
        "collections.map_constructors", "/std/collections", "/std/collections/map/map", "",
        ctorMembers, {}, ctorLowered,
        "/std/collections/map", "map", "/std/collections/map");
  }

  // --- Soa ---
  {
    const auto path = findStdlibCollectionFilePath("soa.prime");
    const auto records = path.has_value()
        ? scanStdlibPublicFunctions(*path, "soaVector", "SoaVector<", /*detectStatement=*/false)
        : std::vector<ScannedFunctionRecord>{};

    std::vector<std::string> helperMembers;
    std::vector<std::string> helperLowered;
    std::vector<std::string> ctorMembers;
    std::vector<std::string> ctorLowered;

    for (const auto &rec : records) {
      if (rec.isConstructor) {
        if (std::find(ctorMembers.begin(), ctorMembers.end(), rec.name) == ctorMembers.end()) {
          ctorMembers.push_back(rec.name);
          ctorLowered.push_back(rec.name);
        }
        // SOA constructor names do NOT go into the helper surface
      } else {
        if (std::find(helperMembers.begin(), helperMembers.end(), rec.name) == helperMembers.end()) {
          helperMembers.push_back(rec.name);
        }
        if (rec.takesCollectionParam) {
          helperLowered.push_back(rec.name);
        }
      }
    }

    derived.soaHelpers = buildSurfaceData(
        "collections.soa_helpers", "/std/collections", "/std/collections/soa", "",
        helperMembers, {}, helperLowered,
        "/std/collections/soa", "soa", "/std/collections/soa");

    derived.soaConstructors = buildSurfaceData(
        "collections.soa_constructors", "/std/collections", "/std/collections/soa/soa", "",
        ctorMembers, {}, ctorLowered,
        "/std/collections/soa", "soa", "/std/collections/soa");
  }

  return derived;
}

const CollectionsManifestSurfaces CollectionsSurfaces = deriveCollectionsSurfaces();

constexpr StdlibSurfaceId collectionSurfaceId(std::size_t slotIndex) {
  return static_cast<StdlibSurfaceId>(
      static_cast<int>(StdlibSurfaceId::CollectionsManifestSurface0) +
      static_cast<int>(slotIndex));
}

static_assert(collectionSurfaceId(0) == StdlibSurfaceId::CollectionsManifestSurface0);
static_assert(collectionSurfaceId(5) == StdlibSurfaceId::CollectionsColumnarConstructors);

const std::array<StdlibSurfaceMetadata, 11> Registry = {{
    {
        .id = StdlibSurfaceId::FileHelpers,
        .domain = StdlibSurfaceDomain::File,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = "file.file_helpers",
        .canonicalImportRoot = "/std/file",
        .canonicalPath = "/std/file/File",
        .backingTypeName = {},
        .memberNames = FileHelperMembers,
        .memberAliases = {},
        .statementMemberNames = {},
        .importAliasSpellings = FileHelperImportAliases,
        .compatibilitySpellings = FileHelperCompatibilitySpellings,
        .loweringSpellings = FileHelperLoweringSpellings,
        .borrowedVariants = {},
    },
    {
        .id = StdlibSurfaceId::FileErrorHelpers,
        .domain = StdlibSurfaceDomain::File,
        .shape = StdlibSurfaceShape::ErrorFamily,
        .bridgeKey = "file.file_error",
        .canonicalImportRoot = "/std/file",
        .canonicalPath = "/std/file/FileError",
        .backingTypeName = {},
        .memberNames = FileErrorHelperMembers,
        .memberAliases = {},
        .statementMemberNames = {},
        .importAliasSpellings = FileErrorImportAliases,
        .compatibilitySpellings = FileErrorCompatibilitySpellings,
        .loweringSpellings = FileErrorLoweringSpellings,
        .borrowedVariants = {},
    },
    {
        .id = collectionSurfaceId(0),
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = CollectionsSurfaces.vectorHelpers.bridgeKey,
        .canonicalImportRoot = CollectionsSurfaces.vectorHelpers.canonicalImportRoot,
        .canonicalPath = CollectionsSurfaces.vectorHelpers.canonicalPath,
        .backingTypeName = CollectionsSurfaces.vectorHelpers.backingTypeName,
        .memberNames = CollectionsSurfaces.vectorHelpers.memberNames.views,
        .memberAliases = CollectionsSurfaces.vectorHelpers.memberAliases.views,
        .statementMemberNames = CollectionsSurfaces.vectorHelpers.statementMemberNames.views,
        .importAliasSpellings = CollectionsSurfaces.vectorHelpers.importAliasSpellings.views,
        .compatibilitySpellings = CollectionsSurfaces.vectorHelpers.compatibilitySpellings.views,
        .loweringSpellings = CollectionsSurfaces.vectorHelpers.loweringSpellings.views,
        .borrowedVariants = CollectionsSurfaces.vectorHelpers.borrowedVariants.views,
    },
    {
        .id = collectionSurfaceId(1),
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::ConstructorFamily,
        .bridgeKey = CollectionsSurfaces.vectorConstructors.bridgeKey,
        .canonicalImportRoot = CollectionsSurfaces.vectorConstructors.canonicalImportRoot,
        .canonicalPath = CollectionsSurfaces.vectorConstructors.canonicalPath,
        .backingTypeName = CollectionsSurfaces.vectorConstructors.backingTypeName,
        .memberNames = CollectionsSurfaces.vectorConstructors.memberNames.views,
        .memberAliases = CollectionsSurfaces.vectorConstructors.memberAliases.views,
        .statementMemberNames = CollectionsSurfaces.vectorConstructors.statementMemberNames.views,
        .importAliasSpellings = CollectionsSurfaces.vectorConstructors.importAliasSpellings.views,
        .compatibilitySpellings = CollectionsSurfaces.vectorConstructors.compatibilitySpellings.views,
        .loweringSpellings = CollectionsSurfaces.vectorConstructors.loweringSpellings.views,
        .borrowedVariants = CollectionsSurfaces.vectorConstructors.borrowedVariants.views,
    },
    {
        .id = collectionSurfaceId(2),
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = CollectionsSurfaces.mapHelpers.bridgeKey,
        .canonicalImportRoot = CollectionsSurfaces.mapHelpers.canonicalImportRoot,
        .canonicalPath = CollectionsSurfaces.mapHelpers.canonicalPath,
        .backingTypeName = CollectionsSurfaces.mapHelpers.backingTypeName,
        .memberNames = CollectionsSurfaces.mapHelpers.memberNames.views,
        .memberAliases = CollectionsSurfaces.mapHelpers.memberAliases.views,
        .statementMemberNames = CollectionsSurfaces.mapHelpers.statementMemberNames.views,
        .importAliasSpellings = CollectionsSurfaces.mapHelpers.importAliasSpellings.views,
        .compatibilitySpellings = CollectionsSurfaces.mapHelpers.compatibilitySpellings.views,
        .loweringSpellings = CollectionsSurfaces.mapHelpers.loweringSpellings.views,
        .borrowedVariants = CollectionsSurfaces.mapHelpers.borrowedVariants.views,
    },
    {
        .id = collectionSurfaceId(3),
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::ConstructorFamily,
        .bridgeKey = CollectionsSurfaces.mapConstructors.bridgeKey,
        .canonicalImportRoot = CollectionsSurfaces.mapConstructors.canonicalImportRoot,
        .canonicalPath = CollectionsSurfaces.mapConstructors.canonicalPath,
        .backingTypeName = CollectionsSurfaces.mapConstructors.backingTypeName,
        .memberNames = CollectionsSurfaces.mapConstructors.memberNames.views,
        .memberAliases = CollectionsSurfaces.mapConstructors.memberAliases.views,
        .statementMemberNames = CollectionsSurfaces.mapConstructors.statementMemberNames.views,
        .importAliasSpellings = CollectionsSurfaces.mapConstructors.importAliasSpellings.views,
        .compatibilitySpellings = CollectionsSurfaces.mapConstructors.compatibilitySpellings.views,
        .loweringSpellings = CollectionsSurfaces.mapConstructors.loweringSpellings.views,
        .borrowedVariants = CollectionsSurfaces.mapConstructors.borrowedVariants.views,
    },
    {
        .id = collectionSurfaceId(4),
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = CollectionsSurfaces.soaHelpers.bridgeKey,
        .canonicalImportRoot = CollectionsSurfaces.soaHelpers.canonicalImportRoot,
        .canonicalPath = CollectionsSurfaces.soaHelpers.canonicalPath,
        .backingTypeName = CollectionsSurfaces.soaHelpers.backingTypeName,
        .memberNames = CollectionsSurfaces.soaHelpers.memberNames.views,
        .memberAliases = CollectionsSurfaces.soaHelpers.memberAliases.views,
        .statementMemberNames = CollectionsSurfaces.soaHelpers.statementMemberNames.views,
        .importAliasSpellings = CollectionsSurfaces.soaHelpers.importAliasSpellings.views,
        .compatibilitySpellings = CollectionsSurfaces.soaHelpers.compatibilitySpellings.views,
        .loweringSpellings = CollectionsSurfaces.soaHelpers.loweringSpellings.views,
        .borrowedVariants = CollectionsSurfaces.soaHelpers.borrowedVariants.views,
    },
    {
        .id = collectionSurfaceId(5),
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::ConstructorFamily,
        .bridgeKey = CollectionsSurfaces.soaConstructors.bridgeKey,
        .canonicalImportRoot = CollectionsSurfaces.soaConstructors.canonicalImportRoot,
        .canonicalPath = CollectionsSurfaces.soaConstructors.canonicalPath,
        .backingTypeName = CollectionsSurfaces.soaConstructors.backingTypeName,
        .memberNames = CollectionsSurfaces.soaConstructors.memberNames.views,
        .memberAliases = CollectionsSurfaces.soaConstructors.memberAliases.views,
        .statementMemberNames = CollectionsSurfaces.soaConstructors.statementMemberNames.views,
        .importAliasSpellings = CollectionsSurfaces.soaConstructors.importAliasSpellings.views,
        .compatibilitySpellings = CollectionsSurfaces.soaConstructors.compatibilitySpellings.views,
        .loweringSpellings = CollectionsSurfaces.soaConstructors.loweringSpellings.views,
        .borrowedVariants = CollectionsSurfaces.soaConstructors.borrowedVariants.views,
    },
    {
        .id = StdlibSurfaceId::CollectionsContainerErrorHelpers,
        .domain = StdlibSurfaceDomain::Collections,
        .shape = StdlibSurfaceShape::ErrorFamily,
        .bridgeKey = "collections.container_error",
        .canonicalImportRoot = "/std/collections",
        .canonicalPath = "/std/collections/ContainerError",
        .backingTypeName = {},
        .memberNames = CollectionsContainerErrorMembers,
        .memberAliases = {},
        .statementMemberNames = {},
        .importAliasSpellings = CollectionsContainerErrorImportAliases,
        .compatibilitySpellings = CollectionsContainerErrorCompatibilitySpellings,
        .loweringSpellings = CollectionsContainerErrorLoweringSpellings,
        .borrowedVariants = {},
    },
    {
        .id = StdlibSurfaceId::GfxBufferHelpers,
        .domain = StdlibSurfaceDomain::Gfx,
        .shape = StdlibSurfaceShape::HelperFamily,
        .bridgeKey = "gfx.buffer_helpers",
        .canonicalImportRoot = "/std/gfx",
        .canonicalPath = "/std/gfx/Buffer",
        .backingTypeName = {},
        .memberNames = GfxBufferHelperMembers,
        .memberAliases = {},
        .statementMemberNames = {},
        .importAliasSpellings = GfxBufferImportAliases,
        .compatibilitySpellings = GfxBufferCompatibilitySpellings,
        .loweringSpellings = GfxBufferLoweringSpellings,
        .borrowedVariants = {},
    },
    {
        .id = StdlibSurfaceId::GfxErrorHelpers,
        .domain = StdlibSurfaceDomain::Gfx,
        .shape = StdlibSurfaceShape::ErrorFamily,
        .bridgeKey = "gfx.gfx_error",
        .canonicalImportRoot = "/std/gfx",
        .canonicalPath = "/std/gfx/GfxError",
        .backingTypeName = {},
        .memberNames = GfxErrorHelperMembers,
        .memberAliases = {},
        .statementMemberNames = {},
        .importAliasSpellings = GfxErrorImportAliases,
        .compatibilitySpellings = GfxErrorCompatibilitySpellings,
        .loweringSpellings = GfxErrorLoweringSpellings,
        .borrowedVariants = {},
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

bool matchesResolvedSurfaceMemberPath(const StdlibSurfaceMetadata &metadata,
                                      std::string_view path) {
  if (stdlibSurfaceMatchesSpelling(metadata, path) ||
      matchesResolvedRootedMemberPath(path, metadata.canonicalPath, metadata.memberNames)) {
    return true;
  }
  return std::any_of(metadata.importAliasSpellings.begin(),
                     metadata.importAliasSpellings.end(),
                     [&](std::string_view aliasSpelling) {
                       return matchesResolvedRootedMemberPath(path, aliasSpelling, metadata.memberNames);
                     });
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

std::string_view resolveSurfaceMemberNameImpl(const StdlibSurfaceMetadata &metadata,
                                              std::string_view memberName) {
  if (metadata.domain == StdlibSurfaceDomain::Collections &&
      metadata.shape != StdlibSurfaceShape::ErrorFamily) {
    return resolveMetadataMemberName(metadata, memberName);
  }
  if (matchesAny(metadata.memberNames, memberName)) {
    return memberName;
  }
  return {};
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

std::string stdlibSurfaceBackingTypePath(const StdlibSurfaceMetadata &metadata) {
  if (metadata.canonicalPath.empty() || metadata.backingTypeName.empty()) {
    return {};
  }
  return std::string(metadata.canonicalPath) + "/" +
         std::string(metadata.backingTypeName);
}

std::string stdlibSurfaceBackingTypePath(StdlibSurfaceId id) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadata(id);
  if (metadata == nullptr) {
    return {};
  }
  return stdlibSurfaceBackingTypePath(*metadata);
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

bool isStdlibSurfaceStatementMemberName(StdlibSurfaceId id, std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(id);
  return metadata != nullptr && matchesAny(metadata->statementMemberNames, memberName);
}

std::string_view findBorrowedVariant(const StdlibSurfaceMetadata &metadata, std::string_view memberName) {
  for (const auto &variant : metadata.borrowedVariants) {
    if (variant.spelling == memberName) {
      return variant.memberName;
    }
  }
  return {};
}

std::string_view findBorrowedVariant(StdlibSurfaceId id, std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(id);
  if (metadata == nullptr) {
    return {};
  }
  return findBorrowedVariant(*metadata, memberName);
}

std::string resolveCompatibilitySpellingToCanonicalPath(std::string_view compatibilitySpelling) {
  const auto *metadata = findStdlibSurfaceMetadataBySpelling(compatibilitySpelling);
  if (metadata == nullptr) {
    return std::string(compatibilitySpelling);
  }
  const std::string_view canonicalPrefix = metadata->canonicalPath;
  const std::string compatStr(compatibilitySpelling);
  const std::string canonicalStr(canonicalPrefix);
  const size_t lastSlash = compatStr.find_last_of('/');
  if (lastSlash != std::string::npos && lastSlash + 1 < compatStr.size()) {
    const std::string_view memberName = std::string_view(compatibilitySpelling).substr(lastSlash + 1);
    return canonicalStr + "/" + std::string(memberName);
  }
  return std::string(compatibilitySpelling);
}

std::string findCompatibilitySpelling(StdlibSurfaceId id, std::string_view memberName) {
  const auto *metadata = findStdlibSurfaceMetadata(id);
  if (metadata == nullptr) {
    return {};
  }
  const std::string canonicalSuffix = "/" + std::string(memberName);
  for (const auto &spelling : metadata->compatibilitySpellings) {
    if (spelling.size() >= canonicalSuffix.size() &&
        spelling.substr(spelling.size() - canonicalSuffix.size()) == canonicalSuffix) {
      return std::string(spelling);
    }
  }
  return {};
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
  if (normalizedPath.find('/') != std::string_view::npos &&
      !matchesResolvedSurfaceMemberPath(metadata, normalizedPath)) {
    return {};
  }
  const std::string_view memberName = pathLeaf(normalizedPath);
  return resolveSurfaceMemberNameImpl(metadata, memberName);
}

} // namespace primec
