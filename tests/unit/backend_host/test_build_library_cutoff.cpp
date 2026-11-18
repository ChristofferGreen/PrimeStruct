// A compile step that links libraries is rebuilt when a library symbol it used changed, not when a library did
// (docs/ModulesAndLibrariesPlan.md section 11, src/pipeline/BuildLibraryCutoff.h). The cases build a small library with
// `primec --emit=lib`, run a one-step graph through `runBuildGraph` and read the `built app` / `up to date app` line.
// No case depends on timestamps or sleeps: the cache compares content hashes.
#include "primec/pipeline/BuildCache.h"
#include "primec/pipeline/BuildGraphJson.h"
#include "primec/pipeline/BuildLibraryCutoff.h"
#include "primec/pipeline/BuildRunner.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_library_cutoff");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;

std::string readFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void writeFile(const std::filesystem::path &path, const std::string &text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
}

int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

void replaceFirst(std::string &text, const std::string &from, const std::string &to) {
  const size_t at = text.find(from);
  REQUIRE(at != std::string::npos);
  text.replace(at, from.size(), to);
}

// The program calls `add` and `bumped`, not `sub`.
const std::string Program = "import</ui>\nimport /ui/*\n[return<i32>]\nmain() {\n  "
                            "return(add(1i32, 2i32) + bumped(3i32))\n}\n";

// A project with a library `ui` (ui.prime -> ui.psa) and one compile step `app` that links it.
class LibraryProject {
public:
  LibraryProject() : dir_(primec::testing::testScratchDir("build_library_cutoff")) {
    source_ = "[namespace] ui {\n"
              "  [return<i32>] hidden([i32] a) { return(a + 1i32) }\n"
              "  [public return<i32>] add([i32] a, [i32] b) { return(a + b) }\n"
              "  [public return<i32>] sub([i32] a, [i32] b) { return(a - b) }\n"
              "  [public return<i32>] bumped([i32] a) { return(hidden(a)) }\n"
              "}\n";
    writeFile(dir_ / "app.prime", Program);
    buildLibrary();
  }

  std::filesystem::path dir() const {
    return dir_;
  }
  std::filesystem::path cacheFile() const {
    return primec::buildCacheFilePath((dir_ / ".ps" / "cache").string(), "app");
  }
  std::string output() const {
    return readFile(dir_ / "out" / "app.cpp");
  }

  void edit(const std::string &from, const std::string &to) {
    replaceFirst(source_, from, to);
    buildLibrary();
  }

  // Adds a function at the end of the namespace.
  void addFunction(const std::string &text) {
    source_.insert(source_.rfind("}\n"), text);
    buildLibrary();
  }

  void buildLibrary() {
    writeFile(dir_ / "ui.prime", source_);
    REQUIRE(run("cd '" + dir_.string() + "' && '" + Primec +
                "' --emit=lib ui.prime -o ui.psa > /dev/null 2>&1") == 0);
  }

  primec::BuildGraphData graph() const {
    primec::BuildStep step;
    step.id = "app";
    step.kind = "compile";
    step.name = "app";
    step.source = "app.prime";
    step.output = "app.cpp";
    step.emitKind = "optcpp";
    step.libraries = {"ui.psa"};
    primec::BuildGraphData graph;
    graph.steps.push_back(step);
    return graph;
  }

  // Runs the graph; the text printed to standard output (`built app` or `up to date app`).
  std::string build(int *status = nullptr) const {
    primec::BuildRunSettings settings;
    settings.primecPath = Primec;
    settings.projectDir = dir_.string();
    settings.outDir = (dir_ / "out").string();
    settings.cacheDir = (dir_ / ".ps" / "cache").string();
    settings.jobs = 1;
    std::ostringstream out;
    std::ostringstream err;
    const int code = primec::runBuildGraph(graph(), settings, out, err);
    if (status != nullptr) {
      *status = code;
    }
    return code == 0 ? out.str() : out.str() + err.str();
  }

  // What a fresh compile of the program against the library gives.
  std::string freshOutput() const {
    REQUIRE(run("cd '" + dir_.string() + "' && '" + Primec +
                "' --emit=optcpp -O0 --library ui.psa --out-dir fresh -o app.cpp app.prime > "
                "/dev/null 2>&1") == 0);
    return readFile(dir_ / "fresh" / "app.cpp");
  }

private:
  std::filesystem::path dir_;
  std::string source_;
};

} // namespace

TEST_CASE("library cutoff: second run of a library step is up to date") {
  LibraryProject project;
  CHECK(project.build() == "built app\n");
  CHECK(project.build() == "up to date app\n");
}

TEST_CASE("library cutoff: body of a function the program did not call does not rebuild") {
  LibraryProject project;
  REQUIRE(project.build() == "built app\n");
  const std::string before = readFile(project.dir() / "ui.psa");
  project.edit("return(a - b)", "return(a - b - 1i32)");
  REQUIRE(readFile(project.dir() / "ui.psa") != before);
  CHECK(project.build() == "up to date app\n");
}

TEST_CASE("library cutoff: signature of a function the program did not call does not rebuild") {
  LibraryProject project;
  REQUIRE(project.build() == "built app\n");
  project.edit("sub([i32] a, [i32] b)", "sub([i32] a, [i32] b, [i32] c{0i32})");
  CHECK(project.build() == "up to date app\n");
}

TEST_CASE("library cutoff: a new function in the library does not rebuild") {
  LibraryProject project;
  REQUIRE(project.build() == "built app\n");
  project.addFunction("  [public return<i32>] mul([i32] a, [i32] b) { return(a * b) }\n");
  CHECK(project.build() == "up to date app\n");
}

