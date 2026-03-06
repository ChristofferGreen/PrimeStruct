#include "primec/ProcessRunner.h"

#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>

extern char **environ;

namespace primec {
namespace {

class SystemProcessRunner final : public ProcessRunner {
public:
  int run(const std::vector<std::string> &args) const override {
    if (args.empty()) {
      return EINVAL;
    }

    std::vector<char *> argv;
    argv.reserve(args.size() + 1);
    for (const std::string &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = 0;
    const int spawnResult = posix_spawnp(&pid, args.front().c_str(), nullptr, nullptr, argv.data(), ::environ);
    if (spawnResult != 0) {
      return spawnResult;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
      return errno == 0 ? ECHILD : errno;
    }

    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
      return 128 + WTERMSIG(status);
    }
    return status;
  }
};

} // namespace

const ProcessRunner &systemProcessRunner() {
  static const SystemProcessRunner runner;
  return runner;
}

} // namespace primec
