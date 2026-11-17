// `b.option` of /std/build and `primec build --option` / `--list-options` (docs/BuildSystemPlan.md section 5). A `primec
// build` compiles each script with the build stdlib, about 1.2 s cold, and keeps the compiled script in the project, so
// each case that runs scripts has one project with as few scripts as it can, and each case is a CTest entry of its own
// (CMakeLists.txt). The options reach the script as program arguments, so they are not part of the script cache key.
#include "primec/pipeline/BuildCommand.h"
#include "primec/pipeline/BuildGraphJson.h"
#include "primec/pipeline/BuildOptions.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_options");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;
const std::string FixtureDir = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/build/";

std::string readFile(const std::string &path) {
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

const std::string Program = "[return<i32>]\nmain() {\n  return(3i32)\n}\n";

class Project {
public:
  explicit Project(const std::string &name) : dir_(primec::testing::testScratchDir(name)) {
    std::filesystem::remove_all(dir_);
    writeFile(dir_ / "src/p.prime", Program);
  }

  std::filesystem::path dir() const {
    return dir_;
  }

  void write(const std::string &relative, const std::string &text) const {
    writeFile(dir_ / relative, text);
  }

  BuildResult build(const std::string &options) const {
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

  // The compiled scripts of the project's cache and their modification times.
  // The scripts of subprojects keep theirs in their own folder.
  std::map<std::string, std::filesystem::file_time_type> compiledScripts() const {
    std::map<std::string, std::filesystem::file_time_type> found;
    std::error_code ec;
    const std::filesystem::path folders[] = {dir_, dir_ / "sub"};
    for (const std::filesystem::path &folder : folders) {
      for (const auto &entry : std::filesystem::directory_iterator(folder / ".ps" / "cache", ec)) {
        if (entry.path().extension() == ".ir") {
          found[entry.path().string()] = std::filesystem::last_write_time(entry.path());
        }
      }
    }
    return found;
  }

private:
  std::filesystem::path dir_;
};

const std::string ScriptHead = "import /std/build/*\n\n";
} // namespace

TEST_CASE("options reach the root script and the list matches the golden file") {
  Project project("build_options_reach");
  project.write(
      "make.prime",
      ScriptHead +
          "[return<Graph> effects(heap_alloc)]\nbuild([Build mut] b) {\n"
          "  [bool] ui{b.option(\"native_ui\", true, \"Build the native UI\")}\n"
          "  [i32] level{b.option(\"level\", 2i32, \"Optimization level\")}\n"
          "  [String] name{b.option(\"name\", \"app\")}\n"
          "  [String] flavor{b.option(\"flavor\", \"plain\", \"Flavor of the subproject\")}\n"
          "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n  cc.optimize(level)\n"
          "  if(ui) {\n    b.compile(\"ui\", cc, \"src/p.prime\")\n  }\n"
          "  b.compile(name.view(), cc, \"src/p.prime\")\n  return(b.graph())\n}\n");
  // A bool option gives different dry-run exports for its two values.
  const BuildResult defaults = project.build("--dry-run");
  REQUIRE_MESSAGE(defaults.status == 0, defaults.err);
  CHECK(contains(defaults.out, "\"id\":\"compile:ui\""));
  CHECK(contains(defaults.out, "\"id\":\"compile:app\""));
  CHECK(contains(defaults.out, "\"optimize\":2"));
  CHECK(contains(defaults.out, "\"name\":\"native_ui\",\"type\":\"bool\",\"default\":\"true\""));
  const auto compiledBefore = project.compiledScripts();
  CHECK(compiledBefore.size() == 1);

  const BuildResult changed =
      project.build("--dry-run --option name=x=y --option native_ui=false --option level=-3");
  REQUIRE_MESSAGE(changed.status == 0, changed.err);
  CHECK_FALSE(contains(changed.out, "compile:ui"));
  CHECK(contains(changed.out, "\"id\":\"compile:x=y\""));
  CHECK(contains(changed.out, "\"optimize\":-3"));
  CHECK(contains(changed.out,
                 "\"name\":\"native_ui\",\"type\":\"bool\",\"default\":\"true\",\"description\":"
                 "\"Build the native UI\",\"value\":\"false\"}"));
  CHECK(contains(changed.out,
                 "\"name\":\"name\",\"type\":\"String\",\"default\":\"app\",\"description\":\"\","
                 "\"value\":\"x=y\"}"));
  // The compiled script is the same file: options are not part of the script cache key.
  CHECK(project.compiledScripts() == compiledBefore);

  const BuildResult list = project.build("--list-options");
  REQUIRE_MESSAGE(list.status == 0, list.err);
  CHECK(list.out == readFile(FixtureDir + "options/list_options.golden.txt"));
}

TEST_CASE("an option reaches the scripts of a subproject and is listed once") {
  Project project("build_options_sub");
  project.write(
      "make.prime",
      ScriptHead +
          "[return<Graph> effects(heap_alloc)]\nbuild([Build mut] b) {\n"
          "  b.subproject(\"sub\")\n"
          "  [bool] ui{b.option(\"native_ui\", true, \"Build the native UI\")}\n"
          "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n"
          "  if(ui) {\n    b.compile(\"ui\", cc, \"src/p.prime\")\n  }\n  return(b.graph())\n}\n");
  project.write(
      "sub/make.prime",
      ScriptHead +
          "[return<Graph> effects(heap_alloc)]\nbuild([Build mut] b) {\n"
          "  [bool] ui{b.option(\"native_ui\", true, \"Build the native UI\")}\n"
          "  [String] flavor{b.option(\"flavor\", \"plain\", \"Flavor of the subproject\")}\n"
          "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n"
          "  if(!ui) {\n    b.compile(flavor.view(), cc, \"../src/p.prime\")\n  }\n  "
          "return(b.graph())\n}\n");
  // An option of the project reaches the subproject too, and so does one that only the subproject declares.
  const BuildResult changed =
      project.build("--dry-run --option flavor=fancy --option native_ui=false");
  REQUIRE_MESSAGE(changed.status == 0, changed.err);
  CHECK_FALSE(contains(changed.out, "compile:ui"));
  CHECK(contains(changed.out, "\"id\":\"sub/compile:fancy\""));
  CHECK(contains(changed.out,
                 "\"name\":\"flavor\",\"type\":\"String\",\"default\":\"plain\",\"description\":"
                 "\"Flavor of the subproject\",\"value\":\"fancy\"}"));
  // The option both scripts declare is one entry with the value of this run.
  const size_t first = changed.out.find("\"name\":\"native_ui\"");
  REQUIRE(first != std::string::npos);
  CHECK(changed.out.find("\"name\":\"native_ui\"", first + 1) == std::string::npos);
  CHECK(contains(changed.out, "\"value\":\"false\"}"));
  CHECK(project.compiledScripts().size() == 2);
}

TEST_CASE("an option that no script declares and a value of the wrong type are diagnostics") {
  Project project("build_options_errors");
  project.write(
      "make.prime",
      ScriptHead +
          "[return<Graph> effects(heap_alloc)]\nbuild([Build mut] b) {\n"
          "  [bool] ui{b.option(\"native_ui\", true)}\n  [i32] level{b.option(\"level\", 2i32)}\n"
          "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n"
          "  if(ui) {\n    b.compile(\"ui\", cc, \"src/p.prime\")\n  }\n  return(b.graph())\n}\n");

  const BuildResult unknown = project.build("--dry-run --option nope=1");
  CHECK(unknown.status == 1);
  CHECK(unknown.err == "primec build: unknown option nope (declared: level, native_ui)\n");
  CHECK(unknown.out.empty());

  const BuildResult badBool = project.build("--dry-run --option native_ui=yes");
  CHECK(badBool.status == 1);
  CHECK(badBool.err ==
        "primec build: option native_ui (bool) expects true or false, got \"yes\"\n");

  const BuildResult badInt = project.build("--dry-run --option level=2.5 --option nope=1");
  CHECK(badInt.status == 1);
  CHECK(badInt.err == "primec build: option level (i32) expects a 32-bit integer, got \"2.5\"\n"
                      "primec build: unknown option nope (declared: level, native_ui)\n");
}

TEST_CASE("an i32 option takes the limits of its type") {
  Project project("build_options_limits");
  project.write("make.prime",
                ScriptHead +
                    "[return<Graph> effects(heap_alloc)]\nbuild([Build mut] b) {\n"
                    "  [i32] level{b.option(\"level\", 2i32)}\n"
                    "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n  cc.optimize(level)\n"
                    "  b.compile(\"first.txt\", cc, \"src/p.prime\")\n  return(b.graph())\n}\n");
  const BuildResult tooBig = project.build("--dry-run --option level=2147483648");
  CHECK(tooBig.status == 1);
  CHECK(contains(tooBig.err, "expects a 32-bit integer"));
  const BuildResult empty = project.build("--dry-run --option level=");
  CHECK(contains(empty.err, "expects a 32-bit integer, got \"\""));
  const BuildResult limits = project.build("--dry-run --option level=-2147483648");
  CHECK_MESSAGE(limits.status == 0, limits.err);
  CHECK(contains(limits.out, "\"optimize\":-2147483648"));
  const BuildResult top = project.build("--dry-run --option level=2147483647");
  CHECK(contains(top.out, "\"optimize\":2147483647"));
  // A failed build runs no step.
  const BuildResult none = project.build("--option level=x");
  CHECK(none.status == 1);
  CHECK(none.out.empty());
}

TEST_CASE("an option changes the steps that run and the cache of the steps follows it") {
  Project project("build_options_cache");
  project.write("make.prime",
                ScriptHead +
                    "[return<Graph> effects(heap_alloc)]\nbuild([Build mut] b) {\n"
                    "  [i32] level{b.option(\"level\", 0i32)}\n"
                    "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n  cc.optimize(level)\n"
                    "  b.compile(\"first.txt\", cc, \"src/p.prime\")\n  return(b.graph())\n}\n");
  const BuildResult first = project.build("--jobs 1");
  REQUIRE_MESSAGE(first.status == 0, first.err);
  CHECK(first.out == "built compile:first.txt\n");
  const auto compiled = project.compiledScripts();
  CHECK(project.build("--jobs 1").out == "up to date compile:first.txt\n");
  // Another value changes the step (its settings are part of the key of the step), the same value again does not.
  CHECK(project.build("--jobs 1 --option level=1").out == "built compile:first.txt\n");
  CHECK(project.build("--jobs 1 --option level=1").out == "up to date compile:first.txt\n");
  CHECK(project.build("--jobs 1 --option level=0").out == "built compile:first.txt\n");
  CHECK(project.compiledScripts() == compiled);
}

TEST_CASE("options reach every stage of a staged script") {
  Project project("build_options_stages");
  project.write(
      "make.prime",
      ScriptHead +
          "[return<Graph> effects(heap_alloc, build_run)]\nbuild([Build mut] b) {\n"
          "  [String] first{b.option(\"first\", \"one\")}\n"
          "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n"
          "  b.compile(first.view(), cc, \"src/p.prime\")\n"
          "  [Graph] firstStage{b.graph()}\n  firstStage.run()\n"
          "  [Build mut] next{Build{}}\n"
          "  [String] second{next.option(\"second\", \"two\")}\n"
          "  next.compile(second.view(), cc, \"src/p.prime\")\n  return(next.graph())\n}\n");
  // An option of the first stage is seen again in the second, and one that only the second stage declares is known.
  const BuildResult run = project.build("--jobs 1 --option first=alpha --option second=beta");
  REQUIRE_MESSAGE(run.status == 0, run.err);
  CHECK(run.out == "stage 1\nbuilt compile:alpha\nstage 2\nbuilt compile:beta\n");
  // A name that no stage declares is reported at the end, after the stages ran.
  const BuildResult unknown = project.build("--jobs 1 --option third=3");
  CHECK(unknown.status == 1);
  CHECK(unknown.err == "primec build: unknown option third (declared: first, second)\n");
  // Staged listings show the options declared up to the first run.
  const BuildResult list = project.build("--list-options");
  CHECK(list.out == "first: String, default \"one\"\n");
}

TEST_CASE("the plan reads --option and --list-options") {
  const std::filesystem::path dir = primec::testing::testScratchDir("build_options_plan");
  std::filesystem::remove_all(dir);
  writeFile(dir / "make.prime", "// empty\n");
  primec::BuildCommandPlan plan;
  std::string error;
  REQUIRE(primec::planBuildCommand(
      {dir.string(), "--option", "b=2", "--option=a=1=2", "--list-options", "-O2"}, plan, error));
  REQUIRE(plan.options.size() == 2);
  CHECK(plan.options[0].name == "a");
  CHECK(plan.options[0].value == "1=2");
  CHECK(plan.options[1].name == "b");
  CHECK(plan.listOptions);
  CHECK(plan.forwarded == std::vector<std::string>{"-O2"});
  CHECK(primec::buildOptionProgramArguments(plan.options) ==
        std::vector<std::string>{"a=1=2", "b=2"});
  // The options are not part of the compile of the script, so they are not in its arguments (nor in the cache key).
  for (const std::string &argument : plan.arguments) {
    CHECK(argument.find("a=1") == std::string::npos);
  }

  CHECK_FALSE(primec::planBuildCommand({dir.string(), "--option", "flag"}, plan, error));
  CHECK(error == "--option expects name=value, got \"flag\"");
  CHECK_FALSE(primec::planBuildCommand({dir.string(), "--option", "=1"}, plan, error));
  CHECK(error.find("is not a name") != std::string::npos);
  CHECK_FALSE(primec::planBuildCommand({dir.string(), "--option", "1x=1"}, plan, error));
  CHECK_FALSE(primec::planBuildCommand({dir.string(), "--option", "a b=1"}, plan, error));
  CHECK_FALSE(primec::planBuildCommand({dir.string(), "--option"}, plan, error));
  CHECK(error == "--option requires a value");
  CHECK_FALSE(
      primec::planBuildCommand({dir.string(), "--option", "a=1", "--option", "a=2"}, plan, error));
  CHECK(error == "--option a was given twice");
  REQUIRE(primec::planBuildCommand(
      {dir.string(), "--option", "a=", "--option", "b.c-d_e=x"}, plan, error));
  CHECK(plan.options[0].value.empty());
}

TEST_CASE("the merge keeps one entry per option and reports a clash") {
  const std::string parent = "{\"version\":1,\"nodes\":[],\"problems\":[],\"options\":[{\"name\":"
                             "\"b\",\"type\":\"bool\",\"default\":\"true\","
                             "\"description\":\"\",\"value\":\"true\"}]}";
  const std::string child = "{\"version\":1,\"nodes\":[],\"problems\":[],\"options\":[{\"name\":"
                            "\"b\",\"type\":\"bool\",\"default\":\"true\","
                            "\"description\":\"\",\"value\":\"true\"},{\"name\":\"a\",\"type\":"
                            "\"i32\",\"default\":\"1\",\"description\":\"\","
                            "\"value\":\"1\"},{\"name\":\"c\",\"type\":\"String\",\"default\":"
                            "\"x\",\"description\":\"\",\"value\":\"x\"}]}";
  const std::string clash = "{\"version\":1,\"nodes\":[],\"problems\":[],\"options\":[{\"name\":"
                            "\"c\",\"type\":\"i32\",\"default\":\"4\","
                            "\"description\":\"\",\"value\":\"4\"}]}";
  std::string merged;
  std::string error;
  REQUIRE(
      primec::mergeBuildGraphJson(parent, {{"one", child}, {"two", clash}}, ".", merged, error));
  primec::BuildGraphData graph;
  REQUIRE(primec::parseBuildGraphJson(merged, graph, error));
  REQUIRE(graph.options.size() == 3);
  CHECK(graph.options[0].name == "a");
  CHECK(graph.options[1].name == "b");
  CHECK(graph.options[2].name == "c");
  CHECK(graph.options[2].type == "String");
  REQUIRE(graph.problems.size() == 1);
  CHECK(graph.problems[0] == "two: option c is declared as i32 with default 4, but another script "
                             "declares String with default x");
}

TEST_SUITE_END();
