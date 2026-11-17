// Function steps (TODO-5919; docs/BuildSystemPlan.md section 4): `b.step(name, inputs, outputs, source, function)` adds a
// step that `primec build` runs in-process on a VM thread. A project in a scratch directory has a `tools/*.prime` file
// with the definition and a make.prime that adds the step; a run prints `built <id>` or `up to date <id>` per step. No
// case depends on timestamps, sleeps or the order in which two steps finish: the cache compares content hashes only and
// the cases that run two steps look at both outputs. Each case that starts `primec` is a CTest entry of its own
// (CMakeLists.txt), because a build compiles make.prime with the build stdlib and a step file with the stdlib.
#include "primec/pipeline/BuildCopyStep.h"
#include "primec/pipeline/BuildGraphJson.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_function_steps");

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

bool contains(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}

struct BuildResult {
  int status = 0;
  std::string out;
  std::string err;
};

// A step definition that writes `file` with `text`, as a tools/ file.
std::string
writerSource(const std::string &name, const std::string &file, const std::string &text) {
  return "import /std/build_step/*\n\n[return<i32> effects(heap_alloc)]\n" + name +
         "() {\n  write_output(\"" + file + "\", \"" + text + "\")\n  return(0i32)\n}\n";
}

class StepProject {
public:
  explicit StepProject(const std::string &name) : dir_(primec::testing::testScratchDir(name)) {
    std::filesystem::remove_all(dir_);
  }

  // The make.prime: `body` are the lines of `build` after the `Build` is made; `effects` is the effect list of `build`.
  void writeScript(const std::string &body, const std::string &effects = "heap_alloc") {
    writeFile(
        dir_ / "make.prime",
        "import /std/build/*\nimport /std/collections/vector/*\nimport "
        "/std/text/*\n\n[return<Graph> effects(" +
            effects +
            ")]\nbuild([Build mut] b) {\n  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n" +
            body + "}\n");
  }

  // The lines that add one function step: `outputs` and `inputs` are file names.
  static std::string stepLines(const std::string &name,
                               const std::string &source,
                               const std::string &function,
                               const std::string &outputs,
                               const std::string &inputs = "") {
    std::string lines = "  [vector<String> mut] " + name +
                        "In{vector<String>{}}\n  [vector<String> mut] " + name +
                        "Out{vector<String>{}}\n";
    if (!inputs.empty()) {
      lines += "  " + name + "In.push(stringFrom(\"" + inputs + "\"))\n";
    }
    lines += "  " + name + "Out.push(stringFrom(\"" + outputs + "\"))\n";
    lines += "  b.step(\"" + name + "\", " + name + "In, " + name + "Out, \"" + source + "\", \"" +
             function + "\")\n";
    return lines;
  }

  std::filesystem::path dir() const {
    return dir_;
  }
  std::filesystem::path outDir() const {
    return dir_ / "out";
  }

