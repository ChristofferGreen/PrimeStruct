#include "ImportResolverInternal.h"
#include "ExpandedSourceBuilder.h"

#include "primec/ImportResolver.h"
#include "primec/ProcessRunner.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec {
namespace {

using namespace import_resolver_detail;

struct SourceCursor {
  int line = 1;
  int column = 1;
};

SourceCursor sourceCursorAt(std::string_view source, size_t offset) {
  SourceCursor cursor;
  const size_t limit = std::min(offset, source.size());
  for (size_t i = 0; i < limit; ++i) {
    if (source[i] == '\n') {
      ++cursor.line;
      cursor.column = 1;
    } else {
      ++cursor.column;
    }
  }
  return cursor;
}

void appendSourceSlice(ExpandedSourceBuilder &builder,
                       std::size_t unitId,
                       const std::string &source,
                       size_t start,
                       size_t end) {
  if (start >= end || start >= source.size()) {
    return;
  }
  end = std::min(end, source.size());
  const SourceCursor cursor = sourceCursorAt(source, start);
  builder.appendSegment(unitId,
                        std::string_view(source.data() + start, end - start),
                        cursor.line,
                        cursor.column);
}

struct ImportEntry {
  std::string path;
  bool isLogical = false;
};

bool expandImportsInternal(const std::string &baseDir,
                           const std::string &source,
                           std::unordered_set<std::string> &expanded,
                           std::string &error,
                           const std::vector<std::filesystem::path> &importRoots,
                           ExpandedSourceBuilder &builder,
                           std::size_t sourceUnitId) {
  size_t copyStart = 0;
  for (size_t i = 0; i < source.size();) {
    if (source[i] == '"' || source[i] == '\'') {
      i = skipQuotedLiteral(source, i);
      continue;
    }
    if (source[i] == '/' && i + 1 < source.size()) {
      if (source[i + 1] == '/') {
        i = skipLineComment(source, i);
        continue;
      }
      if (source[i + 1] == '*') {
        i = skipBlockComment(source, i);
        continue;
      }
    }
    size_t payloadStart = 0;
    size_t end = 0;
    if (scanLegacyIncludeDirective(source, i, payloadStart, end)) {
      if (end == std::string::npos) {
        error = "unterminated include<...> directive";
      } else {
        error = "legacy include<...> is no longer supported; use import<...>";
      }
      return false;
    }
    if (!scanIncludeDirective(source, i, payloadStart, end)) {
      ++i;
      continue;
    }
    if (end == std::string::npos) {
      error = "unterminated import<...> directive";
      return false;
    }

    appendSourceSlice(builder, sourceUnitId, source, copyStart, i);

    std::string payload = source.substr(payloadStart, end - payloadStart);
    std::vector<ImportEntry> paths;
    std::optional<std::string> versionTag;
    size_t pos = 0;
    auto skipWhitespace = [&]() {
      pos = skipWhitespaceAndComments(payload, pos);
    };
    while (true) {
      skipWhitespace();
      if (pos >= payload.size()) {
        break;
      }
      if (payload[pos] == ',' || payload[pos] == ';') {
        ++pos;
        continue;
      }
      size_t entryPos = pos;
      if (tryConsumeIncludeKeyword(payload, entryPos, "version")) {
        pos = entryPos;
        skipWhitespace();
        if (pos >= payload.size() || payload[pos] != '=') {
          error = "expected '=' after import version";
          return false;
        }
        ++pos;
        skipWhitespace();
        if (versionTag) {
          error = "duplicate version attribute in import<...>";
          return false;
        }
        std::string versionValue;
        std::string parseError;
        if (!readQuotedString(payload, pos, versionValue, parseError)) {
          error = parseError;
          return false;
        }
        versionTag = trim(versionValue);
        if (pos < payload.size()) {
          if (!std::isspace(static_cast<unsigned char>(payload[pos])) && payload[pos] != ',' &&
              payload[pos] != ';') {
            error = "unexpected characters after import version";
            return false;
          }
        }
      } else if (payload[pos] == '"' || payload[pos] == '\'') {
        std::string path;
        std::string parseError;
        if (!readQuotedString(payload, pos, path, parseError)) {
          error = parseError;
          return false;
        }
        paths.push_back({trim(path), false});
        if (pos < payload.size() && !std::isspace(static_cast<unsigned char>(payload[pos])) &&
            payload[pos] != ',' && payload[pos] != ';') {
          error = "import path cannot have suffix";
          return false;
        }
      } else {
        std::string path;
        if (!readBareIncludePath(payload, pos, path)) {
          error = "invalid import entry in import<...>";
          return false;
        }
        path = trim(path);
        if (path.empty() || path.front() != '/') {
          error = "unquoted import paths must be slash paths";
          return false;
        }
        if (!validateSlashPath(path, error)) {
          return false;
        }
        paths.push_back({path, true});
      }
      skipWhitespace();
    }

    if (paths.empty()) {
      error = "import<...> requires at least one path";
      return false;
    }

    std::optional<std::vector<int>> requestedVersion;
    if (versionTag) {
      std::vector<int> parsed;
      if (!parseVersionParts(*versionTag, parsed, error)) {
        return false;
      }
      requestedVersion = std::move(parsed);
    }

    auto resolveIncludePath = [&](const ImportEntry &entry,
                                  std::filesystem::path &resolved,
                                  std::optional<std::string> &resolvedVersion) -> bool {
      const std::string &path = entry.path;
      const bool isLogical = entry.isLogical;
      std::filesystem::path requested(path);
      resolvedVersion.reset();
      bool isAbsolute = requested.is_absolute();
      std::filesystem::path logicalPath = isAbsolute ? requested.relative_path() : requested;
      if (requestedVersion) {
        std::vector<std::filesystem::path> roots;
        if (!isLogical) {
          if (isAbsolute) {
            roots = importRoots;
          } else {
            roots.push_back(std::filesystem::path(baseDir));
            for (const auto &root : importRoots) {
              roots.push_back(root);
            }
          }
          if (roots.empty()) {
            roots.push_back(std::filesystem::path(baseDir));
          }
        } else {
          roots = importRoots;
          if (roots.empty()) {
            std::ostringstream requestedText;
            for (size_t part = 0; part < requestedVersion->size(); ++part) {
              if (part > 0) {
                requestedText << ".";
              }
              requestedText << (*requestedVersion)[part];
            }
            error = "import version not found: " + requestedText.str();
            return false;
          }
        }
        std::string versionError;
        bool foundVersion = false;
        bool foundCandidate = false;
        std::filesystem::path lastCandidate;
        std::filesystem::path bestCandidate;
        std::vector<int> bestParts;
        std::string bestVersion;
        for (const auto &root : roots) {
          std::string selected;
          std::string rootError;
          if (!selectVersionDirectory(root, *requestedVersion, selected, rootError)) {
            versionError = rootError;
            continue;
          }
          foundVersion = true;
          std::filesystem::path candidate = root / selected / logicalPath;
          if (std::filesystem::exists(candidate)) {
            if (isPrivatePath(candidate)) {
              error = "import path refers to private folder: " +
                      std::filesystem::absolute(candidate).string();
              return false;
            }
            std::vector<int> candidateParts;
            std::string parseError;
            if (!parseVersionParts(selected, candidateParts, parseError)) {
              error = "invalid import version: " + selected;
              return false;
            }
            if (!foundCandidate || isNewerVersion(candidateParts, bestParts)) {
              bestParts = std::move(candidateParts);
              bestCandidate = candidate;
              bestVersion = selected;
            }
            foundCandidate = true;
            continue;
          }
          lastCandidate = candidate;
        }
        if (!foundVersion) {
          error = versionError.empty() ? "import version not found" : versionError;
          return false;
        }
        if (!foundCandidate) {
          if (!lastCandidate.empty()) {
            error = "failed to read import: " + std::filesystem::absolute(lastCandidate).string();
          } else {
            error = "failed to read import: " + logicalPath.string();
          }
          return false;
        }
        resolved = std::filesystem::absolute(bestCandidate);
        resolvedVersion = bestVersion;
        return true;
      }

      if (isLogical) {
        for (const auto &root : importRoots) {
          std::filesystem::path candidate = root / logicalPath;
          if (std::filesystem::exists(candidate)) {
            if (isPrivatePath(candidate)) {
              error = "import path refers to private folder: " +
                      std::filesystem::absolute(candidate).string();
              return false;
            }
            resolved = std::filesystem::absolute(candidate);
            return true;
          }
        }
        error = "failed to read import: " + path;
        return false;
      }
      if (!isAbsolute) {
        std::filesystem::path candidate = std::filesystem::path(baseDir) / requested;
        if (std::filesystem::exists(candidate)) {
          if (isPrivatePath(candidate)) {
            error = "import path refers to private folder: " +
                    std::filesystem::absolute(candidate).string();
            return false;
          }
          resolved = std::filesystem::absolute(candidate);
          return true;
        }
        for (const auto &root : importRoots) {
          candidate = root / requested;
          if (std::filesystem::exists(candidate)) {
            if (isPrivatePath(candidate)) {
              error = "import path refers to private folder: " +
                      std::filesystem::absolute(candidate).string();
              return false;
            }
            resolved = std::filesystem::absolute(candidate);
            return true;
          }
        }
        error = "failed to read import: " +
                std::filesystem::absolute(std::filesystem::path(baseDir) / requested).string();
        return false;
      }

      if (std::filesystem::exists(requested)) {
        if (isPrivatePath(requested)) {
          error = "import path refers to private folder: " +
                  std::filesystem::absolute(requested).string();
          return false;
        }
        resolved = std::filesystem::absolute(requested);
        return true;
      }
      for (const auto &root : importRoots) {
        std::filesystem::path candidate = root / logicalPath;
        if (std::filesystem::exists(candidate)) {
          if (isPrivatePath(candidate)) {
            error = "import path refers to private folder: " +
                    std::filesystem::absolute(candidate).string();
            return false;
          }
          resolved = std::filesystem::absolute(candidate);
          return true;
        }
      }
      error = "failed to read import: " + std::filesystem::absolute(requested).string();
      return false;
    };

    std::optional<std::string> selectedVersion;
    std::vector<std::filesystem::path> allImportFiles;
    for (const auto &path : paths) {
      std::filesystem::path resolved;
      std::optional<std::string> resolvedVersion;
      if (!resolveIncludePath(path, resolved, resolvedVersion)) {
        return false;
      }
      if (requestedVersion) {
        if (!resolvedVersion.has_value()) {
          error = "import version not resolved";
          return false;
        }
        if (!selectedVersion) {
          selectedVersion = *resolvedVersion;
        } else if (*selectedVersion != *resolvedVersion) {
          error = "import version mismatch: expected " + *selectedVersion +
                  " but got " + *resolvedVersion;
          return false;
        }
      }
      std::vector<std::filesystem::path> importFiles;
      if (!collectPrimeFiles(resolved, importFiles, error)) {
        return false;
      }
      allImportFiles.insert(allImportFiles.end(), importFiles.begin(), importFiles.end());
    }
    for (const auto &importFile : allImportFiles) {
      const std::string expandedKey = normalizePathKey(importFile);
      if (expanded.count(expandedKey) > 0) {
        continue;
      }
      std::string resolvedText = importFile.string();
      std::string included;
      if (!readFile(resolvedText, included)) {
        error = "failed to read import: " + resolvedText;
        return false;
      }
      expanded.insert(expandedKey);
      const std::size_t importedUnitId =
          builder.addUnit(SourceUnitKind::Import, std::filesystem::absolute(importFile).string(), {}, 1, 1);
      const size_t beforeSize = builder.textSize();
      if (!expandImportsInternal(importFile.parent_path().string(),
                                 included,
                                 expanded,
                                 error,
                                 importRoots,
                                 builder,
                                 importedUnitId)) {
        return false;
      }
      const size_t afterSize = builder.textSize();
      if (afterSize > beforeSize && !builder.textEndsWithNewline()) {
        builder.appendGenerated("\n", "<import-separator>");
      }
    }
    i = end + 1;
    copyStart = i;
  }
  appendSourceSlice(builder, sourceUnitId, source, copyStart, source.size());
  return true;
}

} // namespace

