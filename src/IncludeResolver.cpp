#include "primec/IncludeResolver.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace primec {
namespace {

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
  std::filesystem::path parent = path.parent_path();
  for (const auto &part : parent) {
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
  std::unordered_set<std::string> expanded;
  return expandIncludesInternal(baseDir, source, expanded, error, includeRoots);
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
      if (source.compare(i, 8, "include<") == 0) {
        size_t start = i + 8;
        size_t end = source.find('>', start);
        if (end == std::string::npos) {
          error = "unterminated include<...> directive";
          return false;
        }
        std::string payload = source.substr(start, end - start);
        std::vector<std::string> paths;
        std::optional<std::string> versionTag;
        size_t pos = 0;
        while (pos < payload.size()) {
          while (pos < payload.size() && std::isspace(static_cast<unsigned char>(payload[pos]))) {
            ++pos;
          }
          if (pos >= payload.size()) {
            break;
          }
          if (payload.compare(pos, 8, "version=") == 0) {
            pos += 8;
            if (pos < payload.size() && payload[pos] == '\"') {
              ++pos;
              size_t quoteEnd = payload.find('\"', pos);
              if (quoteEnd == std::string::npos) {
                error = "unterminated version string in include<...>";
                return false;
              }
              if (versionTag) {
                error = "duplicate version attribute in include<...>";
                return false;
              }
              std::string versionValue = payload.substr(pos, quoteEnd - pos);
              versionTag = trim(versionValue);
              pos = quoteEnd + 1;
              continue;
            }
          }
          if (payload[pos] == '\"') {
            ++pos;
            size_t quoteEnd = payload.find('\"', pos);
            if (quoteEnd == std::string::npos) {
              error = "unterminated include path string";
              return false;
            }
            std::string path = payload.substr(pos, quoteEnd - pos);
            paths.push_back(trim(path));
            pos = quoteEnd + 1;
          } else {
            size_t next = payload.find(',', pos);
            if (next == std::string::npos) {
              next = payload.size();
            }
            pos = next + 1;
          }
          if (pos < payload.size() && payload[pos] == ',') {
            ++pos;
          }
        }

        if (paths.empty()) {
          error = "include<...> requires at least one quoted path";
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

        auto resolveIncludePath = [&](const std::string &path, std::filesystem::path &resolved) -> bool {
          std::filesystem::path requested(path);
          bool isAbsolute = requested.is_absolute();
          std::filesystem::path logicalPath = isAbsolute ? requested.relative_path() : requested;
          if (requestedVersion) {
            std::vector<std::filesystem::path> roots;
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
            std::string versionError;
            bool foundVersion = false;
            std::filesystem::path lastCandidate;
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
                resolved = std::filesystem::absolute(candidate);
                return true;
              }
              lastCandidate = candidate;
            }
            if (!foundVersion) {
              error = versionError.empty() ? "include version not found" : versionError;
              return false;
            }
            if (!lastCandidate.empty()) {
              error = "failed to read include: " + std::filesystem::absolute(lastCandidate).string();
            } else {
              error = "failed to read include: " + logicalPath.string();
            }
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

        for (const auto &path : paths) {
          std::filesystem::path resolved;
          if (!resolveIncludePath(path, resolved)) {
            return false;
          }
          std::vector<std::filesystem::path> includeFiles;
          if (!collectPrimeFiles(resolved, includeFiles, error)) {
            return false;
          }
          for (const auto &includeFile : includeFiles) {
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
