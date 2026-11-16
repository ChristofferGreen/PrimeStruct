// /std/build (docs/spec/stdlib-reference.md): the build graph value and its JSON export on the VM and through optexe.
// The fixture tests/fixtures/build/build_graph.prime builds one graph in two call orders, checks the exports are equal,
// checks the problem report and that the String heap is balanced, and prints the export, which this test compares with
// tests/fixtures/build/build_graph.golden.json. The script tests/fixtures/build/build_script.prime is a build script
// that runs under `--profile build` (effect ceiling file_read and heap_alloc, stdlib import allowlist).
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_graph");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;
const std::string FixtureDir = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/build/";

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("build_graph");
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
} // namespace

TEST_CASE("the build graph export matches the golden file on the VM") {
  const std::string out = scratch("vm_export.json");
  const int failed =
      run(shellQuote(Primec) + " --emit=vm " + shellQuote(FixtureDir + "build_graph.prime") +
          " > " + shellQuote(out) + " 2> " + shellQuote(out + ".err"));
  CHECK_MESSAGE(failed == 0,
                "build_graph case " << failed << " failed (100 and 101 are heap leaks); "
                                    << readFile(out + ".err"));
  CHECK(readFile(out) == readFile(FixtureDir + "build_graph.golden.json"));
}

TEST_CASE("the build graph export matches the golden file through optexe") {
  const std::string exe = scratch("build_graph");
  const std::string log = exe + ".log";
  const int status =
      run(shellQuote(Primec) + " --emit=optexe " + shellQuote(FixtureDir + "build_graph.prime") +
          " -o " + shellQuote(exe) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  const std::string out = exe + ".json";
  const int failed = run(shellQuote(exe) + " > " + shellQuote(out));
  CHECK_MESSAGE(failed == 0,
                "build_graph case " << failed << " failed (100 and 101 are heap leaks)");
  CHECK(readFile(out) == readFile(FixtureDir + "build_graph.golden.json"));
}

TEST_CASE("a build script runs under the build profile with heap_alloc only") {
  const std::string log = scratch("build_script.log");
  // The script builds four nodes and returns their count as the exit code.
  const int nodes =
      run(shellQuote(Primec) + " --profile build --emit=vm " +
          shellQuote(FixtureDir + "build_script.prime") + " > " + shellQuote(log) + " 2>&1");
  CHECK_MESSAGE(nodes == 4, readFile(log));
}

TEST_SUITE_END();
