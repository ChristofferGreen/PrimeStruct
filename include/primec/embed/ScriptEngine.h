#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace primec::embed {

// Outcome of compiling or running a script. Failures are reported as data:
// the embed API never exits the process or writes to stdout/stderr itself.
struct ScriptResult {
  bool ok = false;
  // Return value of `main` truncated to 32 bits, like the primevm exit code.
  int exitCode = 0;
  // Diagnostic text in the same plain format the CLI prints on failure.
  std::string diagnostics;
};

// A compiled script. Cheap to copy (shares the compiled module); `run` may be
// called repeatedly without recompiling.
class Script {
public:
  Script() = default;

  // False when compilation failed; `diagnostics()` then says why.
  bool valid() const { return module_ != nullptr; }
  const std::string &diagnostics() const { return diagnostics_; }

  // Runs the entry definition. `args` become the script's argv after the
  // leading program name.
  ScriptResult run(const std::vector<std::string> &args = {}) const;

private:
  friend class ScriptEngine;
  struct Module;
  std::shared_ptr<const Module> module_;
  std::string name_;
  std::string diagnostics_;
};

class ScriptEngine {
public:
  ScriptEngine();

  // Adds a directory searched for `import` resolution. The bundled stdlib is
  // located automatically.
  void addImportPath(std::string path);
  void setEntryPath(std::string entryPath);

  Script compileFile(const std::string &path) const;
  // Compiles in-memory text. `name` is used in diagnostics and as the base
  // for relative imports.
  Script compileSource(const std::string &name, const std::string &text) const;

private:
  Script compile(const std::string &path, const std::string *text) const;

  std::vector<std::string> importPaths_;
  std::string entryPath_ = "/main";
};

} // namespace primec::embed
