// A script that runs its graph in stages (docs/BuildSystemPlan.md section 3, `Graph.run` of /std/build). make.prime
// builds a first graph with one step that writes `first.txt` (an `--emit=ir` step: no step kind writes PrimeStruct
// source yet), runs it, lists the `*.txt` files in the output directory and builds a second graph with one step for each.
// A `primec build` compiles make.prime with the build stdlib once, about 1.2 s, and keeps the compiled script in the
// project, so the later runs of a case cost a fraction of that. Each case is a CTest entry of its own (CMakeLists.txt).
#include "primec/pipeline/BuildCommand.h"
#include "primec/pipeline/BuildGraphJson.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_stages");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void writeFile(const std::filesystem::path &path, const std::string &text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
}

struct BuildResult {
  int status = 0;
  std::string out;
  std::string err;
};

class StagedProject {
public:
  // `effects` is the effect list of `build`.
  StagedProject(const std::string &name, const std::string &effects) : dir_(primec::testing::testScratchDir(name)) {
    std::filesystem::remove_all(dir_);
    writeFile(dir_ / "src/exit_three.prime", "[return<i32>]\nmain() {\n  return(3i32)\n}\n");
    writeFile(dir_ / "src/exit_five.prime", "[return<i32>]\nmain() {\n  return(5i32)\n}\n");
    writeFile(dir_ / "make.prime",
              "import /std/build/*\nimport /std/glob/*\nimport /std/file/*\nimport /std/path/*\n"
              "import /std/collections/vector/*\nimport /std/text/*\nimport /std/text/view/*\n\n"
              "[return<Graph> effects(" +
                  effects +
                  ")]\nbuild([Build mut] b) {\n"
                  "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n"
                  "  b.compile(\"first.txt\", cc, \"src/exit_three.prime\")\n"
                  "  [Graph] firstStage{b.graph()}\n  firstStage.run()\n"
                  "  [Build mut] next{Build{}}\n"
                  "  [vector<String> mut] found{vector<String>{}}\n  [vector<String> mut] listed{vector<String>{}}\n"
                  "  [Result<FileError>] globbed{glob(\"" +
                  (dir_ / "out").string() +
                  "\", \"*.txt\", found, listed)}\n"
                  "  for([i32 mut] i{0i32}, i < found.count(), i++) {\n"
                  "    [String] path{found[i]}\n"
                  "    [String mut] label{stringFrom(\"from_\")}\n"
                  "    [String] base{stem(path.view())}\n"
                  "    label.appendBytes(base.data, base.count())\n"
                  "    next.compile(label.view(), cc, \"src/exit_five.prime\")\n  }\n"
                  "  return(next.graph())\n}\n");
  }

  std::filesystem::path dir() const { return dir_; }

  BuildResult build(const std::string &options = "--jobs 1") const {
    BuildResult result;
    const std::string out = (dir_ / "build.stdout").string();
    const int status = std::system(("'" + Primec + "' build '" + dir_.string() + "' " + options + " > '" + out +
                                    "' 2> '" + out + ".err'")
                                       .c_str());
    result.status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result.out = readFile(out);
    result.err = readFile(out + ".err");
    return result;
  }

private:
  std::filesystem::path dir_;
};
} // namespace

TEST_CASE("a script that runs its graph builds a second graph from the files the first stage wrote") {
  StagedProject project("build_stages_run", "heap_alloc, file_read, build_run");
  const BuildResult first = project.build();
  REQUIRE_MESSAGE(first.status == 0, first.err);
  CHECK(first.out == "stage 1\nbuilt compile:first.txt\nstage 2\nbuilt compile:from_first\n");
  CHECK(std::filesystem::exists(project.dir() / "out" / "first.txt"));
  CHECK(std::filesystem::exists(project.dir() / "out" / "from_first"));
  // The stages share the cache: a second build runs no step in either stage.
  const BuildResult second = project.build();
  CHECK_MESSAGE(second.status == 0, second.err);
  CHECK(second.out == "stage 1\nup to date compile:first.txt\nstage 2\nup to date compile:from_first\n");
}

TEST_CASE("the dry run of a staged script ends at the first run and says the plan is incomplete") {
  StagedProject project("build_stages_dry", "heap_alloc, file_read, build_run");
  const BuildResult dry = project.build("--dry-run");
  REQUIRE_MESSAGE(dry.status == 0, dry.err);
  CHECK(dry.out.find("\"incomplete\":true") != std::string::npos);
  CHECK(dry.out.find("compile:first.txt") != std::string::npos);
  CHECK(dry.out.find("from_") == std::string::npos);
  CHECK_FALSE(std::filesystem::exists(project.dir() / "out"));
  // The stage after the run is complete; nothing was built, so its listing is empty and so is the graph.
  const BuildResult next = project.build("--dry-run --stage 1");
  REQUIRE_MESSAGE(next.status == 0, next.err);
  CHECK(next.out.find("incomplete") == std::string::npos);
  CHECK(next.out.find("\"nodes\":[]") != std::string::npos);
}

TEST_CASE("a script that calls run without the build_run effect is a diagnostic") {
  StagedProject project("build_stages_effect", "heap_alloc, file_read");
  const BuildResult dry = project.build("--dry-run");
  CHECK(dry.status != 0);
  CHECK(dry.err.find("make.prime") != std::string::npos);
  CHECK(dry.err.find("build_run") != std::string::npos);
}

TEST_CASE("the graph parser reads the incomplete marker and the plan reads --stage") {
  primec::BuildGraphData graph;
  std::string error;
  REQUIRE(primec::parseBuildGraphJson("{\"version\":1,\"nodes\":[],\"problems\":[],\"incomplete\":true}", graph, error));
  CHECK(graph.incomplete);
  REQUIRE(primec::parseBuildGraphJson("{\"version\":1,\"nodes\":[],\"problems\":[]}", graph, error));
  CHECK_FALSE(graph.incomplete);

  const std::filesystem::path dir = primec::testing::testScratchDir("build_stages_plan");
  std::filesystem::remove_all(dir);
  writeFile(dir / "make.prime", "// empty\n");
  primec::BuildCommandPlan plan;
  REQUIRE(primec::planBuildCommand({dir.string(), "--stage", "2"}, plan, error));
  CHECK(plan.stage == 2);
  CHECK(plan.forwarded.empty());
  REQUIRE(primec::planBuildCommand({dir.string()}, plan, error));
  CHECK(plan.stage == 0);
  CHECK_FALSE(primec::planBuildCommand({dir.string(), "--stage", "two"}, plan, error));
  CHECK(error == "--stage expects a number");
}

TEST_SUITE_END();
