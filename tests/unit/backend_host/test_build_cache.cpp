// `primec build` skips steps whose inputs did not change (docs/BuildSystemPlan.md section 8, src/pipeline/BuildCache.h).
// Every case builds a small project of two IR compile steps in a scratch directory: `alpha` imports a helper file and
// `beta` reads nothing else. A run prints `built <id>` or `up to date <id>` per step, so the cases compare those lines.
// No case depends on timestamps or sleeps: the cache compares content hashes only.
#include "primec/pipeline/BuildGraphJson.h"
#include "primec/pipeline/BuildRunner.h"
#include "primec/support/Sha256.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_cache");

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

int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

struct BuildResult {
  int status = 0;
  std::string out;
  std::string err;
};

// A project of two steps. `alpha` (src/alpha.prime) imports helper.prime; `beta` (src/beta.prime) imports nothing.
// unrelated.prime is in the project and imported by nothing.
class CacheProject {
public:
  CacheProject() : dir_(primec::testing::testScratchDir("build_cache")) {
    writeScript(0);
    writeHelper(1);
    writeFile(dir_ / "src/alpha.prime",
              "import<\"" + (dir_ / "helper.prime").string() +
                  "\">\n\n[return<i32>]\nmain() {\n  return(helper_a())\n}\n");
    writeFile(dir_ / "src/beta.prime", "[return<i32>]\nmain() {\n  return(2i32)\n}\n");
    writeFile(dir_ / "unrelated.prime", "[return<i32>]\nunrelated() {\n  return(9i32)\n}\n");
  }

  // beta's optimization level is the compiler setting the cases change.
  void writeScript(int betaOptimization) {
    writeFile(dir_ / "make.prime",
              "import /std/build/*\n\n[return<Graph> effects(heap_alloc)]\nbuild([Build mut] b) {\n"
              "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n  b.compile(\"alpha\", cc, \"src/alpha.prime\")\n"
              "  [Compiler mut] other{cc}\n  other.optimize(" +
                  std::to_string(betaOptimization) + "i32)\n  b.compile(\"beta\", other, \"src/beta.prime\")\n"
                                                     "  return(b.graph())\n}\n");
  }

  void writeHelper(int value) {
    writeFile(dir_ / "helper.prime",
              "[return<i32>]\nhelper_a() {\n  return(" + std::to_string(value) + "i32)\n}\n");
  }

  std::filesystem::path dir() const { return dir_; }
  std::filesystem::path outDir() const { return dir_ / "out"; }
  std::filesystem::path cacheDir() const { return dir_ / ".ps" / "cache"; }

  BuildResult build() const {
    BuildResult result;
    const std::string out = (dir_ / "build.stdout").string();
    result.status = run("'" + Primec + "' build '" + dir_.string() + "' --jobs 1 > '" + out + "' 2> '" + out + ".err'");
    result.out = readFile(out);
    result.err = readFile(out + ".err");
    return result;
  }

private:
  std::filesystem::path dir_;
};

const std::string BuiltBoth = "built compile:alpha\nbuilt compile:beta\n";
const std::string BothUpToDate = "up to date compile:alpha\nup to date compile:beta\n";
} // namespace

TEST_CASE("a second run of an unchanged build runs no step") {
  CacheProject project;
  const BuildResult first = project.build();
  REQUIRE_MESSAGE(first.status == 0, first.err);
  CHECK(first.out == BuiltBoth);
  const std::string alphaOutput = readFile((project.outDir() / "alpha").string());
  CHECK_FALSE(alphaOutput.empty());

  const BuildResult second = project.build();
  CHECK_MESSAGE(second.status == 0, second.err);
  CHECK(second.out == BothUpToDate);
  CHECK(readFile((project.outDir() / "alpha").string()) == alphaOutput);
}

