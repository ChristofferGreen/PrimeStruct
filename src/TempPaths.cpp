#include "primec/TempPaths.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#elif defined(_WIN32)
#include <process.h>
#endif

namespace primec {
namespace {

unsigned long currentProcessId() {
#if defined(__unix__) || defined(__APPLE__)
  return static_cast<unsigned long>(::getpid());
#elif defined(_WIN32)
  return static_cast<unsigned long>(::_getpid());
#else
  return 0ul;
#endif
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

std::string normalizeExtension(std::string_view extension) {
  if (extension.empty()) {
    return {};
  }
  if (extension.front() == '.') {
    return std::string(extension);
  }
  return "." + std::string(extension);
}

void ensureDirectoryExists(const std::filesystem::path &dir) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
}

} // namespace

std::filesystem::path primecTempRoot() {
  static const std::filesystem::path root = [] {
    std::error_code ec;
    std::filesystem::path baseDir = std::filesystem::temp_directory_path(ec);
    if (ec || baseDir.empty()) {
      baseDir = std::filesystem::current_path();
    }
    std::filesystem::path rootDir = baseDir / "primec";
    ensureDirectoryExists(rootDir);
    return rootDir;
  }();
  ensureDirectoryExists(root);
  return root;
}

std::filesystem::path primecCacheDir(std::string_view category, std::string_view key) {
  return primecTempRoot() / std::string(category) / std::string(key);
}

std::filesystem::path primecUniqueTempFile(std::string_view prefix, std::string_view extension) {
  static std::atomic<uint64_t> counter{0};
  const uint64_t id = counter.fetch_add(1, std::memory_order_relaxed) + 1u;
  const std::string nonce = std::to_string(currentProcessId()) + "|" +
                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "|" +
                            std::to_string(id) + "|" + std::string(prefix) + "|" + std::string(extension);
  return primecTempRoot() / (std::string(prefix) + "_" + hex64(fnv1a64(nonce)) + normalizeExtension(extension));
}

} // namespace primec
