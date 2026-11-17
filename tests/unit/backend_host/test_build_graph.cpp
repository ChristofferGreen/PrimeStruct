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

// The helpers of the export take no vector parameter (a real call cannot), so they are functions of the module
// and `Graph/exportStage` stays small; inlined, it was 40,188 instructions and every `primec build` paid for lowering,
// validating and caching them.
TEST_CASE("the graph export is lowered to real calls on the VM") {
  const std::string out = scratch("vm_lowered.txt");
  const int status = run(shellQuote(Primec) + " --emit=vm --dump-stage ir-lowered " +
                         shellQuote(FixtureDir + "build_graph.prime") + " > " + shellQuote(out) +
                         " 2> " + shellQuote(out + ".err"));
  REQUIRE_MESSAGE(status == 0, readFile(out + ".err"));
  const std::string listing = readFile(out);
  const auto instructionsOf = [&](const std::string &path) -> long {
    const size_t at = listing.find(" " + path + " parameters=");
    if (at == std::string::npos) {
      return -1;
    }
    const size_t eol = listing.find('\n', at);
    const size_t count = listing.find("instructions=", at);
    if (count == std::string::npos || count > eol) {
      return -1;
    }
    return std::strtol(listing.c_str() + count + std::string("instructions=").size(), nullptr, 10);
  };
  for (const char *helper : {"/std/build/Graph/writeNodes",
                             "/std/build/Graph/writeProblems",
                             "/std/build/Graph/sortNodes",
                             "/std/build/Compiler/writeImportPaths"}) {
    CHECK_MESSAGE(instructionsOf(helper) > 0, helper << " is not a function of the module");
  }
  const long exportStage = instructionsOf("/std/build/Graph/exportStage");
  CHECK(exportStage > 0);
  CHECK_MESSAGE(exportStage < 12000, "exportStage has " << exportStage << " instructions");
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

namespace {
// Runs `primec build <arguments>` from `workingDir` (empty: the current directory); the standard output goes to
// `out`, the diagnostics to `out + ".err"`.
int primecBuild(const std::string &arguments,
                const std::string &out,
                const std::string &workingDir = "") {
  const std::string cd =
      workingDir.empty() ? std::string() : "cd " + shellQuote(workingDir) + " && ";
  return run(cd + shellQuote(Primec) + " build " + arguments + " > " + shellQuote(out) + " 2> " +
             shellQuote(out + ".err"));
}
} // namespace

TEST_CASE("primec build --dry-run prints the graph of make.prime") {
  const std::string out = scratch("dry_run.json");
  const int status = primecBuild("--dry-run " + shellQuote(FixtureDir + "project"), out);
  CHECK_MESSAGE(status == 0, readFile(out + ".err"));
  // The graph of a plain program that builds the same nodes prints the same text.
  CHECK(readFile(out) == readFile(FixtureDir + "build_graph.golden.json"));
}

TEST_CASE("primec build --dry-run finds make.prime in the working directory") {
  const std::string out = scratch("dry_run_cwd.json");
  const int status = primecBuild("--dry-run", out, FixtureDir + "project");
  CHECK_MESSAGE(status == 0, readFile(out + ".err"));
  CHECK(readFile(out) == readFile(FixtureDir + "build_graph.golden.json"));
}

namespace {
// A copy of a fixture project in the scratch directory: a run writes its cache (`.ps/cache`) into the project, and the
// checked-in fixtures stay as they are.
std::string projectCopy(const std::string &name) {
  const std::string dir = scratch("project_" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::copy(FixtureDir + name, dir, std::filesystem::copy_options::recursive);
  return dir;
}

// A fresh output directory for one `primec build` run.
std::string freshOutDir(const std::string &name) {
  const std::string dir = scratch(name);
  std::filesystem::remove_all(dir);
  return dir;
}
} // namespace

TEST_CASE("primec build runs two independent steps with --jobs 2") {
  const std::string outDir = freshOutDir("jobs_out");
  const std::string out = scratch("jobs.txt");
  const int status = primecBuild(
      shellQuote(projectCopy("project_jobs")) + " --jobs 2 --out-dir " + shellQuote(outDir), out);
  CHECK_MESSAGE(status == 0, readFile(out + ".err"));
  CHECK(readFile(out) == "built compile:first\nbuilt compile:second\n");
  CHECK(std::filesystem::exists(outDir + "/first"));
  CHECK(std::filesystem::exists(outDir + "/second"));
}

TEST_CASE("primec build names a failing step and stops the steps after it") {
  const std::string outDir = freshOutDir("fail_out");
  const std::string out = scratch("fail.txt");
  const int status = primecBuild(
      shellQuote(projectCopy("project_fail")) + " --jobs 1 --out-dir " + shellQuote(outDir), out);
  CHECK(status != 0);
  const std::string diagnostics = readFile(out + ".err");
  CHECK_MESSAGE(diagnostics.find("step alpha_bad (compile:alpha_bad) failed") != std::string::npos,
                diagnostics);
  CHECK_MESSAGE(diagnostics.find("bad.prime") != std::string::npos, diagnostics);
  CHECK_MESSAGE(diagnostics.find("step beta_good (compile:beta_good) skipped") != std::string::npos,
                diagnostics);
  CHECK(readFile(out).empty());
  CHECK_FALSE(std::filesystem::exists(outDir + "/beta_good"));
}

TEST_CASE("primec build rejects a graph with a library node") {
  const std::string outDir = freshOutDir("library_out");
  const std::string out = scratch("library.txt");
  const int status =
      primecBuild(shellQuote(FixtureDir + "project") + " --out-dir " + shellQuote(outDir), out);
  CHECK(status != 0);
  CHECK(readFile(out + ".err").find("static libraries are not supported by this compiler yet") !=
        std::string::npos);
  CHECK_FALSE(std::filesystem::exists(outDir));
}

TEST_CASE("primec build rejects a bad --jobs") {
  const std::string out = scratch("bad_jobs.txt");
  CHECK(primecBuild(shellQuote(FixtureDir + "project_jobs") + " --jobs zero", out) != 0);
  CHECK(readFile(out + ".err").find("--jobs expects a number") != std::string::npos);
}

TEST_CASE("primec build reports the diagnostics of a broken script") {
  const std::string out = scratch("script_error.txt");
  CHECK(primecBuild(shellQuote(FixtureDir + "project_type_error"), out) != 0);
  CHECK(readFile(out + ".err").find("project_type_error/make.prime:6") != std::string::npos);
}

TEST_CASE("building a make.prime with exe and optexe steps gives programs that run") {
  const std::string outDir = freshOutDir("run_out");
  const std::string out = scratch("run.txt");
  const int status = primecBuild(
      shellQuote(projectCopy("project_run")) + " --jobs 2 --out-dir " + shellQuote(outDir), out);
  REQUIRE_MESSAGE(status == 0, readFile(out + ".err"));
  CHECK(readFile(out) == "built exe:exit_five\nbuilt exe:exit_three\n");
  CHECK(run(shellQuote(outDir + "/exit_three")) == 3);
  CHECK(run(shellQuote(outDir + "/exit_five")) == 5);
}

TEST_CASE("primec build names make.prime when the directory has none") {
  const std::string out = scratch("missing.json");
  CHECK(primecBuild("--dry-run " + shellQuote(FixtureDir), out) != 0);
  CHECK(readFile(out + ".err").find("no make.prime in") != std::string::npos);
}

TEST_CASE("primec build names make.prime for a script with a type error") {
  const std::string out = scratch("type_error.json");
  CHECK(primecBuild("--dry-run " + shellQuote(FixtureDir + "project_type_error"), out) != 0);
  const std::string diagnostics = readFile(out + ".err");
  CHECK_MESSAGE(diagnostics.find("project_type_error/make.prime:6") != std::string::npos,
                diagnostics);
}

TEST_CASE("primec build rejects an effect outside the ceiling and names make.prime") {
  const std::string out = scratch("effects.json");
  CHECK(primecBuild("--dry-run " + shellQuote(FixtureDir + "project_effects"), out) != 0);
  const std::string diagnostics = readFile(out + ".err");
  CHECK_MESSAGE(diagnostics.find("project_effects/make.prime") != std::string::npos, diagnostics);
  CHECK_MESSAGE(diagnostics.find("io_out") != std::string::npos, diagnostics);
}

TEST_SUITE_END();
