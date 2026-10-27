// Runner for PrimeStruct native UI programs on the platform backend.
//
//   primestruct_app program.prime [args...]   compile at launch (development)
//   primestruct_app program.psir  [args...]   run precompiled bytecode
//
// The runtime-only build (PRIMESTRUCT_APP_RUNTIME_ONLY) links no compiler and
// accepts bytecode only; bundles use it (scripts/bundle_macos_app.sh). Started from a
// bundle (Contents/Resources/app.psir next to it) it runs that bytecode and passes
// every argument to the program.
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
#include <iostream>
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

#ifndef PRIMESTRUCT_APP_RUNTIME_ONLY
// The script host (docs/NativeUiPlan.md section 18): the editor's Run starts this process with the script's text on stdin.
// The script runs with the native UI bound, so one that opens windows is an app of its own. Output is unbuffered and
// stdout and stderr share a pipe, so the editor shows it in program order; the last line says how the run ended (the
// process itself exits 0 once it got that far).
int runScriptHost(const std::string &name) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const std::string text((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
  primec::embed::ScriptEngine engine;
  primec::embed::Script script = engine.compileSource(name, text);
  if (!script.valid()) {
    std::fputs((script.diagnostics() + "\n").c_str(), stderr);
    return 0;
  }
  primec::ui::bindNativeUi(script);
  const auto result = script.run({});
  if (result.ok) {
    std::fprintf(stderr, "[exit code %d]\n", result.exitCode);
  } else {
    std::fputs((result.diagnostics + "\n").c_str(), stderr);
  }
  return 0;
}
#endif

} // namespace

int main(int argc, char **argv) {
#ifndef PRIMESTRUCT_APP_RUNTIME_ONLY
  if (argc >= 3 && std::string(argv[1]) == "--script-host") {
    return runScriptHost(argv[2]);
  }
#endif
  // Started from a bundle the program is the bytecode inside it and every argument is the program's (a file named
  // on the command line, as with a native bundle); anywhere else the first argument names the program.
  std::string programPath = bundledProgramPath();
  const bool bundled = !programPath.empty();
  int firstArg = 1;
  if (!bundled) {
    if (argc >= 2) {
      programPath = argv[1];
      firstArg = 2;
    } else {
      std::fputs("usage: primestruct_app program.prime|program.psir [args...]\n", stderr);
      return 64;
    }
  }
  primec::embed::Script script = loadProgram(programPath);
  if (!script.valid()) {
    std::fputs(script.diagnostics().c_str(), stderr);
    return 2;
  }
  primec::ui::bindNativeUi(script);
  std::vector<std::string> args;
  for (int i = firstArg; i < argc; ++i) {
    // Launch Services used to add a process serial number argument; it is not the program's.
    if (!bundled || std::string(argv[i]).rfind("-psn_", 0) != 0) {
      args.push_back(argv[i]);
    }
  }
  const auto result = script.run(args);
  if (!result.ok) {
    std::fputs(result.diagnostics.c_str(), stderr);
    return 3;
  }
  return result.exitCode;
}
