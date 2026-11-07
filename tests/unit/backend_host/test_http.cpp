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

#include <cerrno>
#include <chrono>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

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

TEST_SUITE_END();
