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

static bool buildEmittedCppExecutableAtO0(const std::string &srcPath,
                                          const std::string &cppPath,
                                          const std::string &exePath);

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

void writeTextFile(const std::filesystem::path &path, const std::string &contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  CHECK(file.good());
  file << contents;
  CHECK(file.good());
}

uint64_t fnv1a64(std::string_view text) {
  uint64_t hash = 14695981039346656037ull;
  for (unsigned char byte : text) {
    hash ^= static_cast<uint64_t>(byte);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string hex64(uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << value;
  return out.str();
}

std::filesystem::path testScratchPath(std::string_view relativePath) {
  return primec::testing::testScratchPath(relativePath);
}

[[maybe_unused]] std::filesystem::path testScratchDir(std::string_view prefix) {
  return primec::testing::testScratchDir(prefix);
}

std::string emittedCppCacheSalt() {
  constexpr std::string_view CacheVersion = "emitted-cpp-cache-v1";
  const std::filesystem::path primecPath = std::filesystem::current_path() / "primec";
  std::error_code ec;
  const auto primecSize = std::filesystem::file_size(primecPath, ec);
  const auto primecMtime = std::filesystem::last_write_time(primecPath, ec).time_since_epoch().count();
  return std::string(CacheVersion) + "|" + std::to_string(primecSize) + "|" + std::to_string(primecMtime);
}

bool acquireCacheBuildLock(const std::filesystem::path &lockDir, const std::filesystem::path &artifactPath) {
  using namespace std::chrono_literals;

  auto deadline = std::chrono::steady_clock::now() + 300s;
  for (;;) {
    std::error_code ec;
    if (std::filesystem::create_directory(lockDir, ec)) {
      return true;
    }
    if (std::filesystem::exists(artifactPath)) {
      return false;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      std::filesystem::remove_all(lockDir, ec);
      deadline = std::chrono::steady_clock::now() + 300s;
    }
    std::this_thread::sleep_for(100ms);
  }
}

struct CacheBuildLockGuard {
  std::filesystem::path lockDir;

  ~CacheBuildLockGuard() {
    std::error_code ec;
    std::filesystem::remove_all(lockDir, ec);
  }
};

std::filesystem::path emittedCppFixtureCacheDir() {
  const std::filesystem::path cacheDir = std::filesystem::current_path() / ".primec_test_cache";
  std::filesystem::create_directories(cacheDir);
  return cacheDir;
}

bool buildCachedEmittedCppExecutableAtO0(const std::string &fixtureName,
                                         const std::string &source,
                                         std::string &exePathOut) {
  const std::string cacheKey = fixtureName + "_" + hex64(fnv1a64(emittedCppCacheSalt() + "\n" + source));
  const std::filesystem::path cacheDir = emittedCppFixtureCacheDir();
  const std::filesystem::path srcPath = cacheDir / (cacheKey + ".prime");
  const std::filesystem::path cppPath = cacheDir / (cacheKey + ".cpp");
  const std::filesystem::path exePath = cacheDir / cacheKey;
  const std::filesystem::path lockDir = cacheDir / (cacheKey + ".lock");

  exePathOut = exePath.string();
  if (std::filesystem::exists(exePath)) {
    return true;
  }

  if (!acquireCacheBuildLock(lockDir, exePath)) {
    return std::filesystem::exists(exePath);
  }
  CacheBuildLockGuard guard{lockDir};

  if (std::filesystem::exists(exePath)) {
    return true;
  }

  writeTextFile(srcPath, source);
  return buildEmittedCppExecutableAtO0(srcPath.string(), cppPath.string(), exePath.string());
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

bool hasWasmtime() {
  return runCommand("wasmtime --version > /dev/null 2>&1") == 0;
}

int compileWasmWasiProgram(const std::string &srcPath, const std::string &wasmPath, const std::string &errPath) {
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(errPath);
  return runCommand(wasmCmd);
}

void checkWasmWasiRuntimeInDir(const std::filesystem::path &tempRoot,
                               const std::string &wasmPath,
                               const std::string &outPath,
                               int expectedExitCode,
                               const std::string &expectedStdout) {
  if (!hasWasmtime()) {
    return;
  }

  const std::string wasmRunCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                                 quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(wasmRunCmd) == expectedExitCode);
  CHECK(readFile(outPath) == expectedStdout);
}

} // namespace

#include "test_compile_run_smoke.h"
#include "test_compile_run_vm_math.h"
#include "test_compile_run_vm_bounds.h"
#include "test_compile_run_emitters.h"
