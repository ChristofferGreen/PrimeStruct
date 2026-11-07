// /std/ui/events (docs/spec/stdlib-reference.md): the event values control handlers receive, built and read on the VM,
// the native backend and optexe.
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

TEST_SUITE_BEGIN("primestruct.backend.ui_events");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;
const std::string Fixture = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/ui/events.prime";

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("ui_events");
  std::filesystem::create_directories(dir);
  return (dir / name).string();
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string shellQuote(const std::string &text) {
  return "'" + text + "'";
}

int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Builds the fixture with `--emit=<emit>` and runs it; returns its exit code.
int buildAndRun(const std::string &emit) {
  const std::string exe = scratch("events_" + emit);
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=" + emit + " " + shellQuote(Fixture) +
                         " -o " + shellQuote(exe) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  return run(shellQuote(exe));
}
} // namespace

TEST_CASE("ui events build, read and compare on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixture);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK_MESSAGE(result.exitCode == 0,
                "events check " << result.exitCode << " failed (tests/fixtures/ui/events.prime)");
}

TEST_CASE("ui events build, read and compare on the native backend") {
  const int failed = buildAndRun("native");
  CHECK_MESSAGE(failed == 0,
                "events check " << failed << " failed (tests/fixtures/ui/events.prime)");
}

TEST_CASE("ui events build, read and compare through optexe") {
  const int failed = buildAndRun("optexe");
  CHECK_MESSAGE(failed == 0,
                "events check " << failed << " failed (tests/fixtures/ui/events.prime)");
}

TEST_SUITE_END();
