// /std/path (docs/spec/stdlib-reference.md): the textual path functions on the VM and optexe. The fixture
// tests/fixtures/path/path_functions.prime checks every function, and that the String heap is balanced afterwards.
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

TEST_SUITE_BEGIN("primestruct.backend.path");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;
const std::string Fixture = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/path/path_functions.prime";

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("path");
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
} // namespace

TEST_CASE("the path functions answer every case and balance the string heap on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixture);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK_MESSAGE(result.exitCode == 0, "path_functions case " << result.exitCode << " failed (100 is a heap leak)");
}

TEST_CASE("the path functions answer every case and balance the string heap through optexe") {
  const std::string exe = scratch("path_functions");
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(Fixture) + " -o " + shellQuote(exe) +
                         " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  const int failed = run(shellQuote(exe));
  CHECK_MESSAGE(failed == 0, "path_functions case " << failed << " failed (100 is a heap leak)");
}

TEST_SUITE_END();