TEST_CASE("touching an unrelated file or rewriting an import unchanged re-runs nothing") {
  CacheProject project;
  REQUIRE(project.build().status == 0);
  writeFile(project.dir() / "unrelated.prime", "[return<i32>]\nunrelated() {\n  return(10i32)\n}\n");
  writeFile(project.dir() / "notes.txt", "a new file that no step reads\n");
  project.writeHelper(1);
  const BuildResult again = project.build();
  CHECK_MESSAGE(again.status == 0, again.err);
  CHECK(again.out == BothUpToDate);
}

TEST_CASE("changing an imported file re-runs only the step that read it") {
  CacheProject project;
  REQUIRE(project.build().status == 0);
  const std::string before = readFile((project.outDir() / "alpha").string());
  project.writeHelper(7);
  const BuildResult again = project.build();
  CHECK_MESSAGE(again.status == 0, again.err);
  CHECK(again.out == "built compile:alpha\nup to date compile:beta\n");
  CHECK(readFile((project.outDir() / "alpha").string()) != before);
}

TEST_CASE("changing a compiler setting re-runs only the step it belongs to") {
  CacheProject project;
  REQUIRE(project.build().status == 0);
  project.writeScript(1);
  const BuildResult again = project.build();
  CHECK_MESSAGE(again.status == 0, again.err);
  CHECK(again.out == "up to date compile:alpha\nbuilt compile:beta\n");
}

TEST_CASE("deleting or replacing an output re-runs only that step") {
  CacheProject project;
  REQUIRE(project.build().status == 0);
  const std::string beta = readFile((project.outDir() / "beta").string());
  std::filesystem::remove(project.outDir() / "alpha");
  const BuildResult deleted = project.build();
  CHECK_MESSAGE(deleted.status == 0, deleted.err);
  CHECK(deleted.out == "built compile:alpha\nup to date compile:beta\n");

  // An output somebody edited is not the output the cache recorded.
  writeFile(project.outDir() / "beta", "edited\n");
  const BuildResult edited = project.build();
  CHECK_MESSAGE(edited.status == 0, edited.err);
  CHECK(edited.out == "up to date compile:alpha\nbuilt compile:beta\n");
  CHECK(readFile((project.outDir() / "beta").string()) == beta);
}

TEST_CASE("a build with the cache deleted gives byte-identical outputs") {
  CacheProject project;
  REQUIRE(project.build().status == 0);
  const std::string alpha = readFile((project.outDir() / "alpha").string());
  const std::string beta = readFile((project.outDir() / "beta").string());
  std::filesystem::remove_all(project.dir() / ".ps");
  std::filesystem::remove_all(project.outDir());
  const BuildResult rebuilt = project.build();
  CHECK_MESSAGE(rebuilt.status == 0, rebuilt.err);
  CHECK(rebuilt.out == BuiltBoth);
  CHECK(readFile((project.outDir() / "alpha").string()) == alpha);
  CHECK(readFile((project.outDir() / "beta").string()) == beta);
}

TEST_CASE("with the cache deleted and the outputs left in place the steps still run") {
  CacheProject project;
  REQUIRE(project.build().status == 0);
  // Nothing vouches for the outputs once the cache is gone.
  std::filesystem::remove_all(project.dir() / ".ps");
  CHECK(project.build().out == BuiltBoth);
}

TEST_CASE("a failed step leaves no cache entry that vouches for its output") {
  CacheProject project;
  REQUIRE(project.build().status == 0);
  writeFile(project.dir() / "helper.prime", "[return<i32>]\nhelper_a() {\n  return(\n");
  const BuildResult broken = project.build();
  CHECK(broken.status != 0);
  CHECK(broken.err.find("step alpha (compile:alpha) failed") != std::string::npos);
  // Back to the first helper: the output of the first run would match, but the failed run dropped its entry.
  project.writeHelper(1);
  const BuildResult fixed = project.build();
  CHECK_MESSAGE(fixed.status == 0, fixed.err);
  CHECK(fixed.out == "built compile:alpha\nup to date compile:beta\n");
}

