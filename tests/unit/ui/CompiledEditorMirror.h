#pragma once

// Runs an editor scenario again as the compiled programs (examples/apps/text_editor built with
// scripts/build_native_app.sh --backend headless, and on macOS/arm64 also by primec's own native backend
// against the headless dylib, see CMakeLists.txt) and checks each compiled editor leaves the same headless
// call log, exit code and files as the VM run that just finished. The scenario is whatever the test pushed
// to the headless backend, read back with `pushedScenario()`.
//
//   compiled_editor::Mirror mirror;            // before the VM run: remembers the files
//   const auto result = script.run(args);      // the VM run
//   mirror.expectSame(result.exitCode, args);  // the compiled run, from the same starting files

#include "primec/testing/TestScratch.h"
#include "primec/ui/NativeUiHeadless.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace compiled_editor {

inline std::string shellQuote(const std::string &text) {
  std::string out = "'";
  for (const char c : text) {
    out += c == '\'' ? std::string("'\\''") : std::string(1, c);
  }
  return out + "'";
}

inline std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// The test scratch files (what the scenarios write), without this helper's own output directory.
using Files = std::map<std::string, std::string>;

// testScratchDir hands out a fresh directory per call, so this one is made once.
inline std::filesystem::path outputDir() {
  static const std::filesystem::path dir = [] {
    const std::filesystem::path made = primec::testing::testScratchDir("native_ui_compiled_editor");
    std::filesystem::create_directories(made);
    return made;
  }();
  return dir;
}

inline std::filesystem::path scratchRoot() { return outputDir().parent_path(); }

inline Files snapshot() {
  Files files;
  const std::string skip = outputDir().string();
  for (const auto &entry : std::filesystem::recursive_directory_iterator(scratchRoot())) {
    if (entry.is_regular_file() && entry.path().string().rfind(skip, 0) != 0) {
      files[entry.path().string()] = readFile(entry.path().string());
    }
  }
  return files;
}

inline void restore(const Files &files) {
  const std::string skip = outputDir().string();
  std::vector<std::filesystem::path> extra;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(scratchRoot())) {
    if (entry.is_regular_file() && entry.path().string().rfind(skip, 0) != 0 &&
        files.find(entry.path().string()) == files.end()) {
      extra.push_back(entry.path());
    }
  }
  for (const auto &path : extra) {
    std::filesystem::remove(path);
  }
  for (const auto &[path, contents] : files) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream(path, std::ios::binary) << contents;
  }
}

class Mirror {
public:
  Mirror() {
#ifdef PRIMESTRUCT_COMPILED_EDITOR
    before_ = snapshot();
#endif
  }

  // Call after the VM run of the editor (`vmExitCode` is its exit code, `args` the program arguments).
  void expectSame(int vmExitCode, const std::vector<std::string> &args = {}) {
#ifdef PRIMESTRUCT_COMPILED_EDITOR
    const std::string scenario = primec::ui::headless::pushedScenario();
    std::string expected;
    for (const std::string &line : primec::ui::headless::callLog()) {
      expected += line + "\n";
    }
    const Files afterVm = snapshot();
    std::vector<std::string> editors = {PRIMESTRUCT_COMPILED_EDITOR};
#ifdef PRIMESTRUCT_COMPILED_EDITOR_NATIVE
    editors.push_back(PRIMESTRUCT_COMPILED_EDITOR_NATIVE);
#endif
    for (const std::string &editor : editors) {
      restore(before_);
      const std::string output = (outputDir() / "log.out").string();
      std::string command = "HEADLESS_SCENARIO=" + shellQuote(scenario) + " " + shellQuote(editor);
      for (const std::string &arg : args) {
        command += " " + shellQuote(arg);
      }
      const int status = std::system((command + " > " + shellQuote(output)).c_str());
      const int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
      CHECK_MESSAGE(exitCode == (vmExitCode & 0xFF), editor << ": compiled editor exit code for " << scenario);
      CHECK_MESSAGE(readFile(output) == expected, editor << ": compiled editor call log differs for " << scenario);
      CHECK_MESSAGE(snapshot() == afterVm, editor << ": compiled editor left different files for " << scenario);
    }
    restore(afterVm);
#else
    (void)vmExitCode;
    (void)args;
#endif
  }

private:
  Files before_;
};

} // namespace compiled_editor
