// POSIX host side of /std/net (stdlib/std/net/net.prime), linked into an --emit=optexe program with --link-arg.
// TCP over IPv4, one thread, no globals. It installs no signal handlers: /std/os (os_posix.cpp) installs SIGTERM and
// SIGINT without SA_RESTART, so a blocking poll returns EINTR here and the caller sees interruptedCode().
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>

namespace {

constexpr int32_t TimeoutCode = -1;
constexpr int32_t ClosedCode = -2;
constexpr int32_t AgainCode = -3;
constexpr int32_t InterruptedCode = -4;

// errno as a neutral code: the same on every platform for the cases callers act on, -100 - errno otherwise.
int32_t codeOf(int error) {
  switch (error) {
  case ETIMEDOUT:
    return TimeoutCode;
  case ECONNRESET:
  case EPIPE:
  case ENOTCONN:
  case ECONNABORTED:
    return ClosedCode;
  case EAGAIN:
#if EWOULDBLOCK != EAGAIN
  case EWOULDBLOCK:
#endif
    return AgainCode;
  case EINTR:
    return InterruptedCode;
  default:
    return -100 - error;
  }
}

// Writes to a socket whose peer has gone must fail with EPIPE, not raise SIGPIPE.
void noSigPipe(int fd) {
#ifdef SO_NOSIGPIPE
  int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#else
  (void)fd;
#endif
}

#ifdef MSG_NOSIGNAL
constexpr int SendFlags = MSG_NOSIGNAL;
#else
constexpr int SendFlags = 0;
#endif

int newSocket() {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd >= 0) {
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    noSigPipe(fd);
  }
  return fd;
}

// Waits until `fd` is ready for `events`: 0 when ready, else a negative code.
int32_t waitFor(int fd, short events, int32_t timeoutMs) {
  pollfd entry = {};
  entry.fd = fd;
  entry.events = events;
  const int ready = poll(&entry, 1, timeoutMs < 0 ? -1 : timeoutMs);
  if (ready < 0) {
    return codeOf(errno);
  }
  if (ready == 0) {
    return TimeoutCode;
  }
  return 0;
}

sockaddr_in addressOf(int32_t port, uint32_t host) {
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  address.sin_addr.s_addr = htonl(host);
  return address;
}

} // namespace

extern "C" {

int32_t net_listen(int32_t port, int32_t backlog, int32_t loopbackOnly) {
  if (port < 0 || port > 65535) {
    return -100 - EINVAL;
  }
  const int fd = newSocket();
  if (fd < 0) {
    return codeOf(errno);
  }
  int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  const sockaddr_in address = addressOf(port, loopbackOnly != 0 ? INADDR_LOOPBACK : INADDR_ANY);
  if (bind(fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0 ||
      listen(fd, backlog > 0 ? backlog : SOMAXCONN) != 0) {
    const int error = errno;
    close(fd);
    return codeOf(error);
  }
  return fd;
}

int32_t net_port(int32_t fd) {
  sockaddr_in address = {};
  socklen_t length = sizeof(address);
  if (getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length) != 0) {
    return codeOf(errno);
  }
  return ntohs(address.sin_port);
}

int32_t net_accept(int32_t listener, int32_t timeoutMs) {
  const int32_t ready = waitFor(listener, POLLIN, timeoutMs);
  if (ready != 0) {
    return ready;
  }
  const int fd = accept(listener, nullptr, nullptr);
  if (fd < 0) {
    return codeOf(errno);
  }
  fcntl(fd, F_SETFD, FD_CLOEXEC);
  noSigPipe(fd);
  return fd;
}

int64_t net_recv(int32_t fd, uint8_t *buffer, uint64_t length, int32_t timeoutMs) {
  const int32_t ready = waitFor(fd, POLLIN, timeoutMs);
  if (ready != 0) {
    return ready;
  }
  const ssize_t count = recv(fd, buffer, length, 0);
  return count < 0 ? codeOf(errno) : count;
}

int64_t net_send(int32_t fd, uint8_t *data, uint64_t length, int32_t timeoutMs) {
  uint64_t sent = 0;
  while (sent < length) {
    const int32_t ready = waitFor(fd, POLLOUT, timeoutMs);
    if (ready != 0) {
      return sent > 0 ? static_cast<int64_t>(sent) : ready;
    }
    const ssize_t count = send(fd, data + sent, length - sent, SendFlags);
    if (count < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      const int32_t code = codeOf(errno);
      return sent > 0 && code == InterruptedCode ? static_cast<int64_t>(sent) : code;
    }
    sent += static_cast<uint64_t>(count);
  }
  return static_cast<int64_t>(sent);
}

int32_t net_close(int32_t fd) { return close(fd) == 0 ? 0 : codeOf(errno); }

int32_t net_connect(int32_t port, int32_t timeoutMs) {
  if (port <= 0 || port > 65535) {
    return -100 - EINVAL;
  }
  const int fd = newSocket();
  if (fd < 0) {
    return codeOf(errno);
  }
  // Connect without blocking, wait up to the timeout, then put the socket back in blocking mode.
  const int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  const sockaddr_in address = addressOf(port, INADDR_LOOPBACK);
  int32_t result = 0;
  if (connect(fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
    if (errno != EINPROGRESS) {
      result = codeOf(errno);
    } else {
      result = waitFor(fd, POLLOUT, timeoutMs);
      if (result == 0) {
        int error = 0;
        socklen_t length = sizeof(error);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length);
        result = error != 0 ? codeOf(error) : 0;
      }
    }
  }
  if (result != 0) {
    close(fd);
    return result;
  }
  fcntl(fd, F_SETFL, flags);
  return fd;
}
}
