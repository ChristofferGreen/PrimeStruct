#pragma once

#include <string>
#include <vector>

namespace primec {

class ProcessRunner {
public:
  virtual ~ProcessRunner() = default;
  virtual int run(const std::vector<std::string> &args) const = 0;
};

const ProcessRunner &systemProcessRunner();

} // namespace primec