TEST_CASE("library cutoff: signature of a function the program called rebuilds") {
  LibraryProject project;
  REQUIRE(project.build() == "built app\n");
  project.edit("add([i32] a, [i32] b)", "add([i32] a, [i32] b, [i32] c{0i32})");
  CHECK(project.build() == "built app\n");
  CHECK(project.build() == "up to date app\n");
}

TEST_CASE("library cutoff: body of a function the program called rebuilds and links the new body") {
  LibraryProject project;
  REQUIRE(project.build() == "built app\n");
  const std::string before = project.output();
  project.edit("return(a + b)", "return(a + b + 10i32)");
  CHECK(project.build() == "built app\n");
  CHECK(project.output() != before);
  CHECK(project.output() == project.freshOutput());
  CHECK(project.build() == "up to date app\n");
}

TEST_CASE("library cutoff: body of a private function a called function reaches rebuilds") {
  LibraryProject project;
  REQUIRE(project.build() == "built app\n");
  project.edit("return(a + 1i32)", "return(a + 2i32)");
  CHECK(project.build() == "built app\n");
  CHECK(project.output() == project.freshOutput());
}

TEST_CASE("library cutoff: the entry lists the used symbols and not the archive") {
  LibraryProject project;
  REQUIRE(project.build() == "built app\n");
  const std::string entry = readFile(project.cacheFile());
  CHECK(entry.find(" /ui/add\n") != std::string::npos);
  CHECK(entry.find(" /ui/bumped\n") != std::string::npos);
  CHECK(entry.find("/ui/sub") == std::string::npos);
  CHECK(entry.find("ui.psa") == std::string::npos);
  primec::BuildCacheEntry parsed;
  REQUIRE(primec::readBuildCacheEntry(project.cacheFile().string(), parsed));
  REQUIRE(parsed.uses.size() >= 2);
  for (const primec::BuildLibraryUse &use : parsed.uses) {
    CHECK(use.library == "ui");
    CHECK(use.closureHash.size() == 64);
  }
}

TEST_CASE("library cutoff: a library that cannot be read runs the step") {
  LibraryProject project;
  REQUIRE(project.build() == "built app\n");
  writeFile(project.dir() / "ui.psa", "not an archive");
  int status = 0;
  const std::string out = project.build(&status);
  CHECK(status == 1);
  CHECK(out.find("up to date") == std::string::npos);
}

TEST_CASE("library cutoff: used symbols of two tables compare by name without overload suffixes") {
  primec::BuildLibraryUse use;
  use.path = "/ui/f__ov1";
  use.library = "ui";
  use.signatureHash = "s1";
  use.contentHash = "c1";
  use.closureHash = "k1";
  primec::ModuleInterfaceSymbol same;
  same.path = "/ui/f__ov2";
  same.kind = "function";
  same.signatureHash = "s1";
  same.contentHash = "c1";
  same.closureHash = "k1";
  primec::BuildLibrarySymbols current;
  current["ui"] = {same};
  CHECK(primec::buildLibraryUsesUnchanged({use}, current));
  primec::ModuleInterfaceSymbol sibling = same;
  sibling.signatureHash = "s2";
  current["ui"].push_back(sibling);
  CHECK_FALSE(primec::buildLibraryUsesUnchanged({use}, current));
  current["ui"] = {same};
  current["ui"][0].closureHash = "k2";
  CHECK_FALSE(primec::buildLibraryUsesUnchanged({use}, current));
  current["ui"][0] = same;
  current["ui"].push_back({"/ui/other", "function", "s9", "c9", "k9"});
  CHECK(primec::buildLibraryUsesUnchanged({use}, current));
  current.erase("ui");
  CHECK_FALSE(primec::buildLibraryUsesUnchanged({use}, current));
}

TEST_CASE("library cutoff: the cache entry round trips its uses") {
  const std::filesystem::path dir = primec::testing::testScratchDir("build_library_cutoff_entry");
  primec::BuildCacheEntry entry;
  entry.key = "k";
  entry.outputHash = "o";
  entry.uses.push_back({"/ui/add", "ui", "sig", "content", "closure"});
  REQUIRE(primec::writeBuildCacheEntry((dir / "e.cache").string(), entry));
  primec::BuildCacheEntry read;
  REQUIRE(primec::readBuildCacheEntry((dir / "e.cache").string(), read));
  REQUIRE(read.uses.size() == 1);
  CHECK(read.uses[0].path == "/ui/add");
  CHECK(read.uses[0].library == "ui");
  CHECK(read.uses[0].signatureHash == "sig");
  CHECK(read.uses[0].contentHash == "content");
  CHECK(read.uses[0].closureHash == "closure");
}

TEST_CASE("library cutoff: libraries of a node become one --library each") {
  primec::BuildGraphData graph;
  std::string error;
  REQUIRE(primec::parseBuildGraphJson(
      "{\"nodes\":[{\"id\":\"app\",\"kind\":\"compile\",\"name\":\"app\",\"source\":\"app.prime\","
      "\"output\":\"app\",\"compiler\":{\"emit\":\"optcpp\",\"libraries\":[\"a.psa\",\"b.psa\"]}}],"
      "\"problems\":[]}",
      graph,
      error));
  REQUIRE(graph.steps.size() == 1);
  CHECK(graph.steps[0].libraries == std::vector<std::string>{"a.psa", "b.psa"});
  primec::BuildRunSettings settings;
  settings.primecPath = "primec";
  const std::vector<std::string> arguments = primec::buildStepArguments(graph.steps[0], settings);
  const std::vector<std::string> expected{"--library", "a.psa", "--library", "b.psa"};
  bool found = false;
  for (size_t i = 0; i + expected.size() <= arguments.size(); ++i) {
    found = found ||
            std::equal(expected.begin(), expected.end(), arguments.begin() + static_cast<long>(i));
  }
  CHECK(found);
}

TEST_SUITE_END();
