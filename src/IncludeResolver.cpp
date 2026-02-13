#include "primec/IncludeResolver.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace primec {
namespace {

bool isIncludeBoundaryChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '/';
}

bool isReservedKeyword(const std::string &text) {
  return text == "mut" || text == "return" || text == "include" || text == "import" || text == "namespace" ||
         text == "true" || text == "false";
}

bool isAsciiAlpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isAsciiDigit(char c) {
  return c >= '0' && c <= '9';
}

bool isIdentifierSegmentStart(char c) {
  return isAsciiAlpha(c) || c == '_';
}

bool isIdentifierSegmentChar(char c) {
  return isAsciiAlpha(c) || isAsciiDigit(c) || c == '_';
}

bool isIncludeSegmentChar(char c) {
  return isIdentifierSegmentChar(c);
}

bool validateSlashPath(const std::string &text, std::string &error) {
  if (text.size() < 2 || text[0] != '/') {
    error = "invalid slash path identifier: " + text;
    return false;
  }
  size_t start = 1;
  while (start < text.size()) {
    size_t end = text.find('/', start);
    std::string segment = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (segment.empty() || !isIdentifierSegmentStart(segment[0])) {
      error = "invalid slash path identifier: " + text;
      return false;
    }
    for (size_t i = 1; i < segment.size(); ++i) {
      if (!isIncludeSegmentChar(segment[i])) {
        error = "invalid slash path identifier: " + text;
        return false;
      }
    }
    if (isReservedKeyword(segment)) {
      error = "reserved keyword cannot be used as identifier: " + segment;
      return false;
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  if (text.back() == '/') {
    error = "invalid slash path identifier: " + text;
    return false;
  }
  return true;
}

bool readFile(const std::string &path, std::string &out) {
  std::ifstream file(path);
  if (!file) {
    return false;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  out = buffer.str();
  return true;
}

std::string trim(const std::string &value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

size_t skipQuotedLiteral(const std::string &text, size_t start) {
  if (start >= text.size()) {
    return start;
  }
  char quote = text[start];
  size_t pos = start + 1;
  while (pos < text.size()) {
    char c = text[pos];
    if (c == '\\' && pos + 1 < text.size()) {
      pos += 2;
      continue;
    }
    if (c == quote) {
      return pos + 1;
    }
    ++pos;
  }
  return text.size();
}

size_t skipLineComment(const std::string &text, size_t start) {
  size_t pos = start + 2;
  while (pos < text.size() && text[pos] != '\n') {
    ++pos;
  }
  return pos;
}

size_t skipBlockComment(const std::string &text, size_t start) {
  const size_t end = text.find("*/", start + 2);
  if (end == std::string::npos) {
    return text.size();
  }
  return end + 2;
}

size_t skipWhitespaceAndComments(const std::string &text, size_t start) {
  size_t pos = start;
  while (pos < text.size()) {
    bool advanced = false;
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
      ++pos;
      advanced = true;
    }
    if (pos + 1 < text.size() && text[pos] == '/') {
      if (text[pos + 1] == '/') {
        pos = skipLineComment(text, pos);
        advanced = true;
      } else if (text[pos + 1] == '*') {
        pos = skipBlockComment(text, pos);
        advanced = true;
      }
    }
    if (!advanced) {
      break;
    }
  }
  return pos;
}

bool scanIncludeDirective(const std::string &source, size_t pos, size_t &payloadStart, size_t &payloadEnd) {
  if (pos + 7 > source.size()) {
    return false;
  }
  if (source.compare(pos, 7, "include") != 0) {
    return false;
  }
  if (pos > 0 && isIncludeBoundaryChar(source[pos - 1])) {
    return false;
  }
  size_t scan = pos + 7;
  if (scan < source.size() && isIncludeBoundaryChar(source[scan])) {
    return false;
  }
  scan = skipWhitespaceAndComments(source, scan);
  if (scan >= source.size() || source[scan] != '<') {
    return false;
  }
  payloadStart = scan + 1;
  payloadEnd = source.find('>', payloadStart);
  return true;
}

bool readQuotedString(const std::string &payload, size_t &pos, std::string &out, std::string &error) {
  if (pos >= payload.size() || (payload[pos] != '"' && payload[pos] != '\'')) {
    error = "expected quoted string in include<...>";
    return false;
  }
  char quote = payload[pos];
  ++pos;
  size_t quoteEnd = payload.find(quote, pos);
  if (quoteEnd == std::string::npos) {
    error = "unterminated include string literal";
    return false;
  }
  out = payload.substr(pos, quoteEnd - pos);
  pos = quoteEnd + 1;
  return true;
}

bool readBareIncludePath(const std::string &payload, size_t &pos, std::string &out) {
  size_t start = pos;
  while (pos < payload.size()) {
    char c = payload[pos];
    if (std::isspace(static_cast<unsigned char>(c)) || c == ',') {
      break;
    }
    ++pos;
  }
  if (pos <= start) {
    return false;
  }
  out = payload.substr(start, pos - start);
  return true;
}

bool tryConsumeIncludeKeyword(const std::string &payload, size_t &pos, const std::string &keyword) {
  if (payload.compare(pos, keyword.size(), keyword) != 0) {
    return false;
  }
  size_t end = pos + keyword.size();
  if (end < payload.size()) {
    char c = payload[end];
    if (isIncludeBoundaryChar(c)) {
      return false;
    }
  }
  pos = end;
  return true;
}

std::string quoteShellArg(const std::string &value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

bool extractArchive(const std::filesystem::path &archive,
                    std::filesystem::path &outDir,
                    std::string &error) {
  std::string absPath = std::filesystem::absolute(archive).string();
  std::size_t hash = std::hash<std::string>{}(absPath);
  outDir = std::filesystem::temp_directory_path() / "primec_archives" / std::to_string(hash);
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);
  if (ec) {
    error = "failed to create archive cache directory: " + outDir.string();
    return false;
  }
  std::string command =
      "unzip -q -o " + quoteShellArg(absPath) + " -d " + quoteShellArg(outDir.string());
  if (std::system(command.c_str()) != 0) {
    error = "failed to extract archive: " + absPath;
    return false;
  }
  return true;
}

bool appendArchiveRoots(const std::vector<std::filesystem::path> &roots,
                        std::vector<std::filesystem::path> &expanded,
                        std::string &error) {
  expanded = roots;
  for (const auto &root : roots) {
    if (!std::filesystem::exists(root)) {
      continue;
    }
    if (std::filesystem::is_regular_file(root) && root.extension() == ".zip") {
      std::filesystem::path extracted;
      if (!extractArchive(root, extracted, error)) {
        return false;
      }
      expanded.push_back(std::move(extracted));
      continue;
    }
    if (!std::filesystem::is_directory(root)) {
      continue;
    }
    for (const auto &entry : std::filesystem::directory_iterator(root)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      if (entry.path().extension() != ".zip") {
        continue;
      }
      std::filesystem::path extracted;
      if (!extractArchive(entry.path(), extracted, error)) {
        return false;
      }
      expanded.push_back(std::move(extracted));
    }
  }
  return true;
}

bool parseVersionParts(const std::string &text, std::vector<int> &parts, std::string &error) {
  parts.clear();
  if (text.empty()) {
    error = "include version cannot be empty";
    return false;
  }
  int current = 0;
  bool hasDigit = false;
  for (size_t i = 0; i <= text.size(); ++i) {
    if (i == text.size() || text[i] == '.') {
      if (!hasDigit) {
        error = "invalid include version: " + text;
        return false;
      }
      parts.push_back(current);
      current = 0;
      hasDigit = false;
      continue;
    }
    if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
      error = "invalid include version: " + text;
      return false;
    }
    hasDigit = true;
    current = current * 10 + (text[i] - '0');
  }
  if (parts.size() < 1 || parts.size() > 3) {
    error = "include version must have 1 to 3 numeric parts: " + text;
    return false;
  }
  return true;
}

bool hasPrefix(const std::vector<int> &candidate, const std::vector<int> &prefix) {
  if (candidate.size() < prefix.size()) {
    return false;
  }
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (candidate[i] != prefix[i]) {
      return false;
    }
  }
  return true;
}

bool isNewerVersion(const std::vector<int> &lhs, const std::vector<int> &rhs) {
  size_t maxSize = std::max(lhs.size(), rhs.size());
  for (size_t i = 0; i < maxSize; ++i) {
    int left = i < lhs.size() ? lhs[i] : 0;
    int right = i < rhs.size() ? rhs[i] : 0;
    if (left != right) {
      return left > right;
    }
  }
  return false;
}

bool isPrivatePath(const std::filesystem::path &path) {
  std::error_code ec;
  const bool isDir = std::filesystem::is_directory(path, ec);
  std::filesystem::path scanPath = isDir ? path : path.parent_path();
  for (const auto &part : scanPath) {
    std::string segment = part.string();
    if (segment.empty()) {
      continue;
    }
    if (segment == "." || segment == "..") {
      continue;
    }
    if (!segment.empty() && segment[0] == '_') {
      return true;
    }
  }
  return false;
}

bool collectPrimeFiles(const std::filesystem::path &root,
                       std::vector<std::filesystem::path> &out,
                       std::string &error) {
  out.clear();
  if (isPrivatePath(root)) {
    error = "include path refers to private folder: " + std::filesystem::absolute(root).string();
    return false;
  }
  if (std::filesystem::is_regular_file(root)) {
    out.push_back(root);
    return true;
  }
  if (!std::filesystem::is_directory(root)) {
    error = "failed to read include: " + std::filesystem::absolute(root).string();
    return false;
  }
  std::error_code ec;
  std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied,
                                                   ec);
  std::filesystem::recursive_directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      error = "failed to read include: " + std::filesystem::absolute(root).string();
      return false;
    }
    const std::filesystem::path current = it->path();
    if (it->is_directory()) {
      if (isPrivatePath(current)) {
        it.disable_recursion_pending();
      }
      continue;
    }
    if (!it->is_regular_file()) {
      continue;
    }
    if (current.extension() != ".prime") {
      continue;
    }
    if (isPrivatePath(current)) {
      continue;
    }
    out.push_back(current);
  }
  if (out.empty()) {
    error = "include directory contains no .prime files: " + std::filesystem::absolute(root).string();
    return false;
  }
  std::sort(out.begin(), out.end());
  return true;
}

