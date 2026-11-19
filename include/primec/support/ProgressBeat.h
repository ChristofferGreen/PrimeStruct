#pragma once

// Progress beats for the test watchdog (docs/TestWatchdog.md).
//
// scripts/test_watchdog.py starts a test command with a pipe whose write end is
// inherited as the descriptor named by PRIMESTRUCT_BEAT_FD. A beat is one line
// written to it by the code that does the work, at the start of each unit of
// work. Outside the wrapper the variable is unset and every call does nothing.

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <string_view>

#if !defined(_WIN32)
#include <csignal>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace primec {

// The longest beat line, newline included. A pipe write this small is atomic,
// so beats from several processes never interleave.
inline constexpr std::size_t MaxProgressBeatBytes = 384;

// Writes one beat line to `fd` when it is a pipe. Returns false when nothing was
// written. A closed reader never raises SIGPIPE in the caller.
inline bool emitProgressBeatTo(int fd, std::string_view label) {
#if defined(_WIN32)
  (void)fd;
  (void)label;
  return false;
#else
  if (fd < 0) {
    return false;
  }
  struct stat info {};
  // The variable can outlive the descriptor in a process that closed its
  // inherited descriptors; never write a beat into a file that reused the number.
  if (fstat(fd, &info) != 0 || !S_ISFIFO(info.st_mode)) {
    return false;
  }
  char line[MaxProgressBeatBytes];
  std::size_t size = label.size() < MaxProgressBeatBytes - 1 ? label.size() : MaxProgressBeatBytes - 1;
  for (std::size_t index = 0; index < size; ++index) {
    const char c = label[index];
    line[index] = (c == '\n' || c == '\r') ? ' ' : c;
  }
  line[size++] = '\n';
#if defined(__APPLE__)
  (void)fcntl(fd, F_SETNOSIGPIPE, 1);
  const ssize_t written = write(fd, line, size);
#else
  sigset_t blocked;
  sigset_t previous;
  sigemptyset(&blocked);
  sigaddset(&blocked, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &blocked, &previous);
  const ssize_t written = write(fd, line, size);
  if (written < 0 && errno == EPIPE) {
    const timespec none{0, 0};
    (void)sigtimedwait(&blocked, nullptr, &none);
  }
  pthread_sigmask(SIG_SETMASK, &previous, nullptr);
#endif
  return written == static_cast<ssize_t>(size);
#endif
}

// The descriptor PRIMESTRUCT_BEAT_FD names, or -1 when it is unset or not a number.
inline int progressBeatDescriptor() {
  const char *text = std::getenv("PRIMESTRUCT_BEAT_FD");
  if (text == nullptr || *text == '\0') {
    return -1;
  }
  char *end = nullptr;
  const long value = std::strtol(text, &end, 10);
  if (end == text || *end != '\0' || value < 0 || value > 1 << 20) {
    return -1;
  }
  return static_cast<int>(value);
}

// Reports that the work loop reached `label`. A no-op outside the watchdog.
inline bool emitProgressBeat(std::string_view label) {
  const int fd = progressBeatDescriptor();
  return fd >= 0 && emitProgressBeatTo(fd, label);
}

} // namespace primec
