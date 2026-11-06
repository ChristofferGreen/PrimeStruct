// /std/http (docs/spec/stdlib-reference.md, HTTP): the request parser, responses and the router on the VM and optexe.
#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

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

TEST_CASE("a Router dispatches by method and path on the VM") {
  const int failed = runOnVm("router_dispatch");
  CHECK_MESSAGE(failed == 0, "router_dispatch case " << failed << " failed (tests/fixtures/http/router_dispatch.prime)");
}

TEST_CASE("a Router dispatches by method and path through optexe") {
  const int failed = runThroughOptexe("router_dispatch");
  CHECK_MESSAGE(failed == 0, "router_dispatch case " << failed << " failed (tests/fixtures/http/router_dispatch.prime)");
}

TEST_SUITE_END();
