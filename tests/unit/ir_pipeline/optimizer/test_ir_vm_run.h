#pragma once

#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"
#include "primec/testing/TestScratch.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

// Runs a module in the VM and captures what it prints, for differential tests.
namespace optimizer_test {

struct Outcome {
  bool ok = false;
  uint64_t result = 0;
  std::string error;
  std::string output;

  bool operator==(const Outcome &other) const {
    return ok == other.ok && result == other.result && error == other.error && output == other.output;
  }
};

#if defined(__unix__) || defined(__APPLE__)
// Captures what the VM writes to stdout while alive.
class StdoutCapture {
public:
  explicit StdoutCapture(const std::string &path) : path_(path) {
    std::fflush(stdout);
    saved_ = ::dup(1);
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
      ::dup2(fd, 1);
      ::close(fd);
    }
  }
  std::string finish() {
    std::fflush(stdout);
    if (saved_ >= 0) {
      ::dup2(saved_, 1);
      ::close(saved_);
      saved_ = -1;
    }
    std::ifstream in(path_);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
  }
  ~StdoutCapture() {
    if (saved_ >= 0) {
      finish();
    }
  }

private:
  std::string path_;
  int saved_ = -1;
};
#endif

inline Outcome run(const primec::IrModule &module, const std::vector<std::string_view> &args = {}) {
  static const std::string capturePath =
      primec::testing::testScratchPath("optimizer_vm/stdout.txt").string();
  std::filesystem::create_directories(std::filesystem::path(capturePath).parent_path());
  Outcome outcome;
  StdoutCapture capture(capturePath);
  primec::Vm vm;
  outcome.ok = vm.execute(module, outcome.result, outcome.error, args);
  outcome.output = capture.finish();
  return outcome;
}

} // namespace optimizer_test
