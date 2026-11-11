// /std/http and /std/html (docs/spec/stdlib-reference.md): the request parser, responses, the router and the HTML
// writer on the VM and optexe.
#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.backend.http");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;
const std::string Fixtures = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/http/";

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("http");
  std::filesystem::create_directories(dir);
  return (dir / name).string();
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string shellQuote(const std::string &text) { return "'" + text + "'"; }

int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Runs the fixture `name`.prime on the VM; returns its exit code.
int runOnVm(const std::string &name) {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + name + ".prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  return result.exitCode;
}

// Builds the fixture `name`.prime with --emit=optexe and runs it; returns its exit code.
int runThroughOptexe(const std::string &name) {
  const std::string exe = scratch(name);
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(Fixtures + name + ".prime") + " -o " +
                         shellQuote(exe) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  return run(shellQuote(exe));
}

// A client socket to 127.0.0.1:`port` with 5 s send and receive timeouts, or -1.
int connectWithTimeouts(int port) {
  const int client = socket(AF_INET, SOCK_STREAM, 0);
  if (client < 0) {
    return -1;
  }
  timeval timeout = {};
  timeout.tv_sec = 5;
  setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(client, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    close(client);
    return -1;
  }
  return client;
}

// Everything the server sends until it closes the connection (or the 5 s timeout).
std::string readAll(int client) {
  std::string response;
  char buffer[512];
  ssize_t count = 0;
  while ((count = recv(client, buffer, sizeof(buffer), 0)) > 0) {
    response.append(buffer, static_cast<size_t>(count));
  }
  return response;
}

// Sends `request` on a new connection and returns the whole response.
std::string roundTrip(int port, const std::string &request) {
  const int client = connectWithTimeouts(port);
  if (client < 0) {
    return "connect failed";
  }
  send(client, request.data(), request.size(), 0);
  std::string response = readAll(client);
  close(client);
  return response;
}

// Sends `request` on a new connection and returns how long the whole response took; the response goes to `response`.
std::chrono::milliseconds
timedRoundTrip(int port, const std::string &request, std::string &response) {
  const auto start = std::chrono::steady_clock::now();
  response = roundTrip(port, request);
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                               start);
}

bool socketsPermitted() {
  const int probe = socket(AF_INET, SOCK_STREAM, 0);
  if (probe < 0) {
    return !(errno == EPERM || errno == EACCES);
  }
  close(probe);
  return true;
}

// Builds `source` (a path) with --emit=optexe and both shims; returns the executable path.
std::string buildServer(const std::string &source, const std::string &name) {
  const std::string netShim = std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib/std/net/native/net_shim.cpp";
  const std::string osShim = std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib/std/os/os_posix.cpp";
  const std::string exe = scratch(name);
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(source) + " -o " + shellQuote(exe) +
                         " --link-arg " + shellQuote(netShim) + " --link-arg " + shellQuote(osShim) + " > " +
                         shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  return exe;
}

struct RunningServer {
  pid_t pid = -1;
  int port = 0;
  std::string errors;
};

