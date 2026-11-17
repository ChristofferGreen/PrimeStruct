#pragma once

#include <string>
#include <vector>

namespace primec {

// The hash scheme of the cache files; it is written into every file, and a file of another scheme is ignored.
inline constexpr const char *BuildCacheHashScheme = "sha256";

struct BuildCacheDep {
  std::string path;
  std::string hash;
};

// What a finished compile step leaves in `ProjectRoot/.ps/cache/`: the key it ran under, the hash of its output and
// the content hashes of the files the compiler read (`--emit-deps`) plus the files named by its link arguments.
struct BuildCacheEntry {
  std::string key;
  std::string outputHash;
  std::vector<BuildCacheDep> deps;
};

// The key of one step: the hash of the compiler executable (its version), the hash scheme and the compiler
// arguments of the step (everything after the compiler path: emit kind, optimization, entry, import paths,
// transforms, effects, link arguments, output directory, output name, source).
std::string buildStepKey(const std::string &compilerHash, const std::vector<std::string> &stepArguments);

// The cache file of a step id inside `cacheDir`.
std::string buildCacheFilePath(const std::string &cacheDir, const std::string &stepId);

// Reads the file at `path`. False when it is missing, malformed or of another hash scheme.
bool readBuildCacheEntry(const std::string &path, BuildCacheEntry &entry);

// Writes `entry` to `path` (through a temporary file, so a reader never sees half of it). False when it cannot be
// written, or when a dependency path cannot be stored on one line.
bool writeBuildCacheEntry(const std::string &path, const BuildCacheEntry &entry);

// Hashes the content of every path (sorted, without duplicates). False when a file cannot be read.
bool hashBuildDependencies(const std::vector<std::string> &paths, std::vector<BuildCacheDep> &deps);

// Reads a `--emit-deps` file: one path per line.
bool readDependencyList(const std::string &path, std::vector<std::string> &paths);

// True when `entry` was made under `key`, the output file exists with the recorded hash, and every dependency still
// has its recorded content hash.
bool buildCacheEntryIsCurrent(const BuildCacheEntry &entry, const std::string &key, const std::string &outputPath);

} // namespace primec
