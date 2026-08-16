#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

#include "third_party/doctest.h"

#include "primec/WasmEmitter.h"
#include "primec/testing/TestScratch.h"

namespace ir_pipeline_wasm_test {

inline std::filesystem::path irPipelineWasmPath(const std::string &relativePath) {
  return primec::testing::testScratchPath("ir_pipeline_wasm/" + relativePath);
}

inline bool hasMinimalWasmHeader(const std::vector<uint8_t> &bytes) {
  static constexpr uint8_t Expected[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  if (bytes.size() < sizeof(Expected)) {
    return false;
  }
  for (size_t i = 0; i < sizeof(Expected); ++i) {
    if (bytes[i] != Expected[i]) {
      return false;
    }
  }
  return true;
}

inline bool readU32Leb(const std::vector<uint8_t> &bytes, size_t &offset, uint32_t &value) {
  value = 0;
  uint32_t shift = 0;
  for (int i = 0; i < 5; ++i) {
    if (offset >= bytes.size()) {
      return false;
    }
    const uint8_t byte = bytes[offset++];
    value |= static_cast<uint32_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      return true;
    }
    shift += 7;
  }
  return false;
}

inline bool parseSections(const std::vector<uint8_t> &bytes,
                          std::vector<uint8_t> &sectionIds,
                          std::vector<uint32_t> &sectionLengths,
                          std::string &error) {
  error.clear();
  sectionIds.clear();
  sectionLengths.clear();
  if (!hasMinimalWasmHeader(bytes)) {
    error = "missing wasm header";
    return false;
  }

  size_t offset = 8;
  while (offset < bytes.size()) {
    const uint8_t sectionId = bytes[offset++];
    uint32_t sectionLength = 0;
    if (!readU32Leb(bytes, offset, sectionLength)) {
      error = "failed to decode section length";
      return false;
    }
    if (offset + sectionLength > bytes.size()) {
      error = "section payload exceeds module bounds";
      return false;
    }
    sectionIds.push_back(sectionId);
    sectionLengths.push_back(sectionLength);
    offset += sectionLength;
  }
  return true;
}

inline bool sectionPayloadIsEmptyVector(const std::vector<uint8_t> &bytes, uint8_t sectionId) {
  size_t offset = 8;
  while (offset < bytes.size()) {
    const uint8_t currentSectionId = bytes[offset++];
    uint32_t sectionLength = 0;
    if (!readU32Leb(bytes, offset, sectionLength)) {
      return false;
    }
    if (offset + sectionLength > bytes.size()) {
      return false;
    }
    if (currentSectionId == sectionId) {
      return sectionLength == 1 && bytes[offset] == 0;
    }
    offset += sectionLength;
  }
  return false;
}

inline bool containsByteSequence(const std::vector<uint8_t> &haystack, const std::vector<uint8_t> &needle) {
  if (needle.empty() || needle.size() > haystack.size()) {
    return false;
  }
  for (size_t start = 0; start + needle.size() <= haystack.size(); ++start) {
    bool match = true;
    for (size_t offset = 0; offset < needle.size(); ++offset) {
      if (haystack[start + offset] != needle[offset]) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
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

inline std::string readFileText(const std::string &path) {
  std::ifstream file(path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

inline bool hasWasmtime() {
  return runCommand("wasmtime --version > /dev/null 2>&1") == 0;
}

} // namespace ir_pipeline_wasm_test
