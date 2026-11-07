// /std/web (docs/spec/stdlib-reference.md): controls that take handler closures, their generated routes and the HTML
// they write, on the VM and optexe.
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

TEST_SUITE_BEGIN("primestruct.backend.web");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;
const std::string Fixtures = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/http/";

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("web");
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

// Builds the fixture `name`.prime with --emit=optexe; returns the executable path.
std::string buildOptexe(const std::string &name) {
  const std::string exe = scratch(name);
  const std::string log = exe + ".log";
  const int status =
      run(shellQuote(Primec) + " --emit=optexe " + shellQuote(Fixtures + name + ".prime") + " -o " +
          shellQuote(exe) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  return exe;
}
} // namespace

TEST_CASE("web controls call their click closures through generated routes on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + "web_controls.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK_MESSAGE(result.exitCode == 0,
                "web_controls case " << result.exitCode
                                     << " failed (tests/fixtures/http/web_controls.prime)");
}

TEST_CASE("web controls call their click closures through generated routes through optexe") {
  const std::string exe = buildOptexe("web_controls");
  const int failed = run(shellQuote(exe));
  CHECK_MESSAGE(failed == 0,
                "web_controls case " << failed
                                     << " failed (tests/fixtures/http/web_controls.prime)");
}

TEST_CASE("a duplicate control id stops the program at setup on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + "web_duplicate_control.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  // The panic stops the run with a trap before `main` reaches `return(7i32)`.
  const bool trapped = !result.ok || (result.exitCode != 0 && result.exitCode != 7);
  CHECK_MESSAGE(trapped, "exit code " << result.exitCode);
}

TEST_CASE("a duplicate control id stops the program at setup through optexe") {
  const std::string exe = buildOptexe("web_duplicate_control");
  const std::string errors = exe + ".err";
  const int status = run(shellQuote(exe) + " 2> " + shellQuote(errors));
  CHECK(status != 0);
  CHECK(status != 7);
  const std::string text = readFile(errors);
  CHECK_MESSAGE(text.find("duplicate control id: hello\n") != std::string::npos, text);
}

TEST_SUITE_END();
