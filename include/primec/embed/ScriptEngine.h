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

  // Declares a script function the host will call with Script::call. Compile
  // generates one entry per export, so each export adds compile time. The
  // function must exist in the script with exactly this signature (primitive
  // types only).
  void exportFunction(std::string name, std::vector<HostType> parameters, HostType returnType);
  template <class Signature> void exportFunction(std::string name) {
    exportFunctionTyped(std::move(name), static_cast<Signature *>(nullptr));
  }

  // Host functions applied to every script this engine compiles (a script can
  // still add or override bindings with `Script::bind`).
  template <class F> void bind(std::string name, F callable) { hostBindings_.bind(std::move(name), std::move(callable)); }

  Script compileFile(const std::string &path) const;
  // Compiles in-memory text. `name` is used in diagnostics and as the base
  // for relative imports.
  Script compileSource(const std::string &name, const std::string &text) const;

private:
  template <class R, class... A> void exportFunctionTyped(std::string name, R (*)(A...)) {
    exportFunction(std::move(name), {detail::HostTypeOf<A>::value...}, detail::HostTypeOf<R>::value);
  }

  struct ExportDecl {
    std::string name;
    ExportSignature signature;
  };

  Script compile(const std::string &path, const std::string *text) const;
  bool compileEntry(const std::string &path,
                    const std::string &text,
                    const std::string &entryPath,
                    std::shared_ptr<Script::Module> &module,
                    std::string &diagnostics) const;

  std::vector<std::string> importPaths_;
  std::string entryPath_ = "/main";
  std::string stdlibPath_;
  HostBindings hostBindings_;
  std::vector<ExportDecl> exports_;
};

} // namespace primec::embed
