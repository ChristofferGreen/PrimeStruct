#include "../test_compile_run_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

#include <unistd.h>

namespace {
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

// TODO-5357: the cache key is the emitted C++ text plus the host compiler
// identity, not the primec binary. Relinking primec for an unrelated change
// used to invalidate every fixture and force the (minutes-long) host compile
// again; the executable only depends on the C++ it is built from.
std::string hostCompilerCommand() {
  if (runCommand("c++ --version > /dev/null 2>&1") == 0) {
    return "c++";
  }
  if (runCommand("clang++ --version > /dev/null 2>&1") == 0) {
    return "clang++";
  }
  return {};
}

std::string hostCompilerIdentity(const std::string &cxx) {
  const std::filesystem::path versionPath =
      std::filesystem::current_path() / ".primec_test_cache" / ("compiler_version_" + std::to_string(::getpid()) + ".txt");
  std::filesystem::create_directories(versionPath.parent_path());
  std::string identity = cxx;
  if (runCommand(cxx + " --version > " + quoteShellArg(versionPath.string()) + " 2>&1") == 0) {
    identity += "|" + readFile(versionPath.string());
  }
  std::error_code ec;
  std::filesystem::remove(versionPath, ec);
  return identity;
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
} // namespace

static bool emitCppFixtureSource(const std::string &srcPath, const std::string &cppPath) {
  const std::string emitCppCmd =
      "./primec --emit=cpp " + quoteShellArg(srcPath) + " -o " + quoteShellArg(cppPath) + " --entry /main";
  return runCommand(emitCppCmd) == 0;
}

static bool compileEmittedCppAtO0(const std::string &cxx, const std::string &cppPath, const std::string &exePath) {
  // Compile to a temp path in the same directory and rename into place only
  // once the executable is fully written. Other processes racing on the
  // same cache key (see buildCachedEmittedCppExecutableAtO0's exists()
  // checks, and acquireCacheBuildLock's exists() check above) treat
  // exePath's mere existence as "build complete" - if the compiler wrote
  // exePath directly, a concurrent reader could see (and try to execute) a
  // partially-written file, since std::filesystem::rename is atomic on the
  // same filesystem but a compiler's own -o output is not.
  const std::filesystem::path finalExePath(exePath);
  const std::filesystem::path tempExePath =
      finalExePath.parent_path() /
      (finalExePath.filename().string() + ".building." + std::to_string(::getpid()));
  std::error_code removeTempEc;
  std::filesystem::remove(tempExePath, removeTempEc);

  const std::string compileCmd =
      cxx + " -std=c++23 -O0 " + quoteShellArg(cppPath) + " -o " + quoteShellArg(tempExePath.string());
  if (runCommand(compileCmd) != 0) {
    std::filesystem::remove(tempExePath, removeTempEc);
    return false;
  }

  std::error_code renameEc;
  std::filesystem::rename(tempExePath, finalExePath, renameEc);
  if (renameEc) {
    std::filesystem::remove(tempExePath, removeTempEc);
    return false;
  }
  return true;
}

bool buildCachedEmittedCppExecutableAtO0(const std::string &fixtureName,
                                         const std::string &source,
                                         std::string &exePathOut) {
  const std::string cxx = hostCompilerCommand();
  if (cxx.empty()) {
    return false;
  }
  const std::filesystem::path cacheDir = emittedCppFixtureCacheDir();
  // Emit first (cheap) so the cache key can follow the generated C++.
  const std::string sourceKey = fixtureName + "_" + hex64(fnv1a64(source));
  const std::filesystem::path srcPath = cacheDir / (sourceKey + ".prime");
  const std::filesystem::path emittedPath =
      cacheDir / (sourceKey + ".emit." + std::to_string(::getpid()) + ".cpp");
  writeTextFile(srcPath, source);
  if (!emitCppFixtureSource(srcPath.string(), emittedPath.string())) {
    return false;
  }
  const std::string cppText = readFile(emittedPath.string());
  std::error_code removeEc;
  std::filesystem::remove(emittedPath, removeEc);

  const std::string cacheKey =
      fixtureName + "_" + hex64(fnv1a64("emitted-cpp-cache-v2\n" + hostCompilerIdentity(cxx) + "\n" + cppText));
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

  writeTextFile(cppPath, cppText);
  return compileEmittedCppAtO0(cxx, cppPath.string(), exePath.string());
}
