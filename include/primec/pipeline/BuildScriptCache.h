#pragma once

#include "primec/frontend/ExpandedSource.h"
#include "primec/ir/Ir.h"

#include <string>
#include <vector>

namespace primec {

// The compiled form of a project's make.prime (docs/BuildSystemPlan.md section 8). `primec build` compiles the script
// with the build stdlib before it runs anything, which costs seconds; the script is stored as serialized VM IR in
// `ProjectRoot/.ps/cache/` and the next run loads it instead of compiling. The compile is a pure function of the
// script text, the compiler arguments, the compiler executable and the files the compile read, so the entry is
// reused only while all of them are unchanged. Running the script (its globs and file reads) is never cached.
class BuildScriptCache {
public:
  // `arguments` is the argument list of the script's compile (the options and the entry), `source` the text it
  // compiles, `importPaths` the import search directories it resolves stdlib and imports through. An empty
  // `compilerHash` or `cacheDir` gives a cache that never hits and never stores.
  BuildScriptCache(const std::string &cacheDir,
                   const std::string &compilerHash,
                   const std::vector<std::string> &arguments,
                   const std::string &source,
                   const std::vector<std::string> &importPaths);

  // The cache directory of a project directory: `<project>/.ps/cache`, absolute.
  static std::string cacheDirFor(const std::string &projectDir);

  // True and `module` set when the stored module is current: made under the same key, its file unchanged and every
  // file the compile read still has its recorded content hash.
  bool load(IrModule &module) const;

  // Stores `module` with the files in `sources` as its dependencies. False when nothing could be stored (the cache is
  // disabled, the module does not serialize, the directory cannot be written); callers ignore it.
  bool store(const IrModule &module, const ExpandedSource &sources) const;

private:
  std::string entryPath_;
  std::string modulePath_;
  std::string key_;
};

} // namespace primec
