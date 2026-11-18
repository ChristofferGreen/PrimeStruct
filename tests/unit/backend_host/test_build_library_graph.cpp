// The library and link nodes of a build graph run (docs/ModulesAndLibrariesPlan.md section 11, docs/BuildSystemPlan.md
// section 4): `b.library` builds a `.psa` with `--emit=lib`, `b.link` and `b.linkLibrary` make the step that links a
// library wait for it and compile against the whole closure. The end-to-end cases run `primec build` on the projects
// tests/fixtures/build/project_libraries, project_library_diamond and project_library_fail; the unit cases run
// `resolveBuildLibraryLinks` on hand-made graphs. Nothing depends on timestamps or sleeps.
#include "primec/pipeline/BuildGraphJson.h"
#include "primec/pipeline/BuildLibraryLinks.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_library_graph");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;
const std::string FixtureDir = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/build/";

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("build_library_graph");
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

// A copy of a fixture project in the scratch directory (a run writes its cache into the project).
std::string projectCopy(const std::string &name) {
  const std::string dir = scratch("project_" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::copy(FixtureDir + name, dir, std::filesystem::copy_options::recursive);
  return dir;
}

std::string freshOutDir(const std::string &name) {
  const std::string dir = scratch(name);
  std::filesystem::remove_all(dir);
  return dir;
}

// `primec build <project> --out-dir <outDir>`: the standard output goes to `out`, the diagnostics to `out + ".err"`.
int primecBuild(const std::string &project, const std::string &outDir, const std::string &out) {
  return run(shellQuote(Primec) + " build " + shellQuote(project) + " --jobs 2 --out-dir " +
             shellQuote(outDir) + " > " + shellQuote(out) + " 2> " + shellQuote(out + ".err"));
}

size_t count(const std::string &text, const std::string &needle) {
  size_t found = 0;
  for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) {
    ++found;
  }
  return found;
}

primec::BuildStep step(const std::string &id, const std::string &kind) {
  primec::BuildStep result;
  result.id = id;
  result.kind = kind;
  result.output = kind == "library" ? id.substr(4) + ".psa" : id.substr(4);
  return result;
}

const primec::BuildStep &find(const primec::BuildGraphData &graph, const std::string &id) {
  for (const primec::BuildStep &candidate : graph.steps) {
    if (candidate.id == id) {
      return candidate;
    }
  }
  FAIL("no step " << id);
  return graph.steps.front();
}
} // namespace

TEST_CASE("library build: a library, one that links it and executables that link them run") {
  const std::string outDir = freshOutDir("libraries_out");
  const std::string out = scratch("libraries.txt");
  const int status = primecBuild(projectCopy("project_libraries"), outDir, out);
  REQUIRE_MESSAGE(status == 0, readFile(out + ".err"));
  const std::string text = readFile(out);
  CHECK(text.find("built lib:base\n") != std::string::npos);
  CHECK(text.find("built lib:mid\n") != std::string::npos);
  CHECK(std::filesystem::exists(outDir + "/base.psa"));
  CHECK(std::filesystem::exists(outDir + "/mid.psa"));
  // `app` links only `mid`; the runner passes `base` too.
  CHECK(run(shellQuote(outDir + "/app")) == 8);
  CHECK(run(shellQuote(outDir + "/both")) == 15);
}

TEST_CASE("library build: a second run finds every step up to date") {
  const std::string project = projectCopy("project_libraries");
  const std::string outDir = freshOutDir("libraries_again_out");
  const std::string out = scratch("libraries_again_first.txt");
  REQUIRE_MESSAGE(primecBuild(project, outDir, out) == 0, readFile(out + ".err"));
  const std::string again = scratch("libraries_again.txt");
  REQUIRE_MESSAGE(primecBuild(project, outDir, again) == 0, readFile(again + ".err"));
  const std::string text = readFile(again);
  CHECK(text.find("up to date lib:base\n") != std::string::npos);
  CHECK(text.find("up to date lib:mid\n") != std::string::npos);
  CHECK(text.find("up to date exe:app\n") != std::string::npos);
  CHECK(text.find("built ") == std::string::npos);
}

TEST_CASE("library build: a diamond builds the shared library once") {
  const std::string outDir = freshOutDir("diamond_out");
  const std::string out = scratch("diamond.txt");
  const int status = primecBuild(projectCopy("project_library_diamond"), outDir, out);
  REQUIRE_MESSAGE(status == 0, readFile(out + ".err"));
  const std::string text = readFile(out);
  CHECK(count(text, "lib:shared\n") == 1);
  CHECK(text.find("built lib:shared\n") != std::string::npos);
  CHECK(run(shellQuote(outDir + "/app")) == 3);
}

