#pragma once

#include "primec/embed/Script.h"

#include <optional>
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

  // Holds the script's own code (not the stdlib) to this list of effects, as `primec --allow-effects` does: a
  // definition outside it is a compile diagnostic. Unset (the default): no ceiling.
  void setAllowEffects(std::vector<std::string> effects);

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

  // Registers how an already bound host function runs when tasks run on threads (HostThreading in Script.h); false
  // when nothing is bound under that name. A later `bind` of the same name resets it.
  bool setHostThreading(const std::string &name, HostThreading threading) {
    return hostBindings_.setThreading(name, threading);
  }

  Script compileFile(const std::string &path) const;
  // Compiles in-memory text. `name` is used in diagnostics and as the base
  // for relative imports.
  Script compileSource(const std::string &name, const std::string &text) const;

private:
  template <class R, class... A> void exportFunctionTyped(std::string name, R (*)(A...)) {
    exportFunction(std::move(name), {detail::HostTypeOf<std::remove_cvref_t<A>>::value...},
                   detail::HostTypeOf<R>::value);
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
                    std::string &diagnostics,
                    std::vector<std::string> *sourceFiles = nullptr) const;

  std::vector<std::string> importPaths_;
  std::string entryPath_ = "/main";
  std::string stdlibPath_;
  std::optional<std::vector<std::string>> allowEffects_;
  HostBindings hostBindings_;
  std::vector<ExportDecl> exports_;
};

} // namespace primec::embed
