#include "primec/IncludeResolver.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
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

} // namespace

bool IncludeResolver::expandIncludes(const std::string &inputPath, std::string &source, std::string &error) {
  std::filesystem::path input = std::filesystem::absolute(inputPath);
  std::string content;
  if (!readFile(input.string(), content)) {
    error = "failed to read input: " + input.string();
    return false;
  }
  source = std::move(content);
  std::string baseDir = input.parent_path().string();
  std::unordered_set<std::string> expanded;
  return expandIncludesInternal(baseDir, source, expanded, error);
}

bool IncludeResolver::expandIncludesInternal(const std::string &baseDir,
                                             std::string &source,
                                             std::unordered_set<std::string> &expanded,
                                             std::string &error) {
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

        for (const auto &path : paths) {
          std::filesystem::path resolved(path);
          if (resolved.is_relative()) {
            resolved = std::filesystem::path(baseDir) / resolved;
          }
          resolved = std::filesystem::absolute(resolved);
          std::string resolvedText = resolved.string();
          if (expanded.count(resolvedText) > 0) {
            continue;
          }
          std::string included;
          if (!readFile(resolvedText, included)) {
            error = "failed to read include: " + resolvedText;
            return false;
          }
          expanded.insert(resolvedText);
          if (!expandIncludesInternal(resolved.parent_path().string(), included, expanded, error)) {
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
