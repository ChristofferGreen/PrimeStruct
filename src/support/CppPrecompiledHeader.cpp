#include "primec/support/CppPrecompiledHeader.h"

#include "primec/support/Sha256.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

namespace primec {
namespace {

constexpr size_t MaxCachedHeaders = 8;

unsigned long processId() {
#if defined(__unix__) || defined(__APPLE__)
  return static_cast<unsigned long>(::getpid());
#else
  return 0ul;
#endif
}

std::string uniqueSuffix() {
  static std::atomic<unsigned long> counter{0};
  return std::to_string(processId()) + "-" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

bool readFile(const std::filesystem::path &path, std::string &out) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  out = buffer.str();
  return true;
}

// The compiler's predefined macros under `flags`: they hold its version, the target and every flag that changes
// how a header is read (`__OPTIMIZE__`, the language standard).
bool compilerIdentity(const ProcessRunner &runner,
                      const std::filesystem::path &cacheDir,
                      const std::vector<std::string> &flags,
                      std::string &identity) {
  const std::filesystem::path out = cacheDir / ("identity-" + uniqueSuffix() + ".txt");
  std::vector<std::string> args = {"clang++"};
  args.insert(args.end(), flags.begin(), flags.end());
  args.insert(args.end(), {"-dM", "-E", "-x", "c++", "/dev/null", "-o", out.string()});
  const bool ran = runner.run(args) == 0;
  const bool read = ran && readFile(out, identity) && !identity.empty();
  std::error_code ec;
  std::filesystem::remove(out, ec);
  return read;
}

// Keeps the cache bounded: when more than MaxCachedHeaders headers are there (one per compiler and flag set that
// ever ran), the oldest ones other than `keep` go.
void pruneCache(const std::filesystem::path &cacheDir, const std::filesystem::path &keep) {
  std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> headers;
  std::error_code ec;
  for (const auto &entry : std::filesystem::directory_iterator(cacheDir, ec)) {
    const std::string name = entry.path().filename().string();
    if (name.starts_with("pch-") && name.ends_with(".pch")) {
      std::error_code timeEc;
      headers.emplace_back(entry.last_write_time(timeEc), entry.path());
    }
  }
  if (headers.size() <= MaxCachedHeaders) {
    return;
  }
  std::sort(headers.begin(), headers.end());
  size_t excess = headers.size() - MaxCachedHeaders;
  for (const auto &header : headers) {
    if (excess == 0) {
      break;
    }
    if (header.second != keep) {
      std::filesystem::remove(header.second, ec);
      --excess;
    }
  }
}

} // namespace

std::string cppPrecompiledHeaderKey(std::string_view compilerIdentity,
                                    const std::vector<std::string> &flags,
                                    std::string_view headerText) {
  std::string material;
  material.reserve(compilerIdentity.size() + headerText.size() + 256);
  material += "identity:" + std::to_string(compilerIdentity.size()) + ":";
  material += compilerIdentity;
  material += "\nflags:" + std::to_string(flags.size()) + ":";
  for (const std::string &flag : flags) {
    material += std::to_string(flag.size()) + "=" + flag + ";";
  }
  material += "\nheader:" + std::to_string(headerText.size()) + ":";
  material += headerText;
  return sha256Hex(material).substr(0, 32);
}

std::filesystem::path ensureCppPrecompiledHeader(const ProcessRunner &runner,
                                                 const std::filesystem::path &cacheDir,
                                                 const std::vector<std::string> &flags,
                                                 std::string_view headerText) {
  std::error_code ec;
  std::filesystem::create_directories(cacheDir, ec);
  if (ec || !std::filesystem::is_directory(cacheDir, ec)) {
    return {};
  }
  std::string identity;
  if (!compilerIdentity(runner, cacheDir, flags, identity)) {
    return {};
  }
  const std::string key = cppPrecompiledHeaderKey(identity, flags, headerText);
  const std::filesystem::path finalPath = cacheDir / ("pch-" + key + ".pch");
  if (std::filesystem::is_regular_file(finalPath, ec) && std::filesystem::file_size(finalPath, ec) > 0) {
    return finalPath;
  }

  const std::string suffix = uniqueSuffix();
  const std::filesystem::path headerPath = cacheDir / ("pch-" + key + "-" + suffix + ".h");
  const std::filesystem::path tempPath = cacheDir / ("pch-" + key + "-" + suffix + ".tmp");
  {
    std::ofstream header(headerPath, std::ios::binary);
    header << headerText;
    if (!header) {
      std::filesystem::remove(headerPath, ec);
      return {};
    }
  }
  std::vector<std::string> args = {"clang++"};
  args.insert(args.end(), flags.begin(), flags.end());
  args.insert(args.end(), {"-x", "c++-header", headerPath.string(), "-o", tempPath.string()});
  const bool built = runner.run(args) == 0 && std::filesystem::is_regular_file(tempPath, ec) &&
                     std::filesystem::file_size(tempPath, ec) > 0;
  std::filesystem::remove(headerPath, ec);
  if (!built) {
    std::filesystem::remove(tempPath, ec);
    return {};
  }
  std::filesystem::rename(tempPath, finalPath, ec);
  if (ec) {
    std::filesystem::remove(tempPath, ec);
    return {};
  }
  pruneCache(cacheDir, finalPath);
  return finalPath;
}

bool linkArgsAllowPrecompiledHeader(const std::vector<std::string> &linkArgs) {
  for (size_t i = 0; i < linkArgs.size(); ++i) {
    const std::string &arg = linkArgs[i];
    if (arg == "-framework" || arg == "-rpath") {
      ++i;
      if (i >= linkArgs.size()) {
        return false;
      }
      continue;
    }
    if (arg.starts_with("-l") || arg.starts_with("-L") || arg.starts_with("-Wl,")) {
      continue;
    }
    if (!arg.empty() && arg.front() != '-' &&
        (arg.ends_with(".a") || arg.ends_with(".o") || arg.ends_with(".dylib") || arg.ends_with(".so"))) {
      continue;
    }
    return false;
  }
  return true;
}

} // namespace primec
