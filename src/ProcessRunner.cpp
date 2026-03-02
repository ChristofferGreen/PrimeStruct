#include "primec/ProcessRunner.h"

#include <cstdlib>

namespace primec {
namespace {

class SystemProcessRunner final : public ProcessRunner {
public:
  int run(const std::string &command) const override {
    return std::system(command.c_str());
  }
};

} // namespace

const ProcessRunner &systemProcessRunner() {
  static const SystemProcessRunner runner;
  return runner;
}

} // namespace primec