bool selectVersionDirectory(const std::filesystem::path &baseDir,
                            const std::vector<int> &requested,
                            std::string &selected,
                            std::string &error) {
  if (requested.size() == 3) {
    std::ostringstream exact;
    exact << requested[0] << "." << requested[1] << "." << requested[2];
    std::filesystem::path candidate = baseDir / exact.str();
    if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate)) {
      selected = exact.str();
      return true;
    }
    error = "include version not found: " + exact.str();
    return false;
  }

  if (!std::filesystem::exists(baseDir) || !std::filesystem::is_directory(baseDir)) {
    std::ostringstream requestedText;
    for (size_t i = 0; i < requested.size(); ++i) {
      if (i > 0) {
        requestedText << ".";
      }
      requestedText << requested[i];
    }
    error = "include version not found: " + requestedText.str();
    return false;
  }

  bool found = false;
  std::vector<int> bestParts;
  std::string bestName;
  for (const auto &entry : std::filesystem::directory_iterator(baseDir)) {
    if (!entry.is_directory()) {
      continue;
    }
    std::string name = entry.path().filename().string();
    std::vector<int> parts;
    std::string parseError;
    if (!parseVersionParts(name, parts, parseError)) {
      continue;
    }
    if (!hasPrefix(parts, requested)) {
      continue;
    }
    if (!found || isNewerVersion(parts, bestParts)) {
      bestParts = parts;
      bestName = name;
      found = true;
    }
  }
  if (!found) {
    std::ostringstream requestedText;
    for (size_t i = 0; i < requested.size(); ++i) {
      if (i > 0) {
        requestedText << ".";
      }
      requestedText << requested[i];
    }
    error = "include version not found: " + requestedText.str();
    return false;
  }
  selected = bestName;
  return true;
}

} // namespace

