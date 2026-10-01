#pragma once

#include "primec/embed/Script.h"

#include <string>
#include <vector>

namespace primec::embed {

class ScriptEngine {
public:
  ScriptEngine();

  // Adds a directory searched for `import` resolution. The bundled stdlib is
  // located automatically.
  void addImportPath(std::string path);
  void setEntryPath(std::string entryPath);
  // Directory holding the PrimeStruct stdlib (the folder containing `std/`).
  // When unset the engine tries $PRIMESTRUCT_STDLIB, then the installed copy
  // (<prefix>/share/primestruct/stdlib), then the source tree it was built from.
  void setStdlibPath(std::string path);

  // Host functions applied to every script this engine compiles (a script can
  // still add or override bindings with `Script::bind`).
  template <class F> void bind(std::string name, F callable) { hostBindings_.bind(std::move(name), std::move(callable)); }

  Script compileFile(const std::string &path) const;
  // Compiles in-memory text. `name` is used in diagnostics and as the base
  // for relative imports.
  Script compileSource(const std::string &name, const std::string &text) const;

private:
  Script compile(const std::string &path, const std::string *text) const;

  std::vector<std::string> importPaths_;
  std::string entryPath_ = "/main";
  std::string stdlibPath_;
  HostBindings hostBindings_;
};

} // namespace primec::embed
