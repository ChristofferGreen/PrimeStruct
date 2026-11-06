// POSIX host side of /std/os (stdlib/std/os/os.prime), linked into an --emit=optexe program with --link-arg.
#include <signal.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>

namespace {

volatile sig_atomic_t stopFlag = 0;

void onStopSignal(int) { stopFlag = 1; }

// SIGTERM and SIGINT set the flag. Without SA_RESTART a blocking call (accept, recv, poll) returns EINTR, so a
// server loop wakes up and sees stopRequested().
struct StopHandlers {
  StopHandlers() {
    struct sigaction action = {};
    action.sa_handler = onStopSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);
  }
};

const StopHandlers installStopHandlers;

} // namespace

extern "C" {

const char *ps_os_env_or(const char *name, const char *fallback) {
  const char *value = std::getenv(name);
  return value != nullptr ? value : fallback;
}

int64_t ps_os_monotonic_ms(void) {
  timespec now = {};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<int64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}

bool ps_os_stop_requested(void) { return stopFlag != 0; }

int32_t ps_os_log_line(const char *line) {
  std::string text(line);
  text.push_back('\n');
  size_t written = 0;
  while (written < text.size()) {
    const ssize_t count = write(STDERR_FILENO, text.data() + written, text.size() - written);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    written += static_cast<size_t>(count);
  }
  return static_cast<int32_t>(written);
}
}
