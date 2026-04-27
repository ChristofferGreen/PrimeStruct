#pragma once

#include "test_compile_run_path_png_helpers.h"

#include "third_party/doctest.h"

#include "primec/testing/TestScratch.h"

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

inline std::filesystem::path testScratchPath(std::string_view relativePath) {
  return primec::testing::testScratchPath(relativePath);
}

inline std::filesystem::path testScratchDir(std::string_view prefix) {
  return primec::testing::testScratchDir(prefix);
}

inline std::string writeTemp(const std::string &name, const std::string &contents) {
  const auto path = testScratchPath("sources/" + name);
  std::ofstream file(path);
  CHECK(file.good());
  file << contents;
  CHECK(file.good());
  return path.string();
}

inline std::string quoteShellArg(const std::string &value) {
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

inline int runCommand(const std::string &command) {
  int code = -1;
  constexpr int MaxAttempts = 3;
  for (int attempt = 0; attempt < MaxAttempts; ++attempt) {
    code = std::system(command.c_str());
    if (code != -1 || attempt + 1 == MaxAttempts) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50 * (attempt + 1)));
  }
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

inline int expectedProcessExitCode(int code) {
#if defined(__unix__) || defined(__APPLE__)
  return code & 0xff;
#else
  return code;
#endif
}

inline std::string readFile(const std::string &path) {
  std::ifstream file(path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

inline std::vector<unsigned char> readBinaryFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
}

inline bool hasZipTools() {
  return runCommand("zip -v > /dev/null 2>&1") == 0 &&
         runCommand("unzip -v > /dev/null 2>&1") == 0;
}

inline bool hasPython3() {
  return runCommand("python3 --version > /dev/null 2>&1") == 0;
}

inline bool hasSpirvTools() {
  return runCommand("glslangValidator -v > /dev/null 2>&1") == 0 ||
         runCommand("glslc --version > /dev/null 2>&1") == 0;
}

inline bool hasWasmtime() {
  return runCommand("wasmtime --version > /dev/null 2>&1") == 0;
}

inline bool createZip(const std::filesystem::path &zipPath,
                      const std::filesystem::path &sourceDir) {
  const std::string command = "cd " + quoteShellArg(sourceDir.string()) +
                              " && zip -q -r " + quoteShellArg(zipPath.string()) +
                              " .";
  return runCommand(command) == 0;
}
