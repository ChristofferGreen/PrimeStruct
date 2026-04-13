#include "test_compile_run_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

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
} // namespace

bool buildEmittedCppExecutableAtO0(const std::string &srcPath,
                                   const std::string &cppPath,
                                   const std::string &exePath) {
  std::string cxx = "clang++";
  if (runCommand("c++ --version > /dev/null 2>&1") == 0) {
    cxx = "c++";
  } else if (runCommand("clang++ --version > /dev/null 2>&1") != 0) {
    return false;
  }

  const std::string emitCppCmd =
      "./primec --emit=cpp " + quoteShellArg(srcPath) + " -o " + quoteShellArg(cppPath) + " --entry /main";
  if (runCommand(emitCppCmd) != 0) {
    return false;
  }

  const std::string compileCmd =
      cxx + " -std=c++23 -O0 " + quoteShellArg(cppPath) + " -o " + quoteShellArg(exePath);
  return runCommand(compileCmd) == 0;
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
