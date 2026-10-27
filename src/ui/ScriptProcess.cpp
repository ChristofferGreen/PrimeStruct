// Runs a script in its own process (include/primec/ui/ScriptRunner.h).

#include "primec/ui/ScriptRunner.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <filesystem>
#include <mutex>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern char **environ;

struct PsScriptRun {
  std::string name;
  std::string text;
  std::string stdlibDir;
  std::string runner;
  PsScriptOutput output = nullptr;
  PsScriptFinished finished = nullptr;
  void *user = nullptr;
  std::atomic<bool> stopRequested{false};
  std::atomic<pid_t> pid{0};
  std::thread worker;
};

namespace {

std::mutex errorMutex;
std::string lastError;

void setError(std::string text) {
  std::lock_guard<std::mutex> lock(errorMutex);
  lastError = std::move(text);
}

// Where the runner executable is: PRIMESTRUCT_SCRIPT_RUNNER, then PrimeScriptRunner beside the library or program this code
// is in (the bundle's Contents/MacOS), then the build tree's copy (development and tests).
std::string findRunner() {
  if (const char *fromEnv = std::getenv("PRIMESTRUCT_SCRIPT_RUNNER"); fromEnv != nullptr && *fromEnv != '\0') {
    return fromEnv;
  }
  Dl_info info{};
  if (dladdr(reinterpret_cast<void *>(&ps_script_start), &info) != 0 && info.dli_fname != nullptr) {
    const std::filesystem::path beside = std::filesystem::path(info.dli_fname).parent_path() / "PrimeScriptRunner";
    std::error_code ignored;
    if (std::filesystem::exists(beside, ignored)) {
      return beside.string();
    }
  }
#ifdef PRIMESTRUCT_SCRIPT_RUNNER_BUILD_PATH
  {
    std::error_code ignored;
    if (std::filesystem::exists(PRIMESTRUCT_SCRIPT_RUNNER_BUILD_PATH, ignored)) {
      return PRIMESTRUCT_SCRIPT_RUNNER_BUILD_PATH;
    }
  }
#endif
  return {};
}

void emit(PsScriptRun &run, const std::string &text) {
  if (!text.empty() && run.output != nullptr) {
    run.output(1, text.data(), text.size(), run.user);
  }
}

void finish(PsScriptRun &run, int status, int exitCode) {
  if (run.finished != nullptr) {
    run.finished(status, exitCode, run.user);
  }
}

bool writeAll(int fd, const std::string &text) {
  size_t done = 0;
  while (done < text.size()) {
    const ssize_t wrote = ::write(fd, text.data() + done, text.size() - done);
    if (wrote < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    done += static_cast<size_t>(wrote);
  }
  return true;
}

void execute(PsScriptRun &run) {
  int toChild[2] = {-1, -1};
  int fromChild[2] = {-1, -1};
  if (::pipe(toChild) != 0 || ::pipe(fromChild) != 0) {
    emit(run, "The script could not be started: no pipe.\n");
    finish(run, 1, 0);
    return;
  }
  for (const int fd : {toChild[0], toChild[1], fromChild[0], fromChild[1]}) {
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
  }
  // The runner's stdin is the text, and its stdout and stderr are one pipe, so the output stays in program order. The
  // standard stdlib location is passed through the environment.
  std::vector<std::string> environment;
  for (char **entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
    environment.emplace_back(*entry);
  }
  if (!run.stdlibDir.empty()) {
    environment.push_back("PRIMESTRUCT_STDLIB=" + run.stdlibDir);
  }
  std::vector<char *> envp;
  for (std::string &entry : environment) {
    envp.push_back(entry.data());
  }
  envp.push_back(nullptr);
  std::vector<std::string> arguments = {run.runner, "--script-host", run.name};
  std::vector<char *> argv;
  for (std::string &argument : arguments) {
    argv.push_back(argument.data());
  }
  argv.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, toChild[0], 0);
  posix_spawn_file_actions_adddup2(&actions, fromChild[1], 1);
  posix_spawn_file_actions_adddup2(&actions, fromChild[1], 2);
  pid_t pid = 0;
  const int spawned = posix_spawn(&pid, run.runner.c_str(), &actions, nullptr, argv.data(), envp.data());
  posix_spawn_file_actions_destroy(&actions);
  ::close(toChild[0]);
  ::close(fromChild[1]);
  if (spawned != 0) {
    ::close(toChild[1]);
    ::close(fromChild[0]);
    emit(run, std::string("The script could not be started: ") + std::strerror(spawned) + ".\n");
    finish(run, 1, 0);
    return;
  }
  run.pid.store(pid);
  if (run.stopRequested.load()) {
    ::kill(pid, SIGTERM); // stopped before it was running
  }
  // The runner reads all of its input before it runs anything, so writing it first cannot block on its output.
  writeAll(toChild[1], run.text);
  ::close(toChild[1]);

  char buffer[4096];
  for (;;) {
    const ssize_t got = ::read(fromChild[0], buffer, sizeof(buffer));
    if (got > 0) {
      if (run.output != nullptr) {
        run.output(1, buffer, static_cast<size_t>(got), run.user);
      }
    } else if (got == 0 || errno != EINTR) {
      break;
    }
  }
  ::close(fromChild[0]);
  int waitStatus = 0;
  while (::waitpid(pid, &waitStatus, 0) < 0 && errno == EINTR) {
  }
  run.pid.store(0);
  if (WIFSIGNALED(waitStatus)) {
    if (run.stopRequested.load()) {
      emit(run, "[stopped]\n");
      finish(run, 2, 0);
    } else {
      emit(run, "[the script's process ended with signal " + std::to_string(WTERMSIG(waitStatus)) + "]\n");
      finish(run, 1, 0);
    }
    return;
  }
  const int code = WIFEXITED(waitStatus) ? WEXITSTATUS(waitStatus) : -1;
  if (code != 0) {
    emit(run, "[the script runner exited with code " + std::to_string(code) + "]\n");
    finish(run, 1, code);
    return;
  }
  finish(run, run.stopRequested.load() ? 2 : 0, 0);
}

} // namespace