bool IncludeResolver::expandIncludes(const std::string &inputPath,
                                     std::string &source,
                                     std::string &error,
                                     const std::vector<std::string> &includePaths) {
  std::filesystem::path input = std::filesystem::absolute(inputPath);
  std::string content;
  if (!readFile(input.string(), content)) {
    error = "failed to read input: " + input.string();
    return false;
  }
  source = std::move(content);
  std::string baseDir = input.parent_path().string();
  std::vector<std::filesystem::path> includeRoots;
  includeRoots.reserve(includePaths.size());
  for (const auto &path : includePaths) {
    if (path.empty()) {
      continue;
    }
    includeRoots.push_back(std::filesystem::absolute(path));
  }
  std::vector<std::filesystem::path> expandedRoots;
  if (!appendArchiveRoots(includeRoots, expandedRoots, error)) {
    return false;
  }
  std::unordered_set<std::string> expanded;
  return expandIncludesInternal(baseDir, source, expanded, error, expandedRoots);
}

bool IncludeResolver::expandIncludesInternal(const std::string &baseDir,
                                             std::string &source,
                                             std::unordered_set<std::string> &expanded,
                                             std::string &error,
                                             const std::vector<std::filesystem::path> &includeRoots) {
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
      if (scanIncludeDirective(source, i, payloadStart, end)) {
        if (end == std::string::npos) {
          error = "unterminated include<...> directive";
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
          if (payload[pos] == ';') {
            error = "semicolon is not allowed in include<...>";
            return false;
          }
          size_t entryPos = pos;
          if (tryConsumeIncludeKeyword(payload, entryPos, "version")) {
            pos = entryPos;
            skipWhitespace();
            if (pos >= payload.size() || payload[pos] != '=') {
              error = "expected '=' after include version";
              return false;
            }
            ++pos;
            skipWhitespace();
            if (versionTag) {
              error = "duplicate version attribute in include<...>";
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
              if (payload[pos] == ';') {
                error = "semicolon is not allowed in include<...>";
                return false;
              }
              if (!std::isspace(static_cast<unsigned char>(payload[pos])) && payload[pos] != ',') {
                error = "unexpected characters after include version";
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
            if (pos < payload.size() && payload[pos] == ';') {
              error = "semicolon is not allowed in include<...>";
              return false;
            }
            if (pos < payload.size() && !std::isspace(static_cast<unsigned char>(payload[pos])) &&
                payload[pos] != ',') {
              error = "include path cannot have suffix";
              return false;
            }
          } else {
            std::string path;
            if (!readBareIncludePath(payload, pos, path)) {
              error = "invalid include entry in include<...>";
              return false;
            }
            path = trim(path);
            if (path.empty() || path.front() != '/') {
              error = "unquoted include paths must be slash paths";
              return false;
            }
            if (!validateSlashPath(path, error)) {
              return false;
            }
            paths.push_back({path, true});
          }
          skipWhitespace();
          if (pos >= payload.size()) {
            break;
          }
          if (payload[pos] == ';') {
            error = "semicolon is not allowed in include<...>";
            return false;
          }
          if (payload[pos] != ',') {
            error = "expected ',' between include entries";
            return false;
          }
          ++pos;
        }

        if (paths.empty()) {
          error = "include<...> requires at least one path";
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
                roots = includeRoots;
              } else {
                roots.push_back(std::filesystem::path(baseDir));
                for (const auto &root : includeRoots) {
                  roots.push_back(root);
                }
              }
              if (roots.empty()) {
                roots.push_back(std::filesystem::path(baseDir));
              }
            } else {
              roots = includeRoots;
              if (roots.empty()) {
                std::ostringstream requestedText;
                for (size_t i = 0; i < requestedVersion->size(); ++i) {
                  if (i > 0) {
                    requestedText << ".";
                  }
                  requestedText << (*requestedVersion)[i];
                }
                error = "include version not found: " + requestedText.str();
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
                  error = "include path refers to private folder: " + std::filesystem::absolute(candidate).string();
                  return false;
                }
                std::vector<int> candidateParts;
                std::string parseError;
                if (!parseVersionParts(selected, candidateParts, parseError)) {
                  error = "invalid include version: " + selected;
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
              error = versionError.empty() ? "include version not found" : versionError;
              return false;
            }
            if (!foundCandidate) {
              if (!lastCandidate.empty()) {
                error = "failed to read include: " + std::filesystem::absolute(lastCandidate).string();
              } else {
                error = "failed to read include: " + logicalPath.string();
              }
              return false;
            }
            resolved = std::filesystem::absolute(bestCandidate);
            resolvedVersion = bestVersion;
            return true;
          }

          if (isLogical) {
            for (const auto &root : includeRoots) {
              std::filesystem::path candidate = root / logicalPath;
              if (std::filesystem::exists(candidate)) {
                if (isPrivatePath(candidate)) {
                  error = "include path refers to private folder: " + std::filesystem::absolute(candidate).string();
                  return false;
                }
                resolved = std::filesystem::absolute(candidate);
                return true;
              }
            }
            error = "failed to read include: " + path;
            return false;
          }
          if (!isAbsolute) {
            std::filesystem::path candidate = std::filesystem::path(baseDir) / requested;
            if (std::filesystem::exists(candidate)) {
              if (isPrivatePath(candidate)) {
                error = "include path refers to private folder: " + std::filesystem::absolute(candidate).string();
                return false;
              }
              resolved = std::filesystem::absolute(candidate);
              return true;
            }
            for (const auto &root : includeRoots) {
              candidate = root / requested;
              if (std::filesystem::exists(candidate)) {
                if (isPrivatePath(candidate)) {
                  error = "include path refers to private folder: " + std::filesystem::absolute(candidate).string();
                  return false;
                }
                resolved = std::filesystem::absolute(candidate);
                return true;
              }
            }
            error = "failed to read include: " + std::filesystem::absolute(std::filesystem::path(baseDir) / requested).string();
            return false;
          }

          if (std::filesystem::exists(requested)) {
            if (isPrivatePath(requested)) {
              error = "include path refers to private folder: " + std::filesystem::absolute(requested).string();
              return false;
            }
            resolved = std::filesystem::absolute(requested);
            return true;
          }
          for (const auto &root : includeRoots) {
            std::filesystem::path candidate = root / logicalPath;
            if (std::filesystem::exists(candidate)) {
              if (isPrivatePath(candidate)) {
                error = "include path refers to private folder: " + std::filesystem::absolute(candidate).string();
                return false;
              }
              resolved = std::filesystem::absolute(candidate);
              return true;
            }
          }
          error = "failed to read include: " + std::filesystem::absolute(requested).string();
          return false;
        };

        std::optional<std::string> selectedVersion;
        std::vector<std::filesystem::path> allIncludeFiles;
        for (const auto &path : paths) {
          std::filesystem::path resolved;
          std::optional<std::string> resolvedVersion;
          if (!resolveIncludePath(path, resolved, resolvedVersion)) {
            return false;
          }
          if (requestedVersion) {
            if (!resolvedVersion.has_value()) {
              error = "include version not resolved";
              return false;
            }
            if (!selectedVersion) {
              selectedVersion = *resolvedVersion;
            } else if (*selectedVersion != *resolvedVersion) {
              error = "include version mismatch: expected " + *selectedVersion + " but got " + *resolvedVersion;
              return false;
            }
          }
          std::vector<std::filesystem::path> includeFiles;
          if (!collectPrimeFiles(resolved, includeFiles, error)) {
            return false;
          }
          allIncludeFiles.insert(allIncludeFiles.end(), includeFiles.begin(), includeFiles.end());
        }
        for (const auto &includeFile : allIncludeFiles) {
          std::string resolvedText = includeFile.string();
          if (expanded.count(resolvedText) > 0) {
            continue;
          }
          std::string included;
          if (!readFile(resolvedText, included)) {
            error = "failed to read include: " + resolvedText;
            return false;
          }
          expanded.insert(resolvedText);
          if (!expandIncludesInternal(includeFile.parent_path().string(), included, expanded, error, includeRoots)) {
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