TEST_CASE("library build: a failed library stops its dependents and names itself") {
  const std::string outDir = freshOutDir("fail_library_out");
  const std::string out = scratch("fail_library.txt");
  const int status = primecBuild(projectCopy("project_library_fail"), outDir, out);
  CHECK(status != 0);
  const std::string diagnostics = readFile(out + ".err");
  CHECK(diagnostics.find("step broken (lib:broken) failed") != std::string::npos);
  CHECK(diagnostics.find("step top (lib:top) skipped after a failure") != std::string::npos);
  CHECK(diagnostics.find("step app (exe:app) skipped after a failure") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(outDir + "/top.psa"));
  CHECK_FALSE(std::filesystem::exists(outDir + "/app"));
}

TEST_CASE("library links: an executable gets the closure of its libraries and waits for them") {
  primec::BuildGraphData graph;
  graph.steps = {step("lib:a", "library"),
                 step("lib:b", "library"),
                 step("lib:c", "library"),
                 step("exe:app", "executable")};
  graph.links = {{"lib:a", "lib:b"}, {"lib:b", "lib:c"}, {"exe:app", "lib:a"}};
  CHECK(primec::resolveBuildLibraryLinks(graph, "/out").empty());
  const primec::BuildStep &app = find(graph, "exe:app");
  CHECK(app.libraries == std::vector<std::string>{"/out/a.psa", "/out/b.psa", "/out/c.psa"});
  CHECK(app.after == std::vector<std::string>{"lib:a", "lib:b", "lib:c"});
  CHECK(find(graph, "lib:a").libraries == std::vector<std::string>{"/out/b.psa", "/out/c.psa"});
  CHECK(find(graph, "lib:c").libraries.empty());
  CHECK(find(graph, "lib:c").after.empty());
}

TEST_CASE("library links: a library reached twice is listed once") {
  primec::BuildGraphData graph;
  graph.steps = {step("lib:left", "library"),
                 step("lib:right", "library"),
                 step("lib:shared", "library"),
                 step("exe:app", "executable")};
  graph.links = {{"lib:left", "lib:shared"},
                 {"lib:right", "lib:shared"},
                 {"exe:app", "lib:left"},
                 {"exe:app", "lib:right"},
                 {"exe:app", "lib:shared"}};
  CHECK(primec::resolveBuildLibraryLinks(graph, "/out").empty());
  CHECK(find(graph, "exe:app").libraries ==
        std::vector<std::string>{"/out/left.psa", "/out/right.psa", "/out/shared.psa"});
}

TEST_CASE("library links: a cycle, a missing step and a link to a non-library are problems") {
  primec::BuildGraphData cycle;
  cycle.steps = {step("lib:a", "library"), step("lib:b", "library")};
  cycle.links = {{"lib:a", "lib:b"}, {"lib:b", "lib:a"}};
  const std::vector<std::string> cyclic = primec::resolveBuildLibraryLinks(cycle, "/out");
  REQUIRE(cyclic.size() == 2);
  CHECK(cyclic[0] == "library lib:a is in a cycle of libraries");
  CHECK(cyclic[1] == "library lib:b is in a cycle of libraries");
  CHECK(cycle.steps[0].libraries.empty());

  primec::BuildGraphData missing;
  missing.steps = {step("exe:app", "executable")};
  missing.links = {{"exe:app", "lib:none"}};
  const std::vector<std::string> absent = primec::resolveBuildLibraryLinks(missing, "/out");
  REQUIRE(absent.size() == 1);
  CHECK(absent[0].find("names a step that is not in the graph") != std::string::npos);

  primec::BuildGraphData notLibrary;
  notLibrary.steps = {step("exe:app", "executable"), step("exe:other", "executable")};
  notLibrary.links = {{"exe:app", "exe:other"}};
  const std::vector<std::string> wrong = primec::resolveBuildLibraryLinks(notLibrary, "/out");
  REQUIRE(wrong.size() == 1);
  CHECK(wrong[0].find("exe:other is not a library") != std::string::npos);
}

TEST_CASE("library links: the parser reads library and link nodes") {
  const std::string text = "{\"version\":1,\"nodes\":[{\"id\":\"lib:core\",\"kind\":\"library\","
                           "\"name\":\"core\",\"source\":\"c.prime\","
                           "\"output\":\"core.psa\",\"compiler\":{\"emit\":\"lib\",\"optimize\":0,"
                           "\"entry\":\"/core/start\"},"
                           "\"runnable\":true},{\"id\":\"link:app>core\",\"kind\":\"link\","
                           "\"from\":\"exe:app\",\"to\":\"lib:core\","
                           "\"runnable\":true}],\"problems\":[]}";
  primec::BuildGraphData graph;
  std::string error;
  REQUIRE_MESSAGE(primec::parseBuildGraphJson(text, graph, error), error);
  REQUIRE(graph.steps.size() == 1);
  CHECK(graph.steps[0].kind == "library");
  CHECK(graph.steps[0].emitKind == "lib");
  CHECK(graph.unsupportedNodes.empty());
  REQUIRE(graph.links.size() == 1);
  CHECK(graph.links[0].from == "exe:app");
  CHECK(graph.links[0].to == "lib:core");
}
