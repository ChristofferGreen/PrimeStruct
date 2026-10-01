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

// False when this build cannot spawn processes (iOS, PRIMESTRUCT_EMBED_NO_PROCESS);
// the system runner then returns ENOSYS for every command.
bool processSpawningAvailable();

} // namespace primec
