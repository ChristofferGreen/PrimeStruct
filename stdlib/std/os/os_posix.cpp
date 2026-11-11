// POSIX host side of /std/os (stdlib/std/os/os.prime), linked into an --emit=optexe program with --link-arg.
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>

namespace {

volatile sig_atomic_t stopFlag = 0;

// The write end of the stop pipe (ps_os_stop_signal_fd), or -1 while nobody has asked for it.
std::atomic<int> stopPipeWriter{-1};

// Makes the stop pipe readable. Only async-signal-safe calls: the signal handler runs it.
void writeStopByte(int writer) {
  const int saved = errno;
  const uint8_t byte = 1;
  // A full pipe is already readable, so a failed write needs no retry.
  (void)!write(writer, &byte, 1);
  errno = saved;
}

void onStopSignal(int) {
  stopFlag = 1;
  const int writer = stopPipeWriter.load(std::memory_order_relaxed);
  if (writer >= 0) {
    writeStopByte(writer);
  }
}

// Opens the stop pipe and returns its read end, or -100 - errno. A signal that arrived before the pipe existed has
// set the flag, so the pipe is made readable here.
int openStopPipe() {
  int ends[2] = {-1, -1};
  if (pipe(ends) != 0) {
    return -100 - errno;
  }
  for (const int fd : ends) {
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  }
  stopPipeWriter.store(ends[1]);
  if (stopFlag != 0) {
    writeStopByte(ends[1]);
  }
  return ends[0];
}

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

// A descriptor that is readable from the moment the process has received SIGTERM or SIGINT, whichever thread took
// the signal: the handler writes to a pipe that is opened on the first call and never closed. Or -100 - errno.
int32_t ps_os_stop_signal_fd(void) {
  static const int reader = openStopPipe();
  return reader;
}

// How many processors are online, at least 1.
int32_t ps_os_cpu_count(void) {
  const long count = sysconf(_SC_NPROCESSORS_ONLN);
  return count < 1 ? 1 : static_cast<int32_t>(count);
}

// Blocks SIGTERM and SIGINT on the calling thread, so the process delivers them to a thread that has not blocked
// them. Returns 0 or -100 - error.
int32_t ps_os_block_stop_signals(void) {
  sigset_t signals;
  sigemptyset(&signals);
  sigaddset(&signals, SIGTERM);
  sigaddset(&signals, SIGINT);
  const int error = pthread_sigmask(SIG_BLOCK, &signals, nullptr);
  return error == 0 ? 0 : -100 - error;
}

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

// The size of a regular file, or -1 (not a regular file) / -100 - errno.
int64_t ps_os_file_size(const char *path) {
  struct stat info = {};
  if (stat(path, &info) != 0) {
    return -100 - errno;
  }
  return S_ISREG(info.st_mode) ? static_cast<int64_t>(info.st_size) : -1;
}

// Reads up to `capacity` bytes of the file into `buffer`; the count read, or -100 - errno.
int64_t ps_os_read_file(const char *path, uint8_t *buffer, uint64_t capacity) {
  const int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return -100 - errno;
  }
  uint64_t total = 0;
  while (total < capacity) {
    const ssize_t count = read(fd, buffer + total, capacity - total);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      const int code = errno;
      close(fd);
      return -100 - code;
    }
    if (count == 0) {
      break;
    }
    total += static_cast<uint64_t>(count);
  }
  close(fd);
  return static_cast<int64_t>(total);
}
}
