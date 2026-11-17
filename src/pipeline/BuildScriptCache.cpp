#include "primec/pipeline/BuildScriptCache.h"

#include "EmitDeps.h"
#include "primec/ir/IrSerializer.h"
#include "primec/pipeline/BuildCache.h"
#include "primec/support/Sha256.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

namespace primec {

namespace {

constexpr const char *ScriptStepId = "make-script";

bool readBytes(const std::string &path, std::vector<uint8_t> &bytes) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  return true;
}

} // namespace

BuildScriptCache::BuildScriptCache(const std::string &cacheDir,
                                   const std::string &compilerHash,
                                   const std::vector<std::string> &arguments,
                                   const std::string &source,
                                   const std::vector<std::string> &importPaths) {
  if (cacheDir.empty() || compilerHash.empty()) {
    return;
  }
  entryPath_ = buildCacheFilePath(cacheDir, ScriptStepId);
  modulePath_ = entryPath_ + ".ir";
  std::vector<std::string> keyArguments = arguments;
  keyArguments.push_back("source:" + source);
  for (const std::string &path : importPaths) {
    keyArguments.push_back("import-path:" + path);
  }
  key_ = buildStepKey(compilerHash, keyArguments);
}

std::string BuildScriptCache::cacheDirFor(const std::string &projectDir) {
  std::error_code ec;
  const std::filesystem::path absolute = std::filesystem::absolute(projectDir, ec);
  return ((ec ? std::filesystem::path(projectDir) : absolute) / ".ps" / "cache")
      .lexically_normal()
      .string();
}

bool BuildScriptCache::load(IrModule &module) const {
  if (key_.empty()) {
    return false;
  }
  BuildCacheEntry entry;
  if (!readBuildCacheEntry(entryPath_, entry) ||
      !buildCacheEntryIsCurrent(entry, key_, modulePath_)) {
    return false;
  }
  std::vector<uint8_t> bytes;
  std::string error;
  IrModule loaded;
  if (!readBytes(modulePath_, bytes) || !deserializeIr(bytes, loaded, error)) {
    return false;
  }
  module = std::move(loaded);
  return true;
}

bool BuildScriptCache::store(const IrModule &module, const ExpandedSource &sources) const {
  if (key_.empty()) {
    return false;
  }
  std::vector<uint8_t> bytes;
  std::string error;
  if (!serializeIr(module, bytes, error)) {
    return false;
  }
  BuildCacheEntry entry;
  entry.key = key_;
  if (!hashBuildDependencies(collectSourceDependencies(sources), entry.deps)) {
    return false;
  }
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(modulePath_).parent_path(), ec);
  // The entry goes last: a reader that finds it finds a finished module file.
  std::filesystem::remove(entryPath_, ec);
  const std::string temporary = modulePath_ + ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) {
      return false;
    }
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.flush();
    if (!out) {
      return false;
    }
  }
  std::filesystem::rename(temporary, modulePath_, ec);
  if (ec) {
    std::filesystem::remove(temporary, ec);
    return false;
  }
  entry.outputHash =
      sha256Hex(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
  return writeBuildCacheEntry(entryPath_, entry);
}

} // namespace primec