extern "C" {

PsScriptRun *ps_script_start(const char *name, const char *text, const char *stdlibDir, PsScriptOutput output,
                             PsScriptFinished finished, void *user) {
  const std::string runner = findRunner();
  if (runner.empty()) {
    setError("PrimeScriptRunner was not found (it belongs beside the program, or set PRIMESTRUCT_SCRIPT_RUNNER)");
    return nullptr;
  }
  auto *run = new PsScriptRun;
  run->name = name != nullptr ? name : "script.prime";
  run->text = text != nullptr ? text : "";
  run->stdlibDir = stdlibDir != nullptr ? stdlibDir : "";
  run->runner = runner;
  run->output = output;
  run->finished = finished;
  run->user = user;
  run->worker = std::thread([run] { execute(*run); });
  return run;
}

void ps_script_stop(PsScriptRun *run) {
  if (run == nullptr) {
    return;
  }
  run->stopRequested.store(true);
  if (const pid_t pid = run->pid.load(); pid > 0) {
    ::kill(pid, SIGTERM);
  }
}

void ps_script_release(PsScriptRun *run) {
  if (run == nullptr) {
    return;
  }
  ps_script_stop(run);
  // A process that ignores SIGTERM is ended for good after a moment, so the join cannot hang.
  std::thread escalator([run] {
    for (int i = 0; i < 30 && run->pid.load() > 0; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (const pid_t pid = run->pid.load(); pid > 0) {
      ::kill(pid, SIGKILL);
    }
  });
  run->worker.join();
  escalator.join();
  delete run;
}

const char *ps_script_last_error(void) {
  static thread_local std::string copy;
  std::lock_guard<std::mutex> lock(errorMutex);
  copy = lastError;
  return copy.c_str();
}

} // extern "C"
