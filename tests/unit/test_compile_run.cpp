#include "third_party/doctest.h"

#include "primec/IrSerializer.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {
std::string writeTemp(const std::string &name, const std::string &contents) {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests";
  std::filesystem::create_directories(dir);
  auto path = dir / name;
  std::ofstream file(path);
  CHECK(file.good());
  file << contents;
  CHECK(file.good());
  return path.string();
}

int runCommand(const std::string &command) {
  int code = std::system(command.c_str());
#if defined(__unix__) || defined(__APPLE__)
  if (code == -1) {
    return -1;
  }
  if (WIFEXITED(code)) {
    return WEXITSTATUS(code);
  }
  return -1;
#else
  return code;
#endif
}

std::string readFile(const std::string &path) {
  std::ifstream file(path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::string quoteShellArg(const std::string &value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

bool hasZipTools() {
  return runCommand("zip -v > /dev/null 2>&1") == 0 && runCommand("unzip -v > /dev/null 2>&1") == 0;
}

bool createZip(const std::filesystem::path &zipPath, const std::filesystem::path &sourceDir) {
  const std::string command = "cd " + quoteShellArg(sourceDir.string()) + " && zip -q -r " +
                              quoteShellArg(zipPath.string()) + " .";
  return runCommand(command) == 0;
}
} // namespace

TEST_SUITE_BEGIN("primestruct.compile.run");

#include "test_compile_run_smoke.h"
#include "test_compile_run_vm_collections.h"
#include "test_compile_run_emitters.h"
#include "test_compile_run_native_backend.h"
#include "test_compile_run_includes.h"
#include "test_compile_run_text_filters.h"
#include "test_compile_run_bindings_and_examples.h"