ImportResolver::ImportResolver(const ProcessRunner *processRunner)
    : processRunner_(processRunner != nullptr ? processRunner : &systemProcessRunner()) {}

bool ImportResolver::expandImports(const std::string &inputPath,
                                     std::string &source,
                                     std::string &error,
                                     const std::vector<std::string> &importPaths) {
  ExpandedSource expandedSource;
  if (!expandImports(inputPath, expandedSource, error, importPaths)) {
    return false;
  }
  source = std::move(expandedSource.text);
  return true;
}

bool ImportResolver::expandImports(const std::string &inputPath,
                                     ExpandedSource &expandedSource,
                                     std::string &error,
                                     const std::vector<std::string> &importPaths) {
  expandedSource = {};
  std::filesystem::path input = std::filesystem::absolute(inputPath);
  std::string content;
  if (!readFile(input.string(), content)) {
    error = "failed to read input: " + input.string();
    return false;
  }
  std::string baseDir = input.parent_path().string();
  std::vector<std::filesystem::path> importRoots;
  importRoots.reserve(importPaths.size());
  for (const auto &path : importPaths) {
    if (path.empty()) {
      continue;
    }
    importRoots.push_back(std::filesystem::absolute(path));
  }
  std::vector<std::filesystem::path> expandedRoots;
  if (!appendArchiveRoots(importRoots, *processRunner_, expandedRoots, error)) {
    return false;
  }
  ExpandedSourceBuilder builder(expandedSource);
  const std::size_t primaryUnitId =
      builder.addUnit(SourceUnitKind::Primary, input.string(), {}, 1, 1);
  std::unordered_set<std::string> expanded;
  return expandImportsInternal(baseDir, content, expanded, error, expandedRoots, builder, primaryUnitId);
}

} // namespace primec
