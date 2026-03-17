#include "third_party/doctest.h"

#include "primec/IrSerializer.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"
#include "src/emitter/EmitterHelpers.h"

#include <algorithm>
#include <cctype>
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

namespace {
std::string writeTemp(const std::string &name, const std::string &contents) {
  auto dir = std::filesystem::current_path() / "primec_tests";
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

int expectedProcessExitCode(int code) {
#if defined(__unix__) || defined(__APPLE__)
  return code & 0xff;
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

std::vector<unsigned char> readBinaryFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
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

uint32_t computePngCrc(const unsigned char *data, size_t size) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t index = 0; index < size; ++index) {
    crc ^= static_cast<uint32_t>(data[index]);
    for (int bit = 0; bit < 8; ++bit) {
      if ((crc & 1u) != 0u) {
        crc = (crc >> 1u) ^ 0xEDB88320u;
      } else {
        crc >>= 1u;
      }
    }
  }
  return ~crc;
}

std::vector<unsigned char> withValidPngCrcs(std::vector<unsigned char> bytes) {
  if (bytes.size() < 8) {
    return bytes;
  }

  size_t offset = 8;
  while (offset + 12 <= bytes.size()) {
    const uint32_t length = (static_cast<uint32_t>(bytes[offset]) << 24u) |
                            (static_cast<uint32_t>(bytes[offset + 1]) << 16u) |
                            (static_cast<uint32_t>(bytes[offset + 2]) << 8u) |
                            static_cast<uint32_t>(bytes[offset + 3]);
    const size_t chunkSize = static_cast<size_t>(length) + 12u;
    CHECK(offset + chunkSize <= bytes.size());
    if (offset + chunkSize > bytes.size()) {
      return bytes;
    }

    const uint32_t crc = computePngCrc(bytes.data() + offset + 4u, static_cast<size_t>(length) + 4u);
    const size_t crcOffset = offset + 8u + static_cast<size_t>(length);
    bytes[crcOffset] = static_cast<unsigned char>((crc >> 24u) & 0xFFu);
    bytes[crcOffset + 1] = static_cast<unsigned char>((crc >> 16u) & 0xFFu);
    bytes[crcOffset + 2] = static_cast<unsigned char>((crc >> 8u) & 0xFFu);
    bytes[crcOffset + 3] = static_cast<unsigned char>(crc & 0xFFu);
    offset += chunkSize;
  }

  return bytes;
}

std::vector<unsigned char> withCorruptedFirstPngChunkCrc(std::vector<unsigned char> bytes) {
  bytes = withValidPngCrcs(bytes);
  if (bytes.size() >= 33) {
    bytes[29] ^= 0xFFu;
  }
  return bytes;
}

std::string escapeStringLiteral(const std::string &text);

std::string injectEscapedPath(std::string source, const std::string &escapedPath) {
  const std::string needle = "__PATH__";
  const size_t pos = source.find(needle);
  CHECK(pos != std::string::npos);
  if (pos == std::string::npos) {
    return source;
  }
  source.replace(pos, needle.size(), escapedPath);
  return source;
}

std::string makeDapFrame(const std::string &payload) {
  return "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n" + payload;
}

std::vector<std::string> parseDapFrames(const std::string &raw, bool &ok) {
  ok = true;
  std::vector<std::string> payloads;
  size_t pos = 0;
  while (pos < raw.size()) {
    const size_t headerEnd = raw.find("\r\n\r\n", pos);
    if (headerEnd == std::string::npos) {
      ok = false;
      return {};
    }
    const std::string headers = raw.substr(pos, headerEnd - pos);
    pos = headerEnd + 4;

    size_t contentLength = 0;
    bool sawContentLength = false;
    size_t headerPos = 0;
    while (headerPos <= headers.size()) {
      const size_t lineEnd = headers.find("\r\n", headerPos);
      const size_t splitEnd = lineEnd == std::string::npos ? headers.size() : lineEnd;
      const std::string line = headers.substr(headerPos, splitEnd - headerPos);
      const size_t colon = line.find(':');
      if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
          return static_cast<char>(std::tolower(c));
        });
        std::string value = line.substr(colon + 1);
        value.erase(value.begin(),
                    std::find_if(value.begin(), value.end(), [](unsigned char c) { return !std::isspace(c); }));
        value.erase(
            std::find_if(value.rbegin(), value.rend(), [](unsigned char c) { return !std::isspace(c); }).base(),
            value.end());
        if (key == "content-length") {
          sawContentLength = true;
          try {
            contentLength = static_cast<size_t>(std::stoull(value));
          } catch (...) {
            ok = false;
            return {};
          }
        }
      }
      if (lineEnd == std::string::npos) {
        break;
      }
      headerPos = lineEnd + 2;
    }

    if (!sawContentLength || pos + contentLength > raw.size()) {
      ok = false;
      return {};
    }
    payloads.push_back(raw.substr(pos, contentLength));
    pos += contentLength;
  }
  return payloads;
}

bool hasZipTools() {
  return runCommand("zip -v > /dev/null 2>&1") == 0 && runCommand("unzip -v > /dev/null 2>&1") == 0;
}

bool hasSpirvTools() {
  return runCommand("glslangValidator -v > /dev/null 2>&1") == 0 ||
         runCommand("glslc --version > /dev/null 2>&1") == 0;
}

bool hasWasmtime() {
  return runCommand("wasmtime --version > /dev/null 2>&1") == 0;
}

bool hasPython3() {
  return runCommand("python3 --version > /dev/null 2>&1") == 0;
}

bool createZip(const std::filesystem::path &zipPath, const std::filesystem::path &sourceDir) {
  const std::string command = "cd " + quoteShellArg(sourceDir.string()) + " && zip -q -r " +
                              quoteShellArg(zipPath.string()) + " .";
  return runCommand(command) == 0;
}
} // namespace

#include "test_compile_run_smoke.h"
#include "test_compile_run_vm_core.h"
#include "test_compile_run_vm_uninitialized.h"
#include "test_compile_run_vm_maybe.h"
#include "test_compile_run_vm_collections.h"
#include "test_compile_run_vm_math.h"
#include "test_compile_run_vm_maps.h"
#include "test_compile_run_vm_bounds.h"
#include "test_compile_run_vm_outputs.h"
#include "test_compile_run_vm_gpu.h"
#include "test_compile_run_emitters.h"
#include "test_compile_run_reflection_codegen.h"
#include "test_compile_run_glsl.h"
#include "test_compile_run_native_backend_core.h"
#include "test_compile_run_native_backend_uninitialized.h"
#include "test_compile_run_native_backend_maybe.h"
#include "test_compile_run_native_backend_argv.h"
#include "test_compile_run_native_backend_control.h"
#include "test_compile_run_native_backend_pointers.h"
#include "test_compile_run_native_backend_math_numeric.h"
#include "test_compile_run_native_backend_collections.h"
#include "test_compile_run_native_backend_imports.h"
#include "test_compile_run_benchmark_harness.h"
#include "test_compile_run_bindings_and_examples.h"
#include "test_compile_run_imports.h"
#include "test_compile_run_text_filters.h"
#include "test_compile_run_math_conformance.h"
