#pragma once

#include "primec/testing/TestScratch.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <cstdlib>
#include <iterator>
#include <sys/wait.h>
#include <unistd.h>

inline std::filesystem::path embedTestDir(std::string_view name) {
  return primec::testing::testScratchDir(std::string("embed_") + std::string(name));
}

inline std::filesystem::path embedWriteFile(const std::filesystem::path &path, const std::string &contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  file << contents;
  return path;
}

// Redirects stdout and stderr to a scratch file for the lifetime of the object
// so a test can assert that an API call wrote nothing to either stream.
class EmbedStreamCapture {
public:
  EmbedStreamCapture() : path_(embedTestDir("capture") / "streams.txt") {
    std::fflush(stdout);
    std::fflush(stderr);
    savedOut_ = ::dup(STDOUT_FILENO);
    savedErr_ = ::dup(STDERR_FILENO);
    std::filesystem::create_directories(path_.parent_path());
    FILE *redirected = std::freopen(path_.c_str(), "w", stdout);
    (void)redirected;
    ::dup2(STDOUT_FILENO, STDERR_FILENO);
  }
  ~EmbedStreamCapture() { restore(); }

  // Restores the streams and returns everything written while captured.
  std::string finish() {
    restore();
    std::ifstream in(path_);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }

private:
  void restore() {
    if (savedOut_ < 0) {
      return;
    }
    std::fflush(stdout);
    std::fflush(stderr);
    ::dup2(savedOut_, STDOUT_FILENO);
    ::dup2(savedErr_, STDERR_FILENO);
    ::close(savedOut_);
    ::close(savedErr_);
    savedOut_ = -1;
    savedErr_ = -1;
  }

  std::filesystem::path path_;
  int savedOut_ = -1;
  int savedErr_ = -1;
};

#ifdef PRIMESTRUCT_TEST_PRIMEC_PATH
struct EmbedProcessResult {
  int exitCode = -1;
  std::string output;  // stdout and stderr combined
};

// Runs the primec binary under test with a shell command line suffix.
inline EmbedProcessResult embedRunPrimec(const std::string &arguments) {
  const auto outPath = embedTestDir("primec_run") / "output.txt";
  const std::string command =
      std::string("\"") + PRIMESTRUCT_TEST_PRIMEC_PATH + "\" " + arguments + " > \"" + outPath.string() + "\" 2>&1";
  EmbedProcessResult result;
  const int status = std::system(command.c_str());
  result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  std::ifstream in(outPath);
  result.output.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return result;
}
#endif
