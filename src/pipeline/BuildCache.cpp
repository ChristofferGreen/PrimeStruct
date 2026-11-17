#include "primec/pipeline/BuildCache.h"

#include "primec/support/Sha256.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace primec {

namespace {

const char *const CacheMagic = "primestruct-build-cache 1";

bool singleLine(const std::string &text) {
  return text.find('\n') == std::string::npos && text.find('\r') == std::string::npos;
}

} // namespace

std::string buildStepKey(const std::string &compilerHash, const std::vector<std::string> &stepArguments) {
  // Every field is length-prefixed, so two argument lists never give the same text.
  std::string text;
  const auto field = [&](const std::string &value) {
    text += std::to_string(value.size());
    text += ':';
    text += value;
    text += '\n';
  };
  field(BuildCacheHashScheme);
  field(compilerHash);
  for (const std::string &argument : stepArguments) {
    field(argument);
  }
  return sha256Hex(text);
}

std::string buildCacheFilePath(const std::string &cacheDir, const std::string &stepId) {
  std::string name;
  for (const char c : stepId) {
    const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                       c == '-' || c == '_';
    name += plain ? c : '_';
  }
  // The digest keeps ids that differ only in punctuation apart.
  name += "-" + sha256Hex(stepId).substr(0, 12) + ".cache";
  return (std::filesystem::path(cacheDir) / name).string();
}

bool readBuildCacheEntry(const std::string &path, BuildCacheEntry &entry) {
  entry = BuildCacheEntry{};
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  std::string line;
  if (!std::getline(file, line) || line != CacheMagic) {
    return false;
  }
  if (!std::getline(file, line) || line != std::string("hash ") + BuildCacheHashScheme) {
    return false;
  }
  bool sawKey = false;
  bool sawOutput = false;
  while (std::getline(file, line)) {
    if (line.rfind("key ", 0) == 0) {
      entry.key = line.substr(4);
      sawKey = true;
    } else if (line.rfind("output ", 0) == 0) {
      entry.outputHash = line.substr(7);
      sawOutput = true;
    } else if (line.rfind("dir ", 0) == 0) {
      const size_t space = line.find(' ', 4);
      if (space == std::string::npos) {
        entry = BuildCacheEntry{};
        return false;
      }
      entry.dirs.push_back({line.substr(space + 1), line.substr(4, space - 4)});
    } else if (line.rfind("dep ", 0) == 0) {
      const size_t space = line.find(' ', 4);
      if (space == std::string::npos) {
        entry = BuildCacheEntry{};
        return false;
      }
      entry.deps.push_back({line.substr(space + 1), line.substr(4, space - 4)});
    } else {
      entry = BuildCacheEntry{};
      return false;
    }
  }
  if (!sawKey || !sawOutput) {
    entry = BuildCacheEntry{};
    return false;
  }
  return true;
}

bool writeBuildCacheEntry(const std::string &path, const BuildCacheEntry &entry) {
  for (const BuildCacheDep &dep : entry.deps) {
    if (!singleLine(dep.path)) {
      return false;
    }
  }
  for (const BuildCacheDep &dir : entry.dirs) {
    if (!singleLine(dir.path)) {
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
  const std::string temporary = path + ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) {
      return false;
    }
    out << CacheMagic << '\n' << "hash " << BuildCacheHashScheme << '\n';
    out << "key " << entry.key << '\n' << "output " << entry.outputHash << '\n';
    for (const BuildCacheDep &dep : entry.deps) {
      out << "dep " << dep.hash << ' ' << dep.path << '\n';
    }
    for (const BuildCacheDep &dir : entry.dirs) {
      out << "dir " << dir.hash << ' ' << dir.path << '\n';
    }
    out.flush();
    if (!out) {
      return false;
    }
  }
  std::filesystem::rename(temporary, path, ec);
  if (ec) {
    std::filesystem::remove(temporary, ec);
    return false;
  }
  return true;
}

bool hashBuildDependencies(const std::vector<std::string> &paths, std::vector<BuildCacheDep> &deps) {
  deps.clear();
  std::vector<std::string> sorted = paths;
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  for (const std::string &path : sorted) {
    std::string hash;
    if (!sha256FileHex(path, hash)) {
      deps.clear();
      return false;
    }
    deps.push_back({path, hash});
  }
  return true;
}

std::string hashDirectoryListing(const std::string &path) {
  std::error_code ec;
  std::filesystem::directory_iterator it(path, std::filesystem::directory_options::none, ec);
  if (ec) {
    return ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory
               ? "missing"
               : "unreadable";
  }
  std::vector<std::pair<std::string, char>> entries;
  for (const std::filesystem::directory_iterator end; it != end; it.increment(ec)) {
    if (ec) {
      return "unreadable";
    }
    std::error_code statusError;
    const std::filesystem::file_status status = std::filesystem::status(it->path(), statusError);
    char kind = 'O';
    if (!statusError && std::filesystem::is_regular_file(status)) {
      kind = 'F';
    } else if (!statusError && std::filesystem::is_directory(status)) {
      kind = 'D';
    }
    entries.emplace_back(it->path().filename().string(), kind);
  }
  std::sort(entries.begin(), entries.end());
  // Every name is length-prefixed, so no two listings give the same text.
  std::string text = "listing\n";
  for (const auto &entry : entries) {
    text += entry.second;
    text += ' ';
    text += std::to_string(entry.first.size());
    text += ':';
    text += entry.first;
    text += '\n';
  }
  return sha256Hex(text);
}

std::vector<BuildCacheDep> hashBuildInputDirs(const std::vector<std::string> &paths) {
  std::vector<std::string> sorted = paths;
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  std::vector<BuildCacheDep> dirs;
  for (const std::string &path : sorted) {
    dirs.push_back({path, hashDirectoryListing(path)});
  }
  return dirs;
}

bool readDependencyList(const std::string &path, std::vector<std::string> &paths) {
  paths.clear();
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty()) {
      paths.push_back(line);
    }
  }
  return true;
}

bool buildCacheEntryIsCurrent(const BuildCacheEntry &entry, const std::string &key, const std::string &outputPath) {
  std::string outputHash;
  if (!sha256FileHex(outputPath, outputHash)) {
    return false;
  }
  return buildCacheEntryIsCurrentForOutput(entry, key, outputHash);
}

bool buildCacheEntryIsCurrentForOutput(const BuildCacheEntry &entry,
                                       const std::string &key,
                                       const std::string &outputHash) {
  if (entry.key.empty() || entry.key != key || outputHash != entry.outputHash) {
    return false;
  }
  for (const BuildCacheDep &dep : entry.deps) {
    std::string hash;
    if (!sha256FileHex(dep.path, hash) || hash != dep.hash) {
      return false;
    }
  }
  for (const BuildCacheDep &dir : entry.dirs) {
    if (hashDirectoryListing(dir.path) != dir.hash) {
      return false;
    }
  }
  return true;
}

} // namespace primec