// Starts `exe` with stderr in a file and waits up to 5 s for "listening on <port>".
RunningServer startServer(const std::string &exe, const std::string &environment) {
  RunningServer server;
  server.errors = exe + ".err";
  server.pid = fork();
  REQUIRE(server.pid >= 0);
  if (server.pid == 0) {
    const int errorFd = open(server.errors.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(errorFd, STDERR_FILENO);
    if (!environment.empty()) {
      putenv(const_cast<char *>(environment.c_str()));
    }
    execl(exe.c_str(), exe.c_str(), static_cast<char *>(nullptr));
    _exit(127);
  }
  for (int attempt = 0; attempt < 200 && server.port == 0; ++attempt) {
    const std::string text = readFile(server.errors);
    const size_t at = text.find("listening on ");
    if (at != std::string::npos && text.find('\n', at) != std::string::npos) {
      server.port = std::atoi(text.c_str() + at + 13);
    } else {
      int status = 0;
      if (waitpid(server.pid, &status, WNOHANG) == server.pid) {
        server.pid = -1;
        server.port = WIFEXITED(status) ? -WEXITSTATUS(status) : -1;
        break;
      }
      usleep(25000);
    }
  }
  return server;
}

// Sends SIGTERM and returns the exit code (-1 if it did not exit normally).
int stopServer(RunningServer &server) {
  if (server.pid <= 0) {
    return -1;
  }
  kill(server.pid, SIGTERM);
  int status = 0;
  REQUIRE(waitpid(server.pid, &status, 0) == server.pid);
  server.pid = -1;
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
} // namespace

// The matrix compiles to one program; most of its time (about 10 s in a release build) is the frontend and lowering
// of the parser, which one split would pay twice.
TEST_CASE("the request parser accepts good requests and rejects malformed ones on the VM") {
  const int failed = runOnVm("parse_matrix");
  CHECK_MESSAGE(failed == 0, "parse_matrix case " << failed << " failed (tests/fixtures/http/parse_matrix.prime)");
}

TEST_CASE("the request parser gives the same outcomes through optexe") {
  const int failed = runThroughOptexe("parse_matrix");
  CHECK_MESSAGE(failed == 0, "parse_matrix case " << failed << " failed (tests/fixtures/http/parse_matrix.prime)");
}

TEST_CASE("responses serialize byte for byte on the VM") {
  const int failed = runOnVm("response_bytes");
  CHECK_MESSAGE(failed == 0, "response_bytes case " << failed << " failed (tests/fixtures/http/response_bytes.prime)");
}

TEST_CASE("responses serialize byte for byte through optexe") {
  const int failed = runThroughOptexe("response_bytes");
  CHECK_MESSAGE(failed == 0, "response_bytes case " << failed << " failed (tests/fixtures/http/response_bytes.prime)");
}

TEST_CASE("an html page with a form and its response are byte for byte on the VM") {
  const int failed = runOnVm("html_page");
  CHECK_MESSAGE(failed == 0,
                "html_page case " << failed << " failed (tests/fixtures/http/html_page.prime)");
}

TEST_CASE("an html page with a form and its response are byte for byte through optexe") {
  const int failed = runThroughOptexe("html_page");
  CHECK_MESSAGE(failed == 0,
                "html_page case " << failed << " failed (tests/fixtures/http/html_page.prime)");
}

TEST_CASE("a Router dispatches by method and path on the VM") {
  const int failed = runOnVm("router_dispatch");
  CHECK_MESSAGE(failed == 0, "router_dispatch case " << failed << " failed (tests/fixtures/http/router_dispatch.prime)");
}

TEST_CASE("a Router dispatches by method and path through optexe") {
  const int failed = runThroughOptexe("router_dispatch");
  CHECK_MESSAGE(failed == 0, "router_dispatch case " << failed << " failed (tests/fixtures/http/router_dispatch.prime)");
}

TEST_CASE("Router path parameters match one segment each on the VM") {
  const int failed = runOnVm("router_params");
  CHECK_MESSAGE(failed == 0,
                "router_params case " << failed
                                      << " failed (tests/fixtures/http/router_params.prime)");
}

TEST_CASE("Router path parameters match the same through optexe") {
  const int failed = runThroughOptexe("router_params");
  CHECK_MESSAGE(failed == 0,
                "router_params case " << failed
                                      << " failed (tests/fixtures/http/router_params.prime)");
}

TEST_CASE("an AppRouter passes its state to every route on the VM") {
  const int failed = runOnVm("app_router");
  CHECK_MESSAGE(failed == 0,
                "app_router case " << failed << " failed (tests/fixtures/http/app_router.prime)");
}

TEST_CASE("an AppRouter passes its state to every route through optexe") {
  const int failed = runThroughOptexe("app_router");
  CHECK_MESSAGE(failed == 0,
                "app_router case " << failed << " failed (tests/fixtures/http/app_router.prime)");
}

TEST_CASE("query and form parameters decode and reject malformed values on the VM") {
  const int failed = runOnVm("query_params");
  CHECK_MESSAGE(failed == 0,
                "query_params case " << failed
                                     << " failed (tests/fixtures/http/query_params.prime)");
}

TEST_CASE("query and form parameters decode the same through optexe") {
  const int failed = runThroughOptexe("query_params");
  CHECK_MESSAGE(failed == 0,
                "query_params case " << failed
                                     << " failed (tests/fixtures/http/query_params.prime)");
}

TEST_CASE("serve answers routed, missing, malformed, oversized and silent clients over loopback") {
  if (!socketsPermitted()) {
    MESSAGE("skipped: socket() is not permitted here (EPERM or EACCES)");
    return;
  }
  const std::string exe = buildServer(Fixtures + "http_server.prime", "http_server");
  RunningServer server = startServer(exe, "");
  if (server.port < 0 && readFile(server.errors).find("not permitted") != std::string::npos) {
    MESSAGE("skipped: the server may not listen here: " << readFile(server.errors));
    return;
  }
  REQUIRE_MESSAGE(server.port > 0, readFile(server.errors));

  const std::string hello = roundTrip(server.port, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
  CHECK_MESSAGE(hello == "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 6\r\nConnection: close\r\n\r\n"
                         "hello\n",
                hello);
  const std::string missing = roundTrip(server.port, "GET /missing HTTP/1.1\r\n\r\n");
  CHECK_MESSAGE(missing.rfind("HTTP/1.1 404 Not Found\r\n", 0) == 0, missing);
  const std::string wrongMethod = roundTrip(server.port, "POST / HTTP/1.1\r\n\r\n");
  CHECK_MESSAGE(wrongMethod.rfind("HTTP/1.1 405 Method Not Allowed\r\n", 0) == 0, wrongMethod);
  const std::string malformed = roundTrip(server.port, "NOT HTTP\r\n\r\n");
  CHECK_MESSAGE(malformed == "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", malformed);
  // The body would exceed maxRequestBytes (1024): rejected from the head alone.
  const std::string oversized = roundTrip(server.port, "POST /echo HTTP/1.1\r\nContent-Length: 5000\r\n\r\n");
  CHECK_MESSAGE(oversized.rfind("HTTP/1.1 413 Content Too Large\r\n", 0) == 0, oversized);
  // A head that never ends fills the buffer: 413 as well.
  const std::string endless = roundTrip(server.port, "GET / HTTP/1.1\r\nX-Pad: " + std::string(1100, 'a'));
  CHECK_MESSAGE(endless.rfind("HTTP/1.1 413 Content Too Large\r\n", 0) == 0, endless);

  // A client that sends nothing is cut off by readTimeoutMs (300 ms) and the server keeps serving.
  const int silent = connectWithTimeouts(server.port);
  REQUIRE(silent >= 0);
  const auto start = std::chrono::steady_clock::now();
  const std::string nothing = readAll(silent);
  const auto waited = std::chrono::steady_clock::now() - start;
  close(silent);
  CHECK(nothing.empty());
  CHECK(waited < std::chrono::seconds(3));

  const std::string echoed = roundTrip(server.port, "POST /echo HTTP/1.1\r\nContent-Length: 2\r\n\r\nhi");
  CHECK_MESSAGE(echoed.find("\r\n\r\n1 hi") != std::string::npos, echoed);
  const std::string again = roundTrip(server.port, "GET / HTTP/1.1\r\n\r\n");
  CHECK_MESSAGE(again.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, again);

  CHECK(stopServer(server) == 0);
}

// Browsers open spare connections and send nothing on them. The loop waits on every open connection at once, so a
// silent one holds only its own slot (TODO-5705).
TEST_CASE("an idle connection does not delay other clients over loopback") {
  if (!socketsPermitted()) {
    MESSAGE("skipped: socket() is not permitted here (EPERM or EACCES)");
    return;
  }
  const std::string exe = buildServer(Fixtures + "http_idle_server.prime", "http_idle_server");
  RunningServer server = startServer(exe, "");
  if (server.port < 0 && readFile(server.errors).find("not permitted") != std::string::npos) {
    MESSAGE("skipped: the server may not listen here: " << readFile(server.errors));
    return;
  }
  REQUIRE_MESSAGE(server.port > 0, readFile(server.errors));
  const std::string get = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";

  // Each attempt opens a silent connection first, gives the server time to accept it, then times a GET. The read
  // timeout is 1500 ms, so a loop that waited on the silent connection would take that long every time.
  std::chrono::milliseconds fastest(100000);
  std::chrono::milliseconds slowest(0);
  for (int attempt = 0; attempt < 3; ++attempt) {
    const int idle = connectWithTimeouts(server.port);
    REQUIRE(idle >= 0);
    usleep(50000);
    std::string hello;
    const std::chrono::milliseconds took = timedRoundTrip(server.port, get, hello);
    CHECK_MESSAGE(hello.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, hello);
    fastest = std::min(fastest, took);
    slowest = std::max(slowest, took);
    // The silent connection is still open and is answered once it sends its request.
    send(idle, get.data(), get.size(), 0);
    const std::string late = readAll(idle);
    close(idle);
    CHECK_MESSAGE(late.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, late);
  }
  CHECK_MESSAGE(fastest < std::chrono::milliseconds(100),
                "fastest GET took " << fastest.count() << " ms");
  CHECK_MESSAGE(slowest < std::chrono::milliseconds(1000),
                "slowest GET took " << slowest.count() << " ms");

  // A request that arrives in two parts does not hold up a client that sends its request at once.
  const int slow = connectWithTimeouts(server.port);
  REQUIRE(slow >= 0);
  send(slow, "GET / HTTP/1.1\r\n", 16, 0);
  usleep(50000);
  std::string quick;
  const std::chrono::milliseconds took = timedRoundTrip(server.port, get, quick);
  CHECK_MESSAGE(quick.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, quick);
  CHECK_MESSAGE(took < std::chrono::milliseconds(1000),
                "GET beside a slow client took " << took.count() << " ms");
  send(slow, "\r\n", 2, 0);
  const std::string finished = readAll(slow);
  close(slow);
  CHECK_MESSAGE(finished.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, finished);

  // With both slots (maxConnections 2) held by silent clients, a new client waits in the backlog until the read
  // timeout drops one of them, and is then served.
  const int first = connectWithTimeouts(server.port);
  const int second = connectWithTimeouts(server.port);
  REQUIRE(first >= 0);
  REQUIRE(second >= 0);
  usleep(50000);
  std::string queued;
  const std::chrono::milliseconds waited = timedRoundTrip(server.port, get, queued);
  CHECK_MESSAGE(queued.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, queued);
  CHECK_MESSAGE(waited >= std::chrono::milliseconds(1000),
                "a GET with no free slot took " << waited.count() << " ms");
  CHECK_MESSAGE(waited < std::chrono::milliseconds(3000),
                "a GET with no free slot took " << waited.count() << " ms");
  CHECK(readAll(first).empty());
  CHECK(readAll(second).empty());
  close(first);
  close(second);

  CHECK(stopServer(server) == 0);
}

namespace {
// A server whose "listening on <port>" line is read from a pipe: the wait for it ends when the line arrives or the
// server exits, not after a time limit. A server that is still running when the test leaves is killed.
struct PipedServer {
  pid_t pid = -1;
  int port = 0;
  int errorFd = -1;
  std::string errors;

  PipedServer() = default;
  PipedServer(const PipedServer &) = delete;
  PipedServer &operator=(const PipedServer &) = delete;

  ~PipedServer() {
    if (pid > 0) {
      kill(pid, SIGKILL);
      int status = 0;
      waitpid(pid, &status, 0);
    }
    if (errorFd >= 0) {
      close(errorFd);
    }
  }

  // Starts `exe` with `environment` ("NAME=value" entries) added to this process's.
  void start(const std::string &exe, std::vector<std::string> environment) {
    int ends[2] = {-1, -1};
    REQUIRE(pipe(ends) == 0);
    pid = fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
      close(ends[0]);
      dup2(ends[1], STDERR_FILENO);
      close(ends[1]);
      for (std::string &entry : environment) {
        putenv(entry.data());
      }
      execl(exe.c_str(), exe.c_str(), static_cast<char *>(nullptr));
      _exit(127);
    }
    close(ends[1]);
    errorFd = ends[0];
    char byte = 0;
    while (errors.find('\n') == std::string::npos && read(errorFd, &byte, 1) == 1) {
      errors.push_back(byte);
    }
    const size_t at = errors.find("listening on ");
    port = at == std::string::npos ? -1 : std::atoi(errors.c_str() + at + 13);
  }

  // Whether the server has exited; it is reaped when it has.
  bool exited() {
    int status = 0;
    if (pid > 0 && waitpid(pid, &status, WNOHANG) == pid) {
      pid = -1;
    }
    return pid <= 0;
  }

  // Waits for the server to exit and returns its exit code (-1 if it did not exit normally).
  int waitForExit() {
    if (pid <= 0) {
      return -1;
    }
    int status = 0;
    REQUIRE(waitpid(pid, &status, 0) == pid);
    pid = -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  }

  int stop() {
    if (pid > 0) {
      kill(pid, SIGTERM);
    }
    return waitForExit();
  }
};

// A client socket to 127.0.0.1:`port` that waits as long as the server takes, or -1.
int connectAndWait(int port) {
  const int client = socket(AF_INET, SOCK_STREAM, 0);
  if (client < 0) {
    return -1;
  }
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(client, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    close(client);
    return -1;
  }
  return client;
}

// Sends `request` on a new connection and returns the whole response, however long the server takes.
std::string exchangeAndWait(int port, const std::string &request) {
  const int client = connectAndWait(port);
  if (client < 0) {
    return "connect failed";
  }
  send(client, request.data(), request.size(), 0);
  std::string response = readAll(client);
  close(client);
  return response;
}

// The body of an HTTP response.
std::string bodyOf(const std::string &response) {
  const size_t at = response.find("\r\n\r\n");
  return at == std::string::npos ? std::string("no body: ") + response : response.substr(at + 4);
}

// Builds the worker server fixture (once per process) and starts it; false when sockets are not permitted here.
bool startWorkerServer(PipedServer &server, std::vector<std::string> environment) {
  if (!socketsPermitted()) {
    MESSAGE("skipped: socket() is not permitted here (EPERM or EACCES)");
    return false;
  }
  static const std::string exe =
      buildServer(Fixtures + "http_workers_server.prime", "http_workers_server");
  server.start(exe, std::move(environment));
  if (server.port < 0 && server.errors.find("not permitted") != std::string::npos) {
    MESSAGE("skipped: the server may not listen here: " << server.errors);
    return false;
  }
  REQUIRE_MESSAGE(server.port > 0, server.errors);
  return true;
}

// Compiles a copy of the example app `name` whose `main` calls `serveWorkers` where the example calls `serve`; the
// rest of the example, its handlers included, is unchanged. Returns primec's exit code; its output goes to `log`.
int compileExampleWithWorkers(const std::string &name, std::string &log) {
  std::string source =
      readFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/apps/" + name + "/main.prime");
  const std::string call = "  serve(withRequestLog(";
  const std::string effects =
      "effects(heap_alloc, io_err, net_io) on_error<ServeError, log_serve_error>]\nmain()";
  const size_t callAt = source.find(call);
  REQUIRE(callAt != std::string::npos);
  source.replace(callAt, call.size(), "  serveWorkers(withRequestLog(");
  const size_t effectsAt = source.find(effects);
  REQUIRE(effectsAt != std::string::npos);
  source.replace(effectsAt,
                 effects.size(),
                 "effects(heap_alloc, io_err, net_io, task, lock, channel, atomic) "
                 "on_error<ServeError, log_serve_error>]\nmain()");
  const std::string copy = scratch(name + "_workers.prime");
  std::ofstream(copy, std::ios::binary) << source;
  const std::string logPath = copy + ".log";
  // optcpp: the whole compiler up to the C++ text, without the C++ compiler's seconds.
  const int status = run(shellQuote(Primec) + " --emit=optcpp " + shellQuote(copy) + " -o " +
                         shellQuote(copy + ".cpp") + " > " + shellQuote(logPath) + " 2>&1");
  log = readFile(logPath);
  return status;
}
} // namespace

// None of the serveWorkers cases below measures time: each waits for the server's own answers, in an order only a
// correct server can produce.
TEST_CASE("serveWorkers with 4 workers counts every POST of 16 concurrent clients") {
  PipedServer server;
  if (!startWorkerServer(server, {"WORKERS=4"})) {
    return;
  }
  const std::string post = "POST /hit HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n";
  std::atomic<int> answered{0};
  std::vector<std::thread> clients;
  for (int client = 0; client < 16; ++client) {
    clients.emplace_back([&] {
      for (int request = 0; request < 50; ++request) {
        const std::string response = exchangeAndWait(server.port, post);
        if (response.rfind("HTTP/1.1 200 OK\r\n", 0) == 0 && bodyOf(response) == "ok") {
          answered.fetch_add(1);
        }
      }
    });
  }
  for (std::thread &client : clients) {
    client.join();
  }
  CHECK(answered.load() == 800);
  // The counter is a plain field that every handler changes inside the app's mutex.
  const std::string count =
      exchangeAndWait(server.port, "GET /count HTTP/1.1\r\nHost: localhost\r\n\r\n");
  CHECK_MESSAGE(bodyOf(count) == "800", count);
  CHECK(server.stop() == 0);
}

TEST_CASE("serveWorkers with 1 worker answers others while a client stalls in the middle of its "
          "request") {
  PipedServer server;
  if (!startWorkerServer(server, {"WORKERS=1"})) {
    return;
  }
  const std::string get = "GET /count HTTP/1.1\r\nHost: localhost\r\n\r\n";
  const int stalled = connectAndWait(server.port);
  REQUIRE(stalled >= 0);
  const std::string half = "POST /hit HTTP/1.1\r\nHost: localhost\r\nContent-Le";
  send(stalled, half.data(), half.size(), 0);
  // These are answered while the stalled client is still connected (it is answered itself below): the one worker
  // gives a connection back as soon as it has read what arrived.
  CHECK_MESSAGE(bodyOf(exchangeAndWait(server.port, get)) == "0",
                "first GET beside the stalled client");
  const std::string hit =
      exchangeAndWait(server.port, "POST /hit HTTP/1.1\r\nContent-Length: 0\r\n\r\n");
  CHECK_MESSAGE(bodyOf(hit) == "ok", hit);
  CHECK_MESSAGE(bodyOf(exchangeAndWait(server.port, get)) == "1",
                "second GET beside the stalled client");
  const std::string rest = "ngth: 0\r\n\r\n";
  send(stalled, rest.data(), rest.size(), 0);
  const std::string late = readAll(stalled);
  close(stalled);
  CHECK_MESSAGE(bodyOf(late) == "ok", late);
  CHECK_MESSAGE(bodyOf(exchangeAndWait(server.port, get)) == "2",
                "GET after the stalled client finished");
  CHECK(server.stop() == 0);
}

TEST_CASE("SIGTERM stops serveWorkers from listening while its worker is busy") {
  if (!socketsPermitted()) {
    MESSAGE("skipped: socket() is not permitted here (EPERM or EACCES)");
    return;
  }
  // The handler of GET /hold connects here and waits for a byte, so the test decides how long the worker is busy.
  const int holdListener = socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(holdListener >= 0);
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = 0;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  socklen_t length = sizeof(address);
  const bool listening =
      bind(holdListener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0 &&
      listen(holdListener, 4) == 0 &&
      getsockname(holdListener, reinterpret_cast<sockaddr *>(&address), &length) == 0;
  if (!listening) {
    MESSAGE("skipped: the test may not listen here");
    close(holdListener);
    return;
  }
  PipedServer server;
  if (!startWorkerServer(server,
                         {"WORKERS=1", "HOLD_PORT=" + std::to_string(ntohs(address.sin_port))})) {
    close(holdListener);
    return;
  }
  const int held = connectAndWait(server.port);
  REQUIRE(held >= 0);
  const std::string hold = "GET /hold HTTP/1.1\r\nHost: localhost\r\n\r\n";
  send(held, hold.data(), hold.size(), 0);
  // The handler has connected: the only worker is inside it from here on.
  const int release = accept(holdListener, nullptr, nullptr);
  REQUIRE(release >= 0);
  const int idle = connectAndWait(server.port);
  REQUIRE(idle >= 0);

  kill(server.pid, SIGTERM);
  // The poller closes the connections no worker holds: the idle one ends (or is reset, if it was still waiting to
  // be accepted) although the worker has not finished.
  char byte = 0;
  CHECK(recv(idle, &byte, 1, 0) <= 0);
  close(idle);
  // It stopped listening before that, so nothing new connects, and it has not exited: it waits for its worker.
  const int refused = connectAndWait(server.port);
  CHECK(refused < 0);
  if (refused >= 0) {
    close(refused);
  }
  CHECK(!server.exited());

  // The request the worker is answering is finished, not cut off.
  CHECK(send(release, "x", 1, 0) == 1);
  const std::string answer = readAll(held);
  close(held);
  close(release);
  close(holdListener);
  CHECK_MESSAGE(answer.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, answer);
  CHECK_MESSAGE(bodyOf(answer) == "held", answer);
  CHECK(server.waitForExit() == 0);
}

TEST_CASE("serveWorkers rejects an app whose state is not sendable") {
  const std::string log = scratch("workers_not_sendable.log");
  const int status = run(
      shellQuote(Primec) + " --emit=optcpp " + shellQuote(Fixtures + "workers_not_sendable.prime") +
      " -o " + shellQuote(scratch("workers_not_sendable.cpp")) + " > " + shellQuote(log) + " 2>&1");
  CHECK(status != 0);
  const std::string diagnostics = readFile(log);
  CHECK_MESSAGE(
      diagnostics.find(
          "closure cannot be a Function: capture `inner`, field `state` of WebApp<Borrowed>, "
          "field `bytes` of Borrowed is Pointer<u8>") != std::string::npos,
      diagnostics);
}

TEST_CASE("the hello_button example compiles with serveWorkers and unchanged handlers") {
  std::string log;
  CHECK_MESSAGE(compileExampleWithWorkers("hello_button", log) == 0, log);
}

TEST_CASE("the todo_web example compiles with serveWorkers and unchanged handlers") {
  std::string log;
  CHECK_MESSAGE(compileExampleWithWorkers("todo_web", log) == 0, log);
}

TEST_SUITE_END();
