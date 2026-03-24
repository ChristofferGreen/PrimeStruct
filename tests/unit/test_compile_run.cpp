#include "third_party/doctest.h"

#include "primec/IrSerializer.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"
#include "primec/testing/EmitterHelpers.h"

#include <algorithm>
#include <atomic>
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
#include <unistd.h>
#elif defined(_WIN32)
#include <process.h>
#endif

static bool buildEmittedCppExecutableAtO0(const std::string &srcPath,
                                          const std::string &cppPath,
                                          const std::string &exePath);

namespace {
const std::filesystem::path &testScratchRoot();
std::filesystem::path testScratchPath(std::string_view relativePath);
std::filesystem::path testScratchDir(std::string_view prefix);

unsigned long currentProcessId() {
#if defined(__unix__) || defined(__APPLE__)
  return static_cast<unsigned long>(getpid());
#elif defined(_WIN32)
  return static_cast<unsigned long>(_getpid());
#else
  return 0ul;
#endif
}

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

bool setEnvironmentVariable(const char *name, const std::string &value) {
#if defined(_WIN32)
  return _putenv_s(name, value.c_str()) == 0;
#else
  return setenv(name, value.c_str(), 1) == 0;
#endif
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

std::filesystem::path createTestScratchRoot() {
  const std::filesystem::path baseDir = std::filesystem::current_path() / ".primec_test_scratch";
  std::error_code ec;
  std::filesystem::create_directories(baseDir, ec);

  const std::string nonceSource =
      std::to_string(currentProcessId()) + "|" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const std::filesystem::path root = baseDir / ("session_" + hex64(fnv1a64(nonceSource)));
  std::filesystem::create_directories(root, ec);
  return root;
}

const std::filesystem::path &testScratchRoot() {
  static const std::filesystem::path root = createTestScratchRoot();
  return root;
}

void cleanupTestScratchRoot() {
  std::error_code ec;
  std::filesystem::remove_all(testScratchRoot(), ec);
}

bool configureTestScratchEnvironment() {
  const std::string scratchRoot = testScratchRoot().string();
  const bool configured = setEnvironmentVariable("TMPDIR", scratchRoot) &&
                          setEnvironmentVariable("TMP", scratchRoot) &&
                          setEnvironmentVariable("TEMP", scratchRoot) &&
                          setEnvironmentVariable("TEMPDIR", scratchRoot);
  std::atexit(cleanupTestScratchRoot);
  return configured;
}

[[maybe_unused]] const bool TestScratchEnvironmentConfigured = configureTestScratchEnvironment();

std::filesystem::path testScratchPath(std::string_view relativePath) {
  const std::filesystem::path path = testScratchRoot() / std::string(relativePath);
  std::filesystem::create_directories(path.parent_path());
  return path;
}

[[maybe_unused]] std::filesystem::path testScratchDir(std::string_view prefix) {
  static std::atomic<uint64_t> counter{0};
  const uint64_t id = counter.fetch_add(1, std::memory_order_relaxed) + 1u;
  const std::filesystem::path dir = testScratchRoot() / (std::string(prefix) + "_" + hex64(id));
  std::filesystem::create_directories(dir);
  return dir;
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

bool isPngChunkTypeByte(unsigned char byte) {
  return std::isalpha(static_cast<unsigned char>(byte)) != 0;
}

bool hasPlausiblePngChunkHeader(const std::vector<unsigned char> &bytes, size_t offset) {
  if (offset + 8u > bytes.size()) {
    return false;
  }
  if (!isPngChunkTypeByte(bytes[offset + 4u]) || !isPngChunkTypeByte(bytes[offset + 5u]) ||
      !isPngChunkTypeByte(bytes[offset + 6u]) || !isPngChunkTypeByte(bytes[offset + 7u])) {
    return false;
  }
  const uint32_t length = (static_cast<uint32_t>(bytes[offset]) << 24u) |
                          (static_cast<uint32_t>(bytes[offset + 1u]) << 16u) |
                          (static_cast<uint32_t>(bytes[offset + 2u]) << 8u) |
                          static_cast<uint32_t>(bytes[offset + 3u]);
  return offset + static_cast<size_t>(length) + 12u <= bytes.size();
}

bool repairPngChunkBoundary(std::vector<unsigned char> &bytes, size_t offset) {
  if (hasPlausiblePngChunkHeader(bytes, offset)) {
    return true;
  }

  for (size_t zerosToErase = 1; zerosToErase <= 4u && offset + zerosToErase <= bytes.size(); ++zerosToErase) {
    if (!std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                     bytes.begin() + static_cast<std::ptrdiff_t>(offset + zerosToErase),
                     [](unsigned char byte) { return byte == 0u; })) {
      break;
    }
    std::vector<unsigned char> candidate = bytes;
    candidate.erase(candidate.begin() + static_cast<std::ptrdiff_t>(offset),
                    candidate.begin() + static_cast<std::ptrdiff_t>(offset + zerosToErase));
    if (hasPlausiblePngChunkHeader(candidate, offset)) {
      bytes = std::move(candidate);
      return true;
    }
  }

  for (size_t zerosToInsert = 1; zerosToInsert <= 4u; ++zerosToInsert) {
    std::vector<unsigned char> candidate = bytes;
    candidate.insert(candidate.begin() + static_cast<std::ptrdiff_t>(offset), zerosToInsert, 0u);
    if (hasPlausiblePngChunkHeader(candidate, offset)) {
      bytes = std::move(candidate);
      return true;
    }
  }

  return false;
}

std::vector<unsigned char> withValidPngCrcs(std::vector<unsigned char> bytes) {
  if (bytes.size() < 8) {
    return bytes;
  }

  size_t offset = 8;
  while (offset < bytes.size()) {
    const bool repaired = repairPngChunkBoundary(bytes, offset);
    CHECK(repaired);
    if (!repaired) {
      return bytes;
    }
    if (offset + 12u > bytes.size()) {
      CHECK(offset == bytes.size());
      return bytes;
    }

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
