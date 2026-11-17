// The compiled make.prime of `primec build` is cached in the project (include/primec/pipeline/BuildScriptCache.h,
// docs/BuildSystemPlan.md section 8). The first cases drive the class with a tiny module and a scratch dependency file,
// so they compile nothing; the last two run `primec build --dry-run` on a project, which compiles make.prime once (about 2 s); an edit of the
// script is another key, which the key case covers without a second compile.
#include "primec/pipeline/BuildScriptCache.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.backend.build_script_cache");

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

// A module that returns 7, and the source it was "compiled" from: one file the cache has to keep watching.
class ScratchModule {
public:
  explicit ScratchModule(const std::string &name) : dir_(primec::testing::testScratchDir(name)) {
    std::filesystem::remove_all(dir_);
    writeFile(dependency(), "import one\n");
    primec::IrFunction function;
    function.name = "/main";
    function.instructions.push_back({primec::IrOpcode::PushI32, 7, 0});
    function.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 0});
    module_.functions.push_back(function);
    module_.entryIndex = 0;
    primec::SourceUnit unit;
    unit.kind = primec::SourceUnitKind::Import;
    unit.displayPath = dependency().string();
    sources_.units.push_back(unit);
  }

  std::filesystem::path dependency() const {
    return dir_ / "dep.prime";
  }
  std::string cacheDir() const {
    return primec::BuildScriptCache::cacheDirFor(dir_.string());
  }
  const primec::IrModule &module() const {
    return module_;
  }
  const primec::ExpandedSource &sources() const {
    return sources_;
  }

  primec::BuildScriptCache cache(const std::string &compilerHash = "compiler",
                                 const std::string &source = "text\n") const {
    return primec::BuildScriptCache(
        cacheDir(), compilerHash, {"primec", "--emit=vm"}, source, {"stdlib"});
  }

private:
  std::filesystem::path dir_;
  primec::IrModule module_;
  primec::ExpandedSource sources_;
};
} // namespace

TEST_CASE("a stored script module loads under the same key") {
  ScratchModule scratch("script_cache_roundtrip");
  primec::IrModule loaded;
  CHECK_FALSE(scratch.cache().load(loaded));
  REQUIRE(scratch.cache().store(scratch.module(), scratch.sources()));
  REQUIRE(scratch.cache().load(loaded));
  REQUIRE(loaded.functions.size() == 1);
  CHECK(loaded.functions[0].name == "/main");
  CHECK(loaded.functions[0].instructions.size() == 2);
  CHECK(loaded.entryIndex == 0);
}

TEST_CASE("a script module is not reused under another key") {
  ScratchModule scratch("script_cache_keys");
  REQUIRE(scratch.cache().store(scratch.module(), scratch.sources()));
  primec::IrModule loaded;
  CHECK_FALSE(scratch.cache("another compiler").load(loaded));
  CHECK_FALSE(scratch.cache("compiler", "other text\n").load(loaded));
  const primec::BuildScriptCache otherImports(
      scratch.cacheDir(), "compiler", {"primec", "--emit=vm"}, "text\n", {"elsewhere"});
  CHECK_FALSE(otherImports.load(loaded));
  const primec::BuildScriptCache otherArguments(
      scratch.cacheDir(), "compiler", {"primec", "--emit=vm", "-O3"}, "text\n", {"stdlib"});
  CHECK_FALSE(otherArguments.load(loaded));
  CHECK(scratch.cache().load(loaded));
}

TEST_CASE("a script module is not reused after a file it read changes") {
  ScratchModule scratch("script_cache_dependency");
  REQUIRE(scratch.cache().store(scratch.module(), scratch.sources()));
  primec::IrModule loaded;
  REQUIRE(scratch.cache().load(loaded));
  writeFile(scratch.dependency(), "import two\n");
  CHECK_FALSE(scratch.cache().load(loaded));
  std::filesystem::remove(scratch.dependency());
  CHECK_FALSE(scratch.cache().load(loaded));
}

TEST_CASE("a damaged script module is not loaded") {
  ScratchModule scratch("script_cache_damaged");
  REQUIRE(scratch.cache().store(scratch.module(), scratch.sources()));
  for (const auto &file : std::filesystem::directory_iterator(scratch.cacheDir())) {
    if (file.path().extension() == ".ir") {
      writeFile(file.path(), "not a module");
    }
  }
  primec::IrModule loaded;
  CHECK_FALSE(scratch.cache().load(loaded));
}

TEST_CASE("a cache without a compiler hash or directory never hits or stores") {
  ScratchModule scratch("script_cache_disabled");
  const primec::BuildScriptCache noCompiler(scratch.cacheDir(), "", {"primec"}, "text\n", {});
  const primec::BuildScriptCache noDirectory("", "compiler", {"primec"}, "text\n", {});
  primec::IrModule loaded;
  CHECK_FALSE(noCompiler.store(scratch.module(), scratch.sources()));
  CHECK_FALSE(noDirectory.store(scratch.module(), scratch.sources()));
  CHECK_FALSE(noCompiler.load(loaded));
  CHECK_FALSE(noDirectory.load(loaded));
  CHECK_FALSE(std::filesystem::exists(scratch.cacheDir()));
}

TEST_CASE("a cached make.prime gives the graph of a fresh compile") {
  const std::filesystem::path dir = primec::testing::testScratchDir("script_cache_dry_run");
  std::filesystem::remove_all(dir);
  const auto script = [](const std::string &target) {
    return "import /std/build/*\n\n[return<Graph> effects(heap_alloc)]\nbuild([Build mut] b) {\n"
           "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n  b.compile(\"" +
           target + "\", cc, \"main.prime\")\n  return(b.graph())\n}\n";
  };
  writeFile(dir / "make.prime", script("app"));
  writeFile(dir / "main.prime", "[return<i32>]\nmain() {\n  return(3i32)\n}\n");
  const std::string graphFile = (dir / "graph.json").string();
  const auto dryRun = [&]() {
    const int status = run("'" + Primec + "' build --dry-run '" + dir.string() + "' > '" +
                           graphFile + "' 2> '" + graphFile + ".err'");
    return status;
  };
  REQUIRE_MESSAGE(dryRun() == 0, readFile(graphFile + ".err"));
  const std::string fresh = readFile(graphFile);
  CHECK(fresh.find("\"compile:app\"") != std::string::npos);
  bool stored = false;
  for (const auto &file : std::filesystem::directory_iterator(dir / ".ps" / "cache")) {
    stored = stored || file.path().extension() == ".ir";
  }
  CHECK(stored);

  // The second run loads the stored module and prints the same graph.
  REQUIRE_MESSAGE(dryRun() == 0, readFile(graphFile + ".err"));
  CHECK(readFile(graphFile) == fresh);
}

TEST_CASE("a script that does not compile is reported on every run") {
  const std::filesystem::path dir = primec::testing::testScratchDir("script_cache_broken");
  std::filesystem::remove_all(dir);
  writeFile(
      dir / "make.prime",
      "import /std/build/*\n\n[return<Graph>]\nbuild([Build mut] b) {\n  return(missing(b))\n}\n");
  const std::string out = (dir / "out.txt").string();
  const std::string command = "'" + Primec + "' build --dry-run '" + dir.string() + "' > '" + out +
                              "' 2> '" + out + ".err'";
  CHECK(run(command) != 0);
  const std::string first = readFile(out + ".err");
  CHECK_FALSE(first.empty());
  CHECK(run(command) != 0);
  CHECK(readFile(out + ".err") == first);
  CHECK_FALSE(std::filesystem::exists(dir / ".ps" / "cache"));
}

TEST_SUITE_END();
