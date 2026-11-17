// `b.subproject("dir")` of /std/build (docs/BuildSystemPlan.md section 7): `primec build` runs the make.prime of
// each named folder in a VM run of its own and merges the graphs under the folder's name. A `primec build` compiles each
// script with the build stdlib, about 1.2 s cold, and keeps the compiled script in the project, so every case keeps its
// number of scripts and of levels (a level of scripts runs at once) small, and each case is a CTest entry of its own
// (CMakeLists.txt).
#include "primec/pipeline/BuildGraphJson.h"
#include "primec/pipeline/BuildSubprojects.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_subprojects");

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

// A make.prime whose `build` has `body` between the creation of the compiler `cc` (emit "ir") and the returned graph.
std::string script(const std::string &body, const std::string &effects = "heap_alloc") {
  return "import /std/build/*\n\n[return<Graph> effects(" + effects +
         ")]\nbuild([Build mut] b) {\n  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n" +
         body + "  return(b.graph())\n}\n";
}

const std::string Program = "[return<i32>]\nmain() {\n  return(3i32)\n}\n";

class Project {
public:
  explicit Project(const std::string &name) : dir_(primec::testing::testScratchDir(name)) {
    std::filesystem::remove_all(dir_);
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

private:
  std::filesystem::path dir_;
};

std::string node(const std::string &id,
                 const std::string &kind,
                 const std::string &name,
                 const std::string &source,
                 const std::string &output,
                 const std::string &importPaths = "",
                 const std::string &linkArgs = "") {
  return "{\"id\":\"" + id + "\",\"kind\":\"" + kind + "\",\"name\":\"" + name +
         "\",\"source\":\"" + source + "\",\"output\":\"" + output +
         "\",\"compiler\":{\"emit\":\"ir\",\"optimize\":0,\"entry\":\"/main\","
         "\"import_paths\":[" +
         importPaths + "],\"transforms\":[],\"default_effects\":[],\"link_args\":[" + linkArgs +
         "]},\"runnable\":true}";
}
} // namespace

TEST_CASE(
    "two subfolders merge into one graph with namespaced ids and paths relative to the project") {
  Project project("build_sub_merge");
  project.write(
      "make.prime",
      script("  b.compile(\"root_ir\", cc, \"src/main.prime\")\n  b.subproject(\"tools\")\n"
             "  b.subproject(\"app\")\n"));
  project.write(
      "app/make.prime",
      script("  cc.importPath(\"lib\")\n  b.executable(\"app\", cc, \"src/main.prime\")\n"));
  project.write("tools/make.prime",
                script("  cc.linkArg(\"native.cpp\")\n  cc.linkArg(\"-lm\")\n  b.compile(\"gen\", "
                       "cc, \"gen.prime\")\n"));
  project.write("tools/native.cpp", "// a native source of the subproject\n");
  const BuildResult dry = project.build("--dry-run");
  REQUIRE_MESSAGE(dry.status == 0, dry.err);
  const std::string expected =
      "{\"version\":1,\"nodes\":[" +
      node("app/exe:app", "executable", "app", "app/src/main.prime", "app/app", "\"app/lib\"") +
      "," + node("compile:root_ir", "compile", "root_ir", "src/main.prime", "root_ir") + "," +
      node("tools/compile:gen",
           "compile",
           "gen",
           "tools/gen.prime",
           "tools/gen",
           "",
           "\"tools/native.cpp\",\"-lm\"") +
      "],\"problems\":[]}\n";
  CHECK(dry.out == expected);
  // The same text again, from the cached scripts.
  const BuildResult again = project.build("--dry-run");
  CHECK(again.out == dry.out);
}