  BuildResult build(const std::string &options = "--jobs 2") const {
    BuildResult result;
    const std::string out = (dir_ / "build.stdout").string();
    const int status = std::system(("'" + Primec + "' build '" + dir_.string() + "' " + options +
                                    " > '" + out + "' 2> '" + out + ".err'")
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

TEST_CASE("a generator function step writes a prime file that a compile step then builds") {
  StepProject project("build_fn_generator");
  writeFile(project.dir() / "tools/gen.prime",
            "import /std/build_step/*\n\n[return<i32> effects(heap_alloc)]\ngenerate_program() {\n"
            "  write_output(\"generated.prime\", \"[return<i32>]\\nmain() {\\n  "
            "return(7i32)\\n}\\n\")\n  return(0i32)\n}\n");
  project.writeScript(
      StepProject::stepLines("gen", "tools/gen.prime", "/generate_program", "generated.prime") +
          "  [Graph] firstStage{b.graph()}\n  firstStage.run()\n  [Build mut] next{Build{}}\n"
          "  next.compile(\"generated\", cc, \"out/generated.prime\")\n  return(next.graph())\n",
      "heap_alloc, build_run");
  const BuildResult first = project.build();
  REQUIRE_MESSAGE(first.status == 0, first.err);
  CHECK(first.out == "stage 1\nbuilt step:gen\nstage 2\nbuilt compile:generated\n");
  CHECK(readFile(project.outDir() / "generated.prime") ==
        "[return<i32>]\nmain() {\n  return(7i32)\n}\n");
  CHECK(std::filesystem::exists(project.outDir() / "generated"));
  // Nothing changed, so no step runs again (the function step is cached by its closure, inputs and outputs).
  const BuildResult second = project.build();
  CHECK_MESSAGE(second.status == 0, second.err);
  CHECK(second.out == "stage 1\nup to date step:gen\nstage 2\nup to date compile:generated\n");
}

TEST_CASE("two function steps both produce their outputs") {
  StepProject project("build_fn_two_steps");
  writeFile(project.dir() / "tools/alpha.prime",
            writerSource("write_alpha", "alpha.txt", "alpha one"));
  writeFile(project.dir() / "tools/beta.prime",
            writerSource("write_beta", "nested/beta.txt", "beta one"));
  project.writeScript(
      StepProject::stepLines("alpha", "tools/alpha.prime", "/write_alpha", "alpha.txt") +
      StepProject::stepLines("beta", "tools/beta.prime", "/write_beta", "nested/beta.txt") +
      "  return(b.graph())\n");
  const BuildResult result = project.build();
  REQUIRE_MESSAGE(result.status == 0, result.err);
  CHECK(result.out == "built step:alpha\nbuilt step:beta\n");
  CHECK(readFile(project.outDir() / "alpha.txt") == "alpha one");
  CHECK(readFile(project.outDir() / "nested/beta.txt") == "beta one");
}

TEST_CASE("a function step that writes outside the output directory faults naming the path") {
  StepProject project("build_fn_escape");
  writeFile(project.dir() / "tools/escape.prime",
            writerSource("write_escape", "../escaped.txt", "nope"));
  project.writeScript(
      StepProject::stepLines("escape", "tools/escape.prime", "/write_escape", "inside.txt") +
      "  return(b.graph())\n");
  const BuildResult result = project.build();
  CHECK(result.status != 0);
  CHECK(contains(result.err, "primec build: step escape (step:escape) failed"));
  CHECK(contains(result.err,
                 "build step escape writes outside the output directory: ../escaped.txt"));
  CHECK_FALSE(std::filesystem::exists(project.dir() / "escaped.txt"));
  CHECK_FALSE(std::filesystem::exists(project.outDir() / "inside.txt"));
}

TEST_CASE("a function step that writes an absolute path faults naming the path") {
  StepProject project("build_fn_absolute");
  const std::string target = (project.dir() / "absolute.txt").string();
  writeFile(project.dir() / "tools/absolute.prime", writerSource("write_absolute", target, "nope"));
  project.writeScript(
      StepProject::stepLines("absolute", "tools/absolute.prime", "/write_absolute", "inside.txt") +
      "  return(b.graph())\n");
  const BuildResult result = project.build();
  CHECK(result.status != 0);
  CHECK(contains(result.err, "build step absolute writes outside the output directory: " + target));
  CHECK_FALSE(std::filesystem::exists(target));
}

TEST_CASE("a function step cannot use the file_write effect") {
  StepProject project("build_fn_effect_ceiling");
  writeFile(project.dir() / "tools/raw.prime",
            "import /std/file/*\n\n[return<i32> effects(file_write)]\nwrite_raw() {\n  "
            "return(0i32)\n}\n");
  project.writeScript(StepProject::stepLines("raw", "tools/raw.prime", "/write_raw", "raw.txt") +
                      "  return(b.graph())\n");
  const BuildResult result = project.build();
  CHECK(result.status != 0);
  CHECK(contains(result.err, "step raw (step:raw) failed"));
  CHECK(contains(result.err, "file_write"));
}

TEST_CASE("a function step that does not write its declared output or returns nonzero fails") {
  StepProject project("build_fn_declared_output");
  writeFile(project.dir() / "tools/lazy.prime", writerSource("write_lazy", "other.txt", "x"));
  project.writeScript(
      StepProject::stepLines("lazy", "tools/lazy.prime", "/write_lazy", "declared.txt") +
      "  return(b.graph())\n");
  const BuildResult missing = project.build();
  CHECK(missing.status != 0);
  CHECK(contains(missing.err, "the step did not write its declared output declared.txt"));

  writeFile(project.dir() / "tools/lazy.prime",
            "import /std/build_step/*\n\n[return<i32> effects(heap_alloc)]\nwrite_lazy() {\n  "
            "return(3i32)\n}\n");
  const BuildResult failing = project.build();
  CHECK(failing.status != 0);
  CHECK(contains(failing.err, "/write_lazy returned 3"));
}

TEST_CASE("a function step runs again when a declared input changes") {
  StepProject project("build_fn_cache_input");
  writeFile(project.dir() / "data/in.txt", "one");
  writeFile(project.dir() / "tools/cached.prime",
            writerSource("write_cached", "cached.txt", "first"));
  project.writeScript(
      StepProject::stepLines(
          "cached", "tools/cached.prime", "/write_cached", "cached.txt", "data/in.txt") +
      "  return(b.graph())\n");
  REQUIRE(project.build().out == "built step:cached\n");
  CHECK(project.build().out == "up to date step:cached\n");
  writeFile(project.dir() / "data/in.txt", "two");
  CHECK(project.build().out == "built step:cached\n");
  CHECK(project.build().out == "up to date step:cached\n");
}

TEST_CASE("a function step runs again when its file or an output changes") {
  StepProject project("build_fn_cache_file");
  writeFile(project.dir() / "tools/cached.prime",
            writerSource("write_cached", "cached.txt", "first"));
  project.writeScript(
      StepProject::stepLines("cached", "tools/cached.prime", "/write_cached", "cached.txt") +
      "  return(b.graph())\n");
  REQUIRE(project.build().out == "built step:cached\n");
  writeFile(project.dir() / "tools/cached.prime",
            writerSource("write_cached", "cached.txt", "second"));
  CHECK(project.build().out == "built step:cached\n");
  CHECK(readFile(project.outDir() / "cached.txt") == "second");
  writeFile(project.outDir() / "cached.txt", "tampered");
  CHECK(project.build().out == "built step:cached\n");
  CHECK(readFile(project.outDir() / "cached.txt") == "second");
  CHECK(project.build().out == "up to date step:cached\n");
}

TEST_CASE(
    "the dry run marks a graph with a function step as not self-contained and lists its closure") {
  StepProject project("build_fn_dry_run");
  writeFile(project.dir() / "helpers/text.prime",
            "[public return<i32>]\nhelper_value() {\n  return(4i32)\n}\n");
  writeFile(project.dir() / "tools/gen.prime",
            "import<\"../helpers\">\nimport /std/build_step/*\n\n[return<i32> "
            "effects(heap_alloc)]\ngenerate() {\n"
            "  write_output(\"gen.txt\", \"x\")\n  return(helper_value() - 4i32)\n}\n");
  project.writeScript(
      StepProject::stepLines("gen", "tools/gen.prime", "/generate", "gen.txt", "data/in.txt") +
      "  return(b.graph())\n");
  const BuildResult dry = project.build("--dry-run");
  REQUIRE_MESSAGE(dry.status == 0, dry.err);
  CHECK(contains(dry.out, "\"selfContained\":false"));
  CHECK(contains(
      dry.out,
      "{\"id\":\"step:gen\",\"kind\":\"function\",\"name\":\"gen\",\"source\":\"tools/gen.prime\","
      "\"function\":\"/generate\",\"inputs\":[\"data/in.txt\"],\"outputs\":[\"gen.txt\"],"
      "\"closure\":[\"helpers/text.prime\",\"tools/gen.prime\"],\"runnable\":true}"));
  CHECK_FALSE(std::filesystem::exists(project.outDir()));
}

TEST_CASE("a graph without a function step stays self-contained") {
  StepProject project("build_fn_self_contained");
  writeFile(project.dir() / "src/alpha.prime", "[return<i32>]\nmain() {\n  return(2i32)\n}\n");
  project.writeScript("  b.compile(\"alpha\", cc, \"src/alpha.prime\")\n  return(b.graph())\n");
  const BuildResult dry = project.build("--dry-run");
  REQUIRE_MESSAGE(dry.status == 0, dry.err);
  CHECK_FALSE(contains(dry.out, "selfContained"));
  CHECK_FALSE(contains(dry.out, "closure"));
}

TEST_CASE("a step that declares no outputs or a missing input is a diagnostic") {
  StepProject project("build_fn_declarations");
  writeFile(project.dir() / "tools/none.prime", writerSource("write_none", "none.txt", "x"));
  project.writeScript("  [vector<String> mut] noIn{vector<String>{}}\n  [vector<String> mut] "
                      "noOut{vector<String>{}}\n"
                      "  b.step(\"none\", noIn, noOut, \"tools/none.prime\", \"/write_none\")\n  "
                      "return(b.graph())\n");
  const BuildResult noOutputs = project.build();
  CHECK(noOutputs.status != 0);
  CHECK(contains(noOutputs.err, "step none declares no outputs"));

  project.writeScript(
      StepProject::stepLines(
          "none", "tools/none.prime", "/write_none", "none.txt", "data/missing.txt") +
      "  return(b.graph())\n");
  const BuildResult missing = project.build();
  CHECK(missing.status != 0);
  CHECK(contains(missing.err, "the declared input data/missing.txt is not a file"));
}

TEST_CASE(
    "the path check of a function step accepts the output directory and refuses everything else") {
  const std::filesystem::path root = primec::testing::testScratchDir("build_fn_path_check");
  std::filesystem::remove_all(root);
  const std::string outDir = (root / "out").string();
  std::filesystem::create_directories(root / "out/real");
  std::filesystem::create_directories(root / "elsewhere");
  std::string resolved;
  std::string error;

  CHECK(primec::resolveStepOutputPath("a.txt", outDir, resolved, error));
  CHECK(resolved == outDir + "/a.txt");
  CHECK(primec::resolveStepOutputPath("deep/er/a.txt", outDir, resolved, error));
  CHECK(primec::resolveStepOutputPath("x/../a.txt", outDir, resolved, error));
  CHECK(resolved == outDir + "/a.txt");

  CHECK_FALSE(primec::resolveStepOutputPath("../a.txt", outDir, resolved, error));
  CHECK(error == "writes outside the output directory: ../a.txt");
  CHECK_FALSE(primec::resolveStepOutputPath("x/../../a.txt", outDir, resolved, error));
  CHECK_FALSE(primec::resolveStepOutputPath("/etc/passwd", outDir, resolved, error));
  CHECK(error == "writes outside the output directory: /etc/passwd");
  CHECK_FALSE(primec::resolveStepOutputPath("", outDir, resolved, error));
  CHECK(error == "writes an empty path");
  CHECK_FALSE(primec::resolveStepOutputPath(".", outDir, resolved, error));
  // A folder of the output directory that is a link to somewhere else, and a link as the file itself.
  std::filesystem::create_directory_symlink(root / "elsewhere", root / "out/linked");
  CHECK_FALSE(primec::resolveStepOutputPath("linked/a.txt", outDir, resolved, error));
  CHECK(error == "writes outside the output directory: linked/a.txt");
  writeFile(root / "elsewhere/target.txt", "x");
  std::filesystem::create_symlink(root / "elsewhere/target.txt", root / "out/real/link.txt");
  CHECK_FALSE(primec::resolveStepOutputPath("real/link.txt", outDir, resolved, error));
}

TEST_CASE("the merge of subproject graphs moves a function step's inputs and keeps its outputs") {
  const std::string parent =
      "{\"version\":1,\"nodes\":[],\"problems\":[],\"subprojects\":[\"sub\"]}\n";
  const std::string child = "{\"version\":1,\"nodes\":[{\"id\":\"step:gen\",\"kind\":\"function\","
                            "\"name\":\"gen\",\"source\":\"tools/gen.prime\","
                            "\"function\":\"/generate\",\"inputs\":[\"data/"
                            "in.txt\"],\"outputs\":[\"gen.txt\"],\"runnable\":true}],"
                            "\"problems\":[],\"selfContained\":false}\n";
  std::string merged;
  std::string error;
  REQUIRE(primec::mergeBuildGraphJson(parent, {{"sub", child}}, ".", merged, error));
  CHECK(contains(merged,
                 "{\"id\":\"sub/step:gen\",\"kind\":\"function\",\"name\":\"gen\",\"source\":\"sub/"
                 "tools/gen.prime\","
                 "\"function\":\"/generate\",\"inputs\":[\"sub/data/"
                 "in.txt\"],\"outputs\":[\"gen.txt\"],\"runnable\":true}"));
  CHECK(contains(merged, "\"problems\":[],\"selfContained\":false"));
  primec::BuildGraphData graph;
  REQUIRE(primec::parseBuildGraphJson(merged, graph, error));
  CHECK_FALSE(graph.selfContained);
  REQUIRE(graph.steps.size() == 1);
  CHECK(graph.steps[0].kind == "function");
  CHECK(graph.steps[0].function == "/generate");
  CHECK(graph.steps[0].inputs == std::vector<std::string>{"sub/data/in.txt"});
  CHECK(graph.steps[0].outputs == std::vector<std::string>{"gen.txt"});
}

TEST_SUITE_END();
