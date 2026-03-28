#include "third_party/doctest.h"

#include "test_compile_run_path_png_helpers.h"

#include "primec/IrSerializer.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"
#include "primec/testing/EmitterHelpers.h"
#include "primec/testing/TestScratch.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {
std::filesystem::path testScratchPath(std::string_view relativePath);
std::filesystem::path testScratchDir(std::string_view prefix);

std::string writeTemp(const std::string &name, const std::string &contents) {
  const auto path = testScratchPath("sources/" + name);
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

std::filesystem::path testScratchPath(std::string_view relativePath) {
  return primec::testing::testScratchPath(relativePath);
}

[[maybe_unused]] std::filesystem::path testScratchDir(std::string_view prefix) {
  return primec::testing::testScratchDir(prefix);
}

} // namespace

#include "test_compile_run_vm_bounds.h"
