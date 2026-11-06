// The `net_io` effect (docs/spec/language-core.md, Effects) and /std/net (docs/spec/stdlib-reference.md, Network).
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

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.backend.net");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("net");
  std::filesystem::create_directories(dir);
  return (dir / name).string();
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void writeFile(const std::string &path, const std::string &text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
}

std::string shellQuote(const std::string &text) { return "'" + text + "'"; }

int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

struct PrimecResult {
  int status = 0;
  std::string output;
};

// Runs primec with `arguments` on `source` written to the scratch file `name`.prime; output holds stdout and stderr.
PrimecResult runPrimec(const std::string &name, const std::string &source, const std::string &arguments) {
  const std::string path = scratch(name + ".prime");
  writeFile(path, source);
  const std::string log = scratch(name + ".log");
  PrimecResult result;
  result.status = run(shellQuote(Primec) + " " + arguments + " " + shellQuote(path) + " > " + shellQuote(log) + " 2>&1");
  result.output = readFile(log);
  return result;
}

const std::string Fixtures = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/host_calls/";
const std::string NetShim = std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib/std/net/native/net_shim.cpp";

// Builds the fixture `name`.prime with --emit=optexe and the /std/net shim; returns the executable path.
std::string buildWithNetShim(const std::string &name) {
  const std::string exe = scratch(name);
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(Fixtures + name + ".prime") + " -o " +
                         shellQuote(exe) + " --link-arg " + shellQuote(NetShim) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  return exe;
}

const std::string NetEffectProgram = R"(
[return<int> effects(net_io)]
main() {
  return(7i32)
}
)";
} // namespace

TEST_CASE("the net_io effect sets bit 12 of the IR effect mask and runs on the VM") {
  const PrimecResult dump = runPrimec("net_effect_dump", NetEffectProgram, "--dump-stage ir-lowered");
  REQUIRE_MESSAGE(dump.status == 0, dump.output);
  CHECK_MESSAGE(dump.output.find("function 0 /main parameters=0 effects=0x1000 ") != std::string::npos, dump.output);

  ScriptEngine engine;
  const auto script = engine.compileSource("/net_effect.prime", NetEffectProgram);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 7);
}

TEST_CASE("wasm and glsl reject the net_io effect") {
  const PrimecResult wasm =
      runPrimec("net_effect_wasm", NetEffectProgram, "--emit=wasm -o " + shellQuote(scratch("net_effect.wasm")));
  CHECK(wasm.status != 0);
  CHECK_MESSAGE(wasm.output.find("unsupported effect mask bits for wasm target") != std::string::npos, wasm.output);

  const PrimecResult browser = runPrimec("net_effect_browser",
                                      NetEffectProgram,
                                      "--emit=wasm --wasm-profile browser -o " +
                                          shellQuote(scratch("net_effect_browser.wasm")));
  CHECK(browser.status != 0);
  CHECK_MESSAGE(browser.output.find("unsupported effect mask bits for wasm-browser target") != std::string::npos,
                browser.output);

  const PrimecResult glsl =
      runPrimec("net_effect_glsl", NetEffectProgram, "--emit=glsl -o " + shellQuote(scratch("net_effect.glsl")));
  CHECK(glsl.status != 0);
  CHECK_MESSAGE(glsl.output.find("unsupported effect mask bits for glsl target") != std::string::npos, glsl.output);
}

TEST_CASE("std/net builds with the POSIX shim on optexe and is unbound on the VM") {
  CHECK(std::filesystem::exists(buildWithNetShim("net_echo")));

  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + "net_echo.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto vm = script.run();
  CHECK_FALSE(vm.ok);
  CHECK_MESSAGE(vm.diagnostics.find("unbound host function: net_listen") != std::string::npos, vm.diagnostics);
}

TEST_CASE("a loopback echo through std/net round-trips bytes with timeouts on every call") {
  // One process, one thread, an ephemeral port on 127.0.0.1 (no firewall prompt, no window), and a timeout on every
  // call, so a broken step fails within seconds instead of hanging. The program's exit code names the failed step
  // (tests/fixtures/host_calls/net_echo.prime); 77 means the environment forbids sockets.
  const std::string exe = buildWithNetShim("net_echo");
  const int exitCode = run(shellQuote(exe));
  if (exitCode == 77) {
    MESSAGE("skipped: socket() is not permitted here (EPERM or EACCES)");
    return;
  }
  CHECK_MESSAGE(exitCode == 0, "net_echo failed at step " << exitCode);
}

