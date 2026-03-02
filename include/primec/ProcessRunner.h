#pragma once

#include <string>

namespace primec {

class ProcessRunner {
public:
  virtual ~ProcessRunner() = default;
  virtual int run(const std::string &command) const = 0;
};

const ProcessRunner &systemProcessRunner();

} // namespace primec
