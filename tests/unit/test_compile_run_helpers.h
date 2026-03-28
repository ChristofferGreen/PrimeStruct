#pragma once

#include "third_party/doctest.h"

#include "primec/testing/TestScratch.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
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

inline int runCommand(const std::string &command) {
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