TEST_CASE("the http_hello example answers GET / over loopback, logs it and exits 0 on SIGTERM") {
  const int probe = socket(AF_INET, SOCK_STREAM, 0);
  if (probe < 0 && (errno == EPERM || errno == EACCES)) {
    MESSAGE("skipped: socket() is not permitted here (EPERM or EACCES)");
    return;
  }
  REQUIRE(probe >= 0);
  close(probe);

  const std::string example = std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/apps/http_hello/main.prime";
  const std::string osShim = std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib/std/os/os_posix.cpp";
  const std::string exe = scratch("http_hello");
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(example) + " -o " + shellQuote(exe) +
                         " --link-arg " + shellQuote(NetShim) + " --link-arg " + shellQuote(osShim) + " > " +
                         shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));

  // The server logs "listening on <port>" to stderr; PORT=0 picks a free port and BIND=loopback stays on 127.0.0.1.
  const std::string errors = exe + ".err";
  const pid_t pid = fork();
  REQUIRE(pid >= 0);
  if (pid == 0) {
    const int errorFd = open(errors.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(errorFd, STDERR_FILENO);
    setenv("PORT", "0", 1);
    setenv("BIND", "loopback", 1);
    execl(exe.c_str(), exe.c_str(), static_cast<char *>(nullptr));
    _exit(127);
  }
  int port = 0;
  for (int attempt = 0; attempt < 200 && port == 0; ++attempt) {
    const std::string text = readFile(errors);
    const size_t at = text.find("listening on ");
    if (at != std::string::npos && text.find('\n', at) != std::string::npos) {
      port = std::atoi(text.c_str() + at + 13);
    } else {
      usleep(25000);
    }
  }
  std::string response;
  if (port > 0) {
    const int client = socket(AF_INET, SOCK_STREAM, 0);
    timeval timeout = {};
    timeout.tv_sec = 5;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(client, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0) {
      const std::string request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
      CHECK(send(client, request.data(), request.size(), 0) == static_cast<ssize_t>(request.size()));
      char buffer[512];
      ssize_t count = 0;
      while ((count = recv(client, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, static_cast<size_t>(count));
      }
    }
    close(client);
  }
  kill(pid, SIGTERM);
  int waitStatus = 0;
  REQUIRE(waitpid(pid, &waitStatus, 0) == pid);
  REQUIRE_MESSAGE(port > 0, readFile(errors));
  CHECK_MESSAGE(response.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, response);
  CHECK(response.find("\r\n\r\nhello\n") != std::string::npos);
  // withRequestLog writes one line per request to stderr.
  const std::string requestLog = readFile(errors);
  CHECK_MESSAGE(requestLog.find("GET / 200 ") != std::string::npos, requestLog);
  CHECK(WIFEXITED(waitStatus));
  CHECK(WEXITSTATUS(waitStatus) == 0);
}

namespace {
// Sends `request` to 127.0.0.1:`port` and returns everything the server answers before it closes the connection.
std::string exchange(int port, const std::string &request) {
  std::string response;
  const int client = socket(AF_INET, SOCK_STREAM, 0);
  timeval timeout = {};
  timeout.tv_sec = 5;
  setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(client, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0) {
    send(client, request.data(), request.size(), 0);
    char buffer[512];
    ssize_t count = 0;
    while ((count = recv(client, buffer, sizeof(buffer), 0)) > 0) {
      response.append(buffer, static_cast<size_t>(count));
    }
  }
  close(client);
  return response;
}
} // namespace

TEST_CASE("the web_app example serves /healthz, /hello and /echo, logs requests and exits 0 on SIGTERM") {
  const int probe = socket(AF_INET, SOCK_STREAM, 0);
  if (probe < 0 && (errno == EPERM || errno == EACCES)) {
    MESSAGE("skipped: socket() is not permitted here (EPERM or EACCES)");
    return;
  }
  REQUIRE(probe >= 0);
  close(probe);

  const std::string example = std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/apps/web_app/main.prime";
  const std::string osShim = std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib/std/os/os_posix.cpp";
  const std::string exe = scratch("web_app");
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(example) + " -o " + shellQuote(exe) +
                         " --link-arg " + shellQuote(NetShim) + " --link-arg " + shellQuote(osShim) + " > " +
                         shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));

  const std::string errors = exe + ".err";
  const std::string staticDir = scratch("web_app_public");
  std::filesystem::create_directories(staticDir);
  writeFile(staticDir + "/page.html", "<h1>hi</h1>");
  const pid_t pid = fork();
  REQUIRE(pid >= 0);
  if (pid == 0) {
    const int errorFd = open(errors.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(errorFd, STDERR_FILENO);
    setenv("PORT", "0", 1);
    setenv("BIND", "loopback", 1);
    setenv("STATIC_DIR", staticDir.c_str(), 1);
    execl(exe.c_str(), exe.c_str(), static_cast<char *>(nullptr));
    _exit(127);
  }
  int port = 0;
  for (int attempt = 0; attempt < 200 && port == 0; ++attempt) {
    const std::string text = readFile(errors);
    const size_t at = text.find("listening on ");
    if (at != std::string::npos && text.find('\n', at) != std::string::npos) {
      port = std::atoi(text.c_str() + at + 13);
    } else {
      usleep(25000);
    }
  }
  std::string staticFile;
  std::string staticMissing;
  std::string staticEscape;
  std::string health;
  std::string hello;
  std::string helloDefault;
  std::string echo;
  std::string missing;
  std::string wrongMethod;
  if (port > 0) {
    health = exchange(port, "GET /healthz HTTP/1.1\r\nHost: x\r\n\r\n");
    hello = exchange(port, "GET /hello?name=Ada HTTP/1.1\r\nHost: x\r\n\r\n");
    helloDefault = exchange(port, "GET /hello HTTP/1.1\r\nHost: x\r\n\r\n");
    echo = exchange(port, "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: 9\r\n\r\nping=pong");
    missing = exchange(port, "GET /nope HTTP/1.1\r\nHost: x\r\n\r\n");
    wrongMethod = exchange(port, "POST /healthz HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n");
    staticFile = exchange(port, "GET /static/page.html HTTP/1.1\r\nHost: x\r\n\r\n");
    staticMissing = exchange(port, "GET /static/none.html HTTP/1.1\r\nHost: x\r\n\r\n");
    staticEscape = exchange(port, "GET /static/../page.html HTTP/1.1\r\nHost: x\r\n\r\n");
  }
  kill(pid, SIGTERM);
  int waitStatus = 0;
  REQUIRE(waitpid(pid, &waitStatus, 0) == pid);
  REQUIRE_MESSAGE(port > 0, readFile(errors));
  CHECK_MESSAGE(health.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, health);
  CHECK(health.find("\r\n\r\nok\n") != std::string::npos);
  CHECK(hello.find("\r\n\r\nhello, Ada\n") != std::string::npos);
  CHECK(helloDefault.find("\r\n\r\nhello, world\n") != std::string::npos);
  CHECK_MESSAGE(echo.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, echo);
  CHECK(echo.find("\r\n\r\nping=pong") != std::string::npos);
  CHECK(missing.rfind("HTTP/1.1 404", 0) == 0);
  CHECK(wrongMethod.rfind("HTTP/1.1 405", 0) == 0);
  CHECK_MESSAGE(staticFile.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, staticFile);
  CHECK(staticFile.find("Content-Type: text/html; charset=utf-8\r\n") != std::string::npos);
  CHECK(staticFile.find("\r\n\r\n<h1>hi</h1>") != std::string::npos);
  CHECK(staticMissing.rfind("HTTP/1.1 404", 0) == 0);
  CHECK_MESSAGE(staticEscape.rfind("HTTP/1.1 403", 0) == 0, staticEscape);
  const std::string requestLog = readFile(errors);
  CHECK_MESSAGE(requestLog.find("GET /hello?name=Ada 200 ") != std::string::npos, requestLog);
  CHECK(requestLog.find("POST /echo 200 ") != std::string::npos);
  CHECK(requestLog.find("GET /nope 404 ") != std::string::npos);
  CHECK(WIFEXITED(waitStatus));
  CHECK(WEXITSTATUS(waitStatus) == 0);
}

TEST_CASE("serve_file checks paths and reads files, text and binary") {
  const std::string osShim = std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib/std/os/os_posix.cpp";
  const std::string fixture = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/http/files_check.prime";
  const std::string exe = scratch("files_check");
  const std::string log = exe + ".log";
  const std::string root = scratch("files_root");
  std::filesystem::create_directories(root + "/sub");
  writeFile(root + "/a.txt", "hello file\n");
  writeFile(root + "/bin.dat", std::string("\x00\x01\xff\x02", 4));
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(fixture) + " -o " + shellQuote(exe) +
                         " --link-arg " + shellQuote(osShim) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  setenv("FILES_ROOT", root.c_str(), 1);
  const int exitCode = run(shellQuote(exe));
  unsetenv("FILES_ROOT");
  CHECK_MESSAGE(exitCode == 0, "files_check failed at check " << exitCode);
}

TEST_SUITE_END();
