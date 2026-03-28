#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"

#include "primec/testing/EmitterHelpers.h"

#include <chrono>
#include <iomanip>
#include <thread>

static bool buildEmittedCppExecutableAtO0(const std::string &srcPath,
                                          const std::string &cppPath,
                                          const std::string &exePath);

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
} // namespace

#include "test_compile_run_emitters_chunks/test_compile_run_emitters_01.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_02.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_03.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_04.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_05.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_06.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_07.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_08.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_09.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_10.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_11.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_12.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_13.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_14.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_15.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_16.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_17.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_18.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_19.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_20.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_21.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_22.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_23.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_24.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_25.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_26.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_27.h"
#include "test_compile_run_emitters_chunks/test_compile_run_emitters_28.h"
