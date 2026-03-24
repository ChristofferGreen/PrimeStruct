#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#elif defined(_WIN32)
#include <process.h>
#endif

namespace primec::testing {
namespace detail {
inline unsigned long currentProcessId() {
#if defined(__unix__) || defined(__APPLE__)
  return static_cast<unsigned long>(getpid());
#elif defined(_WIN32)
  return static_cast<unsigned long>(_getpid());
#else
  return 0ul;
#endif
}

inline bool setEnvironmentVariable(const char *name, const std::string &value) {
#if defined(_WIN32)
  return _putenv_s(name, value.c_str()) == 0;
#else
  return setenv(name, value.c_str(), 1) == 0;
#endif
}

inline uint64_t fnv1a64(std::string_view text) {
  uint64_t hash = 14695981039346656037ull;
  for (unsigned char byte : text) {
    hash ^= static_cast<uint64_t>(byte);
    hash *= 1099511628211ull;
  }
  return hash;
}

inline std::string hex64(uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << value;
  return out.str();
}

inline std::filesystem::path createTestScratchRoot() {
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
} // namespace detail

inline const std::filesystem::path &testScratchRoot() {
  static const std::filesystem::path root = detail::createTestScratchRoot();
  return root;
}

inline void cleanupTestScratchRoot() {
  std::error_code ec;
  std::filesystem::remove_all(testScratchRoot(), ec);
}

inline bool ensureTestScratchEnvironment() {
  static const bool configured = [] {
    const std::string scratchRoot = testScratchRoot().string();
    const bool ok = detail::setEnvironmentVariable("TMPDIR", scratchRoot) &&
                    detail::setEnvironmentVariable("TMP", scratchRoot) &&
                    detail::setEnvironmentVariable("TEMP", scratchRoot) &&
                    detail::setEnvironmentVariable("TEMPDIR", scratchRoot);
    std::atexit(cleanupTestScratchRoot);
    return ok;
  }();
  return configured;
}

inline std::filesystem::path testScratchPath(std::string_view relativePath) {
  ensureTestScratchEnvironment();
  const std::filesystem::path path = testScratchRoot() / std::string(relativePath);
  std::filesystem::create_directories(path.parent_path());
  return path;
}

inline std::filesystem::path testScratchDir(std::string_view prefix) {
  ensureTestScratchEnvironment();
  static std::atomic<uint64_t> counter{0};
  const uint64_t id = counter.fetch_add(1, std::memory_order_relaxed) + 1u;
  const std::filesystem::path dir = testScratchRoot() / (std::string(prefix) + "_" + detail::hex64(id));
  std::filesystem::create_directories(dir);
  return dir;
}
} // namespace primec::testing
