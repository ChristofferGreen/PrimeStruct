#include "ImportResolverInternal.h"

#include "primec/ImportResolver.h"
#include "primec/ProcessRunner.h"

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace primec {
namespace {

using namespace import_resolver_detail;

} // namespace

ImportResolver::ImportResolver(const ProcessRunner *processRunner)
    : processRunner_(processRunner != nullptr ? processRunner : &systemProcessRunner()) {}

bool ImportResolver::expandImports(const std::string &inputPath,
                                     std::string &source,
                                     std::string &error,
                                     const std::vector<std::string> &importPaths) {
  std::filesystem::path input = std::filesystem::absolute(inputPath);
  std::string content;
  if (!readFile(input.string(), content)) {
    error = "failed to read input: " + input.string();
    return false;
  }
  source = std::move(content);
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
  std::unordered_set<std::string> expanded;
  return expandImportsInternal(baseDir, source, expanded, error, expandedRoots);
}

bool ImportResolver::expandImportsInternal(const std::string &baseDir,
                                             std::string &source,
                                             std::unordered_set<std::string> &expanded,
                                             std::string &error,
                                             const std::vector<std::filesystem::path> &importRoots) {
  bool changed = true;

  while (changed) {
    changed = false;
    std::string result;
    for (size_t i = 0; i < source.size();) {
      if (source[i] == '"' || source[i] == '\'') {
        size_t end = skipQuotedLiteral(source, i);
        result.append(source.substr(i, end - i));
        i = end;
        continue;
      }
      if (source[i] == '/' && i + 1 < source.size()) {
        if (source[i + 1] == '/') {
          size_t end = skipLineComment(source, i);
          result.append(source.substr(i, end - i));
          i = end;
          continue;
        }
        if (source[i + 1] == '*') {
          size_t end = skipBlockComment(source, i);
          result.append(source.substr(i, end - i));
          i = end;
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
      if (scanIncludeDirective(source, i, payloadStart, end)) {
        if (end == std::string::npos) {
          error = "unterminated import<...> directive";
          return false;
        }
        std::string payload = source.substr(payloadStart, end - payloadStart);
        struct IncludeEntry {
          std::string path;
          bool isLogical = false;
        };
        std::vector<IncludeEntry> paths;
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

        auto resolveIncludePath = [&](const IncludeEntry &entry,
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
                for (size_t i = 0; i < requestedVersion->size(); ++i) {
                  if (i > 0) {
                    requestedText << ".";
                  }
                  requestedText << (*requestedVersion)[i];
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
                  error = "import path refers to private folder: " + std::filesystem::absolute(candidate).string();
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
                  error = "import path refers to private folder: " + std::filesystem::absolute(candidate).string();
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
                error = "import path refers to private folder: " + std::filesystem::absolute(candidate).string();
                return false;
              }
              resolved = std::filesystem::absolute(candidate);
              return true;
            }
            for (const auto &root : importRoots) {
              candidate = root / requested;
              if (std::filesystem::exists(candidate)) {
                if (isPrivatePath(candidate)) {
                  error = "import path refers to private folder: " + std::filesystem::absolute(candidate).string();
                  return false;
                }
                resolved = std::filesystem::absolute(candidate);
                return true;
              }
            }
            error = "failed to read import: " + std::filesystem::absolute(std::filesystem::path(baseDir) / requested).string();
            return false;
          }

          if (std::filesystem::exists(requested)) {
            if (isPrivatePath(requested)) {
              error = "import path refers to private folder: " + std::filesystem::absolute(requested).string();
              return false;
            }
            resolved = std::filesystem::absolute(requested);
            return true;
          }
          for (const auto &root : importRoots) {
            std::filesystem::path candidate = root / logicalPath;
            if (std::filesystem::exists(candidate)) {
              if (isPrivatePath(candidate)) {
                error = "import path refers to private folder: " + std::filesystem::absolute(candidate).string();
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
              error = "import version mismatch: expected " + *selectedVersion + " but got " + *resolvedVersion;
              return false;
            }
          }
          std::vector<std::filesystem::path> includeFiles;
          if (!collectPrimeFiles(resolved, includeFiles, error)) {
            return false;
          }
          allImportFiles.insert(allImportFiles.end(), includeFiles.begin(), includeFiles.end());
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
          if (!expandImportsInternal(importFile.parent_path().string(), included, expanded, error, importRoots)) {
            return false;
          }
          result.append(included);
          if (!included.empty() && included.back() != '\n') {
            result.push_back('\n');
          }
        }
        i = end + 1;
        changed = true;
        continue;
      }
      result.push_back(source[i]);
      ++i;
    }
    source.swap(result);
  }
  return true;
}

} // namespace primec