TEST_CASE("a subproject that two scripts include is evaluated once") {
  // The project and `a` both name `b`, once as "b" and once as "../b/"; a third level would cost another cold compile.
  Project project("build_sub_diamond");
  project.write("make.prime", script("  b.subproject(\"b\")\n  b.subproject(\"a\")\n"));
  project.write("a/make.prime",
                script("  b.compile(\"from_a\", cc, \"a.prime\")\n  b.subproject(\"../b/\")\n"));
  project.write("b/make.prime", script("  b.compile(\"once\", cc, \"b.prime\")\n"));
  const BuildResult dry = project.build("--dry-run");
  REQUIRE_MESSAGE(dry.status == 0, dry.err);
  const std::string expected =
      "{\"version\":1,\"nodes\":[" +
      node("a/compile:from_a", "compile", "from_a", "a/a.prime", "a/from_a") + "," +
      node("b/compile:once", "compile", "once", "b/b.prime", "b/once") + "],\"problems\":[]}\n";
  CHECK(dry.out == expected);
}

TEST_CASE("a subproject cycle is a diagnostic that names the scripts") {
  Project project("build_sub_cycle");
  project.write("make.prime", script("  b.subproject(\"a\")\n"));
  project.write("a/make.prime", script("  b.subproject(\"..\")\n"));
  const BuildResult dry = project.build("--dry-run");
  CHECK(dry.status == 1);
  CHECK(dry.err == "primec build: subproject cycle: make.prime -> a/make.prime -> make.prime\n");
}

TEST_CASE("a missing make.prime and a folder outside the project are diagnostics") {
  Project project("build_sub_missing");
  project.write("make.prime", script("  b.subproject(\"nowhere\")\n"));
  const BuildResult missing = project.build("--dry-run");
  CHECK(missing.status == 1);
  CHECK(missing.err.find("make.prime: subproject \"nowhere\" has no make.prime (") !=
        std::string::npos);
  CHECK(missing.err.find("nowhere/make.prime)") != std::string::npos);
  project.write("make.prime", script("  b.subproject(\"../elsewhere\")\n"));
  const BuildResult outside = project.build("--dry-run");
  CHECK(outside.status == 1);
  CHECK(
      outside.err ==
      "primec build: make.prime: subproject \"../elsewhere\" is not a folder inside the project\n");
}

TEST_CASE("a subproject script that declares build_run is a diagnostic") {
  Project project("build_sub_declares_run");
  project.write("make.prime", script("  b.subproject(\"gen\")\n"));
  project.write("gen/make.prime",
                script("  b.compile(\"x\", cc, \"x.prime\")\n", "heap_alloc, build_run"));
  const BuildResult dry = project.build("--dry-run");
  CHECK(dry.status != 0);
  CHECK(dry.err.find("gen/make.prime") != std::string::npos);
  CHECK(dry.err.find("build_run") != std::string::npos);
}

TEST_CASE("a subproject script that calls Graph.run is a diagnostic") {
  Project project("build_sub_calls_run");
  project.write("make.prime", script("  b.subproject(\"gen\")\n"));
  project.write(
      "gen/make.prime",
      script("  b.compile(\"x\", cc, \"x.prime\")\n  [Graph] now{b.graph()}\n  now.run()\n"));
  const BuildResult dry = project.build("--dry-run");
  CHECK(dry.status != 0);
  CHECK(dry.err.find("gen/make.prime") != std::string::npos);
  CHECK(dry.err.find("build_run") != std::string::npos);
}