TEST_CASE("a damaged cache file is rebuilt and not trusted") {
  CacheProject project;
  REQUIRE(project.build().status == 0);
  for (const auto &entry : std::filesystem::directory_iterator(project.cacheDir())) {
    writeFile(entry.path(), "not a cache file\n");
  }
  const BuildResult again = project.build();
  CHECK_MESSAGE(again.status == 0, again.err);
  CHECK(again.out == BuiltBoth);
  CHECK(project.build().out == BothUpToDate);
}

TEST_CASE("the cache file names its hash scheme and holds content hashes") {
  CacheProject project;
  REQUIRE(project.build().status == 0);
  std::string alphaCache;
  for (const auto &entry : std::filesystem::directory_iterator(project.cacheDir())) {
    const std::string name = entry.path().filename().string();
    if (name.rfind("compile_alpha-", 0) == 0) {
      alphaCache = readFile(entry.path().string());
    }
  }
  REQUIRE_FALSE(alphaCache.empty());
  CHECK(alphaCache.rfind("primestruct-build-cache 1\nhash sha256\nkey ", 0) == 0);
  const std::string helper = readFile((project.dir() / "helper.prime").string());
  CHECK(alphaCache.find("dep " + primec::sha256Hex(helper) + " " + (project.dir() / "helper.prime").string()) !=
        std::string::npos);
  CHECK(alphaCache.find("output " + primec::sha256Hex(readFile((project.outDir() / "alpha").string()))) !=
        std::string::npos);
  // beta imports nothing: its entry does not list the helper.
  for (const auto &entry : std::filesystem::directory_iterator(project.cacheDir())) {
    if (entry.path().filename().string().rfind("compile_beta-", 0) == 0) {
      CHECK(readFile(entry.path().string()).find("helper.prime") == std::string::npos);
    }
  }
}

// The compiler stands for its version through `executableBuildId`. A script is no object file, so its id is the hash of
// its text: a wrapper that runs the real `primec` is a compiler whose id the case changes by editing a comment.
TEST_CASE("a step is rebuilt when the compiler id changes and is up to date when it does not") {
  const std::filesystem::path dir = primec::testing::testScratchDir("build_cache_compiler_id");
  writeFile(dir / "beta.prime", "[return<i32>]\nmain() {\n  return(2i32)\n}\n");
  const std::filesystem::path wrapper = dir / "compiler.sh";
  auto writeWrapper = [&](const std::string &note) {
    writeFile(wrapper, "#!/bin/sh\n# " + note + "\nexec '" + Primec + "' \"$@\"\n");
    std::filesystem::permissions(wrapper, std::filesystem::perms::owner_all);
  };
  primec::BuildStep step;
  step.id = "compile:beta";
  step.kind = "compile";
  step.name = "beta";
  step.source = "beta.prime";
  step.output = "beta";
  step.emitKind = "ir";
  primec::BuildGraphData graph;
  graph.steps.push_back(step);
  auto build = [&]() {
    primec::BuildRunSettings settings;
    settings.primecPath = wrapper.string();
    settings.projectDir = dir.string();
    settings.outDir = (dir / "out").string();
    settings.cacheDir = (dir / ".ps" / "cache").string();
    settings.jobs = 1;
    std::ostringstream out;
    std::ostringstream err;
    const int code = primec::runBuildGraph(graph, settings, out, err);
    return code == 0 ? out.str() : out.str() + err.str();
  };
  writeWrapper("one");
  CHECK(build() == "built compile:beta\n");
  CHECK(build() == "up to date compile:beta\n");
  writeWrapper("two");
  CHECK(build() == "built compile:beta\n");
  CHECK(build() == "up to date compile:beta\n");
  // The same text again is the same compiler, so the entry of that id still holds.
  writeWrapper("two");
  CHECK(build() == "up to date compile:beta\n");
}

TEST_CASE("sha256 matches the published test vectors") {
  CHECK(primec::sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(primec::sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(primec::sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  CHECK(primec::sha256Hex(std::string(1000000, 'a')) ==
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_SUITE_END();
