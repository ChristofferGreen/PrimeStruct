#include "primec/ProcessRunner.h"

#include <algorithm>
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>

extern char **environ;

namespace primec {
namespace {

#if defined(__APPLE__)
std::vector<std::string> sanitizeProcessArgs(std::vector<std::string> args) {
  if (args.empty()) {
    return args;
  }
  const std::string &tool = args.front();
  const bool isClang = tool == "clang++" || tool.ends_with("/clang++");
  if (!isClang) {
    return args;
  }
  args.erase(std::remove(args.begin() + 1, args.end(), "-O0"), args.end());
  return args;
}
#endif

class SystemProcessRunner final : public ProcessRunner {
public:
  int run(const std::vector<std::string> &args) const override {
    if (args.empty()) {
      return EINVAL;
    }

    std::vector<std::string> effectiveArgs = args;
#if defined(__APPLE__)
    effectiveArgs = sanitizeProcessArgs(std::move(effectiveArgs));
#endif

    std::vector<char *> argv;
    argv.reserve(effectiveArgs.size() + 1);
    for (const std::string &arg : effectiveArgs) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = 0;
    const int spawnResult =
        posix_spawnp(&pid, effectiveArgs.front().c_str(), nullptr, nullptr, argv.data(), ::environ);
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