TEST_CASE("primec build builds the steps of a subproject under its folder and caches them") {
  Project project("build_sub_run");
  project.write("make.prime", script("  b.subproject(\"app\")\n"));
  project.write("app/make.prime", script("  b.compile(\"prog\", cc, \"src/main.prime\")\n"));
  project.write("app/src/main.prime", Program);
  const BuildResult first = project.build("--jobs 1");
  REQUIRE_MESSAGE(first.status == 0, first.err);
  CHECK(first.out == "built app/compile:prog\n");
  CHECK(std::filesystem::exists(project.dir() / "out" / "app" / "prog"));
  // The subproject's script is compiled once and kept in its own folder.
  CHECK(std::filesystem::exists(project.dir() / "app" / ".ps" / "cache"));
  const BuildResult second = project.build("--jobs 1");
  CHECK_MESSAGE(second.status == 0, second.err);
  CHECK(second.out == "up to date app/compile:prog\n");
  // A file the step read, relative to the subproject, is an input of the step.
  project.write("app/src/main.prime", "[return<i32>]\nmain() {\n  return(4i32)\n}\n");
  const BuildResult third = project.build("--jobs 1");
  CHECK_MESSAGE(third.status == 0, third.err);
  CHECK(third.out == "built app/compile:prog\n");
}

TEST_CASE("the merge prefixes ids and link ends, moves paths and sorts the nodes") {
  const std::string parent = "{\"version\":1,\"nodes\":[" +
                             node("exe:z", "executable", "z", "z.prime", "z") +
                             "],\"problems\":[],\"subprojects\":[\"s\"],\"incomplete\":true}";
  const std::string child =
      "{\"version\":1,\"nodes\":[" +
      node("exe:a", "executable", "a", "/abs/a.prime", "a", "\"../up\",\"/abs/inc\"") +
      ",{\"id\":\"link:a>b\",\"kind\":\"link\",\"from\":\"exe:a\",\"to\":\"lib:b\",\"runnable\":"
      "false}],"
      "\"problems\":[\"duplicate id exe:a\"]}";
  std::string merged;
  std::string error;
  REQUIRE(primec::mergeBuildGraphJson(parent, {{"sub/dir", child}}, ".", merged, error));
  CHECK(merged == "{\"version\":1,\"nodes\":[" + node("exe:z", "executable", "z", "z.prime", "z") +
                      "," +
                      node("sub/dir/exe:a",
                           "executable",
                           "a",
                           "/abs/a.prime",
                           "sub/dir/a",
                           "\"sub/up\",\"/abs/inc\"") +
                      ",{\"id\":\"sub/dir/link:a>b\",\"kind\":\"link\",\"from\":\"sub/dir/"
                      "exe:a\",\"to\":\"sub/dir/lib:b\","
                      "\"runnable\":false}],\"problems\":[\"sub/dir: duplicate id "
                      "exe:a\"],\"incomplete\":true}\n");
  // Watched directories move too; an absolute one stays.
  std::string watched = node("exe:w", "executable", "w", "w.prime", "w");
  watched.insert(watched.find("\"runnable\""), "\"inputDirs\":[\"src\",\"/abs\"],");
  REQUIRE(primec::mergeBuildGraphJson(
      parent, {{"d", "{\"nodes\":[" + watched + "],\"problems\":[]}"}}, ".", merged, error));
  CHECK(merged.find("\"inputDirs\":[\"d/src\",\"/abs\"]") != std::string::npos);
  primec::BuildGraphData graph;
  REQUIRE(primec::parseBuildGraphJson(
      "{\"nodes\":[],\"problems\":[],\"subprojects\":[\"b\",\"a\"]}", graph, error));
  CHECK(graph.subprojects == std::vector<std::string>{"b", "a"});
}

TEST_CASE("a subproject runs under the options of the project without build_run") {
  using primec::subprojectForwardedOptions;
  CHECK(subprojectForwardedOptions({}) ==
        std::vector<std::string>{"--allow-effects=file_read,heap_alloc"});
  CHECK(subprojectForwardedOptions({"--allow-effects", "heap_alloc,build_run,file_write"}) ==
        std::vector<std::string>{"--allow-effects",
                                 "heap_alloc,build_run,file_write",
                                 "--allow-effects=heap_alloc,file_write"});
  CHECK(subprojectForwardedOptions({"--allow-effects=build_run"}) ==
        std::vector<std::string>{"--allow-effects=build_run", "--allow-effects=none"});
}

TEST_SUITE_END();
