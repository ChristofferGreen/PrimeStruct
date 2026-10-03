#include "primec/support/StdlibSurfaceRegistry.h"

#include "StdlibSurfaceScan.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace primec::stdlib_surface_scan {

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



// Walks up from a starting directory looking for stdlib/std/collections.
// Shared so file discovery has exactly one "where is the collections
// directory" implementation.
std::optional<std::filesystem::path> findStdlibCollectionsDirectory() {
  const std::filesystem::path relativeDir =
      std::filesystem::path("stdlib") / "std" / "collections";
  auto findFromRoot = [&](std::filesystem::path root) -> std::optional<std::filesystem::path> {
    std::error_code ec;
    for (std::size_t depth = 0; depth < 8 && !root.empty(); ++depth) {
      const std::filesystem::path candidate = root / relativeDir;
      if (std::filesystem::is_directory(candidate, ec) && !ec) {
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

// Enumerates every *.prime file directly under stdlib/std/collections/
// (non-recursive: subdirectories like soa_storage-adjacent internals are
// not collection-surface candidates). Sorted for deterministic output
// (AGENTS.md: no unordered iteration affecting output).
std::vector<std::filesystem::path> listStdlibCollectionFiles() {
  std::vector<std::filesystem::path> files;
  const auto directory = findStdlibCollectionsDirectory();
  if (!directory.has_value()) {
    return files;
  }
  std::error_code ec;
  for (const auto &entry : std::filesystem::directory_iterator(*directory, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file() || entry.path().extension() != ".prime") {
      continue;
    }
    files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());
  return files;
}

// Returns the leaf name from a rooted path (e.g. "/std/collections/soa/count" → "count").
// Returns the whole string if there is no slash.
std::string_view pathLeafOf(std::string_view path) {
  const auto pos = path.rfind('/');
  return pos == std::string_view::npos ? path : path.substr(pos + 1);
}

// Trim leading whitespace and return the indentation depth (number of leading spaces).
std::size_t leadingSpaces(std::string_view line) {
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
std::string firstParamType(std::string_view signatureBuffer) {
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
std::string extractFunctionName(std::string_view line) {
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
std::vector<ScannedFunctionRecord> scanStdlibPublicFunctions(
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
ManifestSurfaceData buildSurfaceData(
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

} // namespace primec::stdlib_surface_scan
