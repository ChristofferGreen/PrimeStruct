// Runner for PrimeStruct native UI programs on the platform backend.
//
//   primestruct_app program.prime [args...]   compile at launch (development)
//   primestruct_app program.psir  [args...]   run precompiled bytecode
//
// The runtime-only build (PRIMESTRUCT_APP_RUNTIME_ONLY) links no compiler and
// accepts bytecode only; bundles use it (scripts/bundle_macos_app.sh).
//
// Environment: PRIMESTRUCT_UI_SNAPSHOT=out.png renders the first window into a
// PNG once laid out and then quits (AppKit backend).

#include "primec/embed/Script.h"
#include "primec/ui/NativeUiBindings.h"

#ifndef PRIMESTRUCT_APP_RUNTIME_ONLY
#include "primec/embed/ScriptEngine.h"
#endif

#if defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#endif

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

bool endsWith(const std::string &text, const std::string &suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

primec::embed::Script loadProgram(const std::string &path) {
  if (endsWith(path, ".psir")) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      std::fprintf(stderr, "primestruct_app: cannot read %s\n", path.c_str());
      return {};
    }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return primec::embed::Script::loadBytecode(bytes, path);
  }
#ifdef PRIMESTRUCT_APP_RUNTIME_ONLY
  std::fprintf(stderr, "primestruct_app: this build runs precompiled .psir bytecode only\n");
  return {};
#else
  primec::embed::ScriptEngine engine;
  return engine.compileFile(path);
#endif
}

// Inside a bundle (Name.app/Contents/MacOS/Name) the program is the bytecode in
// Contents/Resources/app.psir.
std::string bundledProgramPath() {
#if defined(__APPLE__)
  char buffer[PATH_MAX];
  uint32_t size = sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) == 0) {
    const std::filesystem::path executable = std::filesystem::weakly_canonical(buffer);
    const std::filesystem::path resource = executable.parent_path().parent_path() / "Resources" / "app.psir";
    if (std::filesystem::exists(resource)) {
      return resource.string();
    }
  }
#endif
  return {};
}

} // namespace

int main(int argc, char **argv) {
  std::string programPath;
  int firstArg = 2;
  if (argc >= 2) {
    programPath = argv[1];
  } else {
    programPath = bundledProgramPath();
    firstArg = argc;
  }
  if (programPath.empty()) {
    std::fputs("usage: primestruct_app program.prime|program.psir [args...]\n", stderr);
    return 64;
  }
  primec::embed::Script script = loadProgram(programPath);
  if (!script.valid()) {
    std::fputs(script.diagnostics().c_str(), stderr);
    return 2;
  }
  primec::ui::bindNativeUi(script);
  const std::vector<std::string> args(argv + firstArg, argv + argc);
  const auto result = script.run(args);
  if (!result.ok) {
    std::fputs(result.diagnostics.c_str(), stderr);
    return 3;
  }
  return result.exitCode;
}
