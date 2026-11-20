#include "primec/support/ProcessRunner.h"
#include "primec/support/ProcessCpuTime.h"
#include "primec/support/ProgressBeat.h"

#include <cerrno>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

// iOS (and any embedding build with PRIMESTRUCT_EMBED_NO_PROCESS) cannot spawn
// processes: posix_spawn/waitpid are unavailable or sandboxed, so the runner
// below reports ENOSYS instead of referencing them.
#if defined(PRIMESTRUCT_NO_PROCESS) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define PRIMESTRUCT_PROCESS_SPAWNING_UNAVAILABLE 1
#endif

#if !defined(PRIMESTRUCT_PROCESS_SPAWNING_UNAVAILABLE)
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include <vector>

extern char **environ;
#endif

namespace primec {
namespace {

#if defined(PRIMESTRUCT_PROCESS_SPAWNING_UNAVAILABLE)
class SystemProcessRunner final : public ProcessRunner {
public:
  int run(const std::vector<std::string> &args) const override {
    (void)args;
    return ENOSYS;
  }
};
#else

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

// Waits for `pid`. Outside the test watchdog this is one blocking waitpid. Behind it, the work loop of a test is
// waiting on a program (`clang++` of a native build) that gives it no step to beat at, and a loaded gate stretches
// that one wait past the silence limit; the loop beats whenever the CPU time of the program and its children (the
// compiler and linker `clang++` starts) advanced since the last look,
// so a program that is running keeps the test alive and one that stopped does not (the hard cap still ends one that
// runs forever). Never a timer: nothing beats when the child made no progress.
int waitForChild(pid_t pid, const std::string &tool, int &status) {
  if (progressBeatDescriptor() < 0) {
    return waitpid(pid, &status, 0) < 0 ? -1 : 0;
  }
  using Clock = std::chrono::steady_clock;
  std::uint64_t lastCpu = 0;
  (void)processTreeCpuTime(pid, lastCpu);
  Clock::time_point lastLook = Clock::now();
  std::chrono::milliseconds pause{2};
  for (;;) {
    const pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      return 0;
    }
    if (result < 0) {
      return -1;
    }
    std::this_thread::sleep_for(pause);
    if (pause < std::chrono::milliseconds(100)) {
      pause *= 2;
    }
    if (Clock::now() - lastLook >= std::chrono::seconds(1)) {
      lastLook = Clock::now();
      std::uint64_t cpu = 0;
      if (processTreeCpuTime(pid, cpu) && cpu > lastCpu) {
        lastCpu = cpu;
        emitProgressBeat("primec: " + tool + " is running");
      }
    }
  }
}

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

    emitProgressBeat("primec: run " + effectiveArgs.front());
    pid_t pid = 0;
    const int spawnResult =
        posix_spawnp(&pid, effectiveArgs.front().c_str(), nullptr, nullptr, argv.data(), ::environ);
    if (spawnResult != 0) {
      return spawnResult;
    }

    int status = 0;
    if (waitForChild(pid, effectiveArgs.front(), status) != 0) {
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

#endif // PRIMESTRUCT_PROCESS_SPAWNING_UNAVAILABLE

} // namespace

bool processSpawningAvailable() {
#if defined(PRIMESTRUCT_PROCESS_SPAWNING_UNAVAILABLE)
  return false;
#else
  return true;
#endif
}

const ProcessRunner &systemProcessRunner() {
  static const SystemProcessRunner runner;
  return runner;
}

} // namespace primec
