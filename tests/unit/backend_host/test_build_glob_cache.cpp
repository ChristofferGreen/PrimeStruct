// A step whose glob listing changes runs again (docs/BuildSystemPlan.md section 8). make.prime globs
// `src/*.prime` with /std/glob and hands the directories it listed to `b.watch`; the step compiles `main.prime`, which
// imports none of the matches, so only the watched listing can make it stale. A run prints `built <id>` or
// `up to date <id>`. No case depends on timestamps or sleeps: the cache compares content and listing hashes.
// Each case is a CTest entry of its own (CMakeLists.txt). A `primec build` compiles make.prime with the build stdlib
// once, about 1.2 s, and keeps the compiled script in the project (BuildScriptCache.h), so a run of an unchanged script
// is a fraction of that and only a case that rewrites make.prime compiles again.
#include "primec/pipeline/BuildCache.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_glob_cache");

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

class GlobProject {
public:
  explicit GlobProject(const std::string &name) : dir_(primec::testing::testScratchDir(name)) {
    std::filesystem::remove_all(dir_);
    writeFile(dir_ / "src/one.prime", "[return<i32>]\none() {\n  return(1i32)\n}\n");
    writeFile(dir_ / "src/sub/two.prime", "[return<i32>]\ntwo() {\n  return(2i32)\n}\n");
    writeFile(dir_ / "main.prime", "[return<i32>]\nmain() {\n  return(3i32)\n}\n");
    writeFile(dir_ / "elsewhere/other.txt", "not watched\n");
    writeScript("**/*.prime", "compile:app");
  }

  // The script globs `pattern` below src and watches `target`.
  void writeScript(const std::string &pattern, const std::string &target) {
    writeFile(dir_ / "make.prime",
              "import /std/build/*\nimport /std/glob/*\nimport /std/file/*\nimport /std/collections/vector/*\n"
              "import /std/text/*\nimport /std/text/view/*\n\n"
              "[return<Graph> effects(heap_alloc, file_read)]\nbuild([Build mut] b) {\n"
              "  [vector<String> mut] sources{vector<String>{}}\n  [vector<String> mut] listed{vector<String>{}}\n"
              "  [Result<FileError>] found{glob(\"" +
                  (dir_ / "src").string() + "\", \"" + pattern +
                  "\", sources, listed)}\n"
                  "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"ir\")\n  b.compile(\"app\", cc, \"main.prime\")\n"
                  "  b.watch(\"" +
                  target + "\", listed)\n  return(b.graph())\n}\n");
  }

  std::filesystem::path dir() const { return dir_; }
  std::filesystem::path src() const { return dir_ / "src"; }

  BuildResult build(const std::string &options = "--jobs 1") const {
    BuildResult result;
    const std::string out = (dir_ / "build.stdout").string();
    result.status = run("'" + Primec + "' build '" + dir_.string() + "' " + options + " > '" + out + "' 2> '" + out +
                        ".err'");
    result.out = readFile(out);
    result.err = readFile(out + ".err");
    return result;
  }

private:
  std::filesystem::path dir_;
};

const std::string Built = "built compile:app\n";
const std::string UpToDate = "up to date compile:app\n";

std::string firstBuildAndCheck(const GlobProject &project) {
  const BuildResult first = project.build();
  REQUIRE_MESSAGE(first.status == 0, first.err);
  CHECK(first.out == Built);
  return first.out;
}
} // namespace

TEST_CASE("a second run of an unchanged glob build runs no step") {
  GlobProject project("build_glob_unchanged");
  firstBuildAndCheck(project);
  const BuildResult second = project.build();
  CHECK_MESSAGE(second.status == 0, second.err);
  CHECK(second.out == UpToDate);
}

TEST_CASE("a file that appears in a listed directory runs the step again") {
  GlobProject project("build_glob_added");
  firstBuildAndCheck(project);
  writeFile(project.src() / "three.prime", "[return<i32>]\nthree() {\n  return(3i32)\n}\n");
  const BuildResult again = project.build();
  CHECK_MESSAGE(again.status == 0, again.err);
  CHECK(again.out == Built);
}

TEST_CASE("a file that appears in a listed subdirectory runs the step again") {
  GlobProject project("build_glob_added_deep");
  firstBuildAndCheck(project);
  writeFile(project.src() / "sub" / "four.prime", "[return<i32>]\nfour() {\n  return(4i32)\n}\n");
  CHECK(project.build().out == Built);
}

TEST_CASE("a matched file that is renamed runs the step again, once") {
  GlobProject project("build_glob_renamed");
  firstBuildAndCheck(project);
  std::filesystem::rename(project.src() / "one.prime", project.src() / "uno.prime");
  CHECK(project.build().out == Built);
  CHECK(project.build().out == UpToDate);
}

TEST_CASE("a matched file that is removed runs the step again") {
  GlobProject project("build_glob_removed");
  firstBuildAndCheck(project);
  std::filesystem::remove(project.src() / "one.prime");
  CHECK(project.build().out == Built);
}

TEST_CASE("rewriting a matched file in place and touching an unwatched directory run nothing") {
  // The listing is the input: names and kinds, not contents. A compile step hashes the files it read itself.
  GlobProject project("build_glob_unrelated");
  firstBuildAndCheck(project);
  writeFile(project.src() / "one.prime", "[return<i32>]\none() {\n  return(10i32)\n}\n");
  writeFile(project.dir() / "elsewhere" / "more.txt", "a new file in a directory nothing watches\n");
  writeFile(project.dir() / "notes.txt", "so is this\n");
  const BuildResult again = project.build();
  CHECK_MESSAGE(again.status == 0, again.err);
  CHECK(again.out == UpToDate);
}

TEST_CASE("a file that does not match the pattern but sits in a listed directory also runs the step") {
  // The runner does not know which entries matched, only which directories were listed: conservative, never stale.
  GlobProject project("build_glob_nonmatching");
  firstBuildAndCheck(project);
  writeFile(project.src() / "readme.txt", "not a source\n");
  CHECK(project.build().out == Built);
}

TEST_CASE("a pattern that starts to read another directory is another step even when no listing changed") {
  GlobProject project("build_glob_pattern");
  firstBuildAndCheck(project);
  // "*.prime" reads only src, so the key (which names the watched directories) differs from "**/*.prime".
  project.writeScript("*.prime", "compile:app");
  CHECK(project.build().out == Built);
}

TEST_CASE("the dry run exports the watched directories sorted") {
  GlobProject project("build_glob_export");
  const BuildResult dry = project.build("--dry-run");
  REQUIRE_MESSAGE(dry.status == 0, dry.err);
  const std::string src = project.src().string();
  CHECK(dry.out.find("\"inputDirs\":[\"" + src + "\",\"" + src + "/sub\"]") != std::string::npos);
  CHECK(dry.out.find("\"problems\":[]") != std::string::npos);
}

TEST_CASE("a watch of a missing target is a problem") {
  GlobProject project("build_glob_missing");
  project.writeScript("**/*.prime", "compile:nothing");
  const BuildResult missing = project.build("--dry-run");
  CHECK(missing.out.find("watch names a missing target compile:nothing") != std::string::npos);
}

TEST_CASE("the cache file records the listing hash of each watched directory") {
  GlobProject project("build_glob_entry");
  firstBuildAndCheck(project);
  std::string cache;
  for (const auto &entry : std::filesystem::directory_iterator(project.dir() / ".ps" / "cache")) {
    if (entry.path().extension() == ".cache") {
      cache = readFile(entry.path().string());
    }
  }
  const std::string src = project.src().string();
  CHECK(cache.find("dir " + primec::hashDirectoryListing(src) + " " + src + "\n") != std::string::npos);
  CHECK(cache.find("dir " + primec::hashDirectoryListing(src + "/sub") + " " + src + "/sub\n") != std::string::npos);
}

TEST_CASE("the listing hash covers names and kinds only") {
  const std::filesystem::path root = primec::testing::testScratchDir("build_glob_hash");
  std::filesystem::remove_all(root);
  writeFile(root / "a.txt", "1");
  std::filesystem::create_directories(root / "d");
  const std::string base = primec::hashDirectoryListing(root.string());
  CHECK(base == primec::hashDirectoryListing(root.string()));
  // Contents do not matter.
  writeFile(root / "a.txt", "changed");
  CHECK(primec::hashDirectoryListing(root.string()) == base);
  // A new entry, a rename and a different kind do.
  writeFile(root / "b.txt", "");
  const std::string added = primec::hashDirectoryListing(root.string());
  CHECK(added != base);
  std::filesystem::rename(root / "b.txt", root / "c.txt");
  const std::string renamed = primec::hashDirectoryListing(root.string());
  CHECK(renamed != added);
  std::filesystem::remove(root / "c.txt");
  std::filesystem::create_directories(root / "c.txt");
  CHECK(primec::hashDirectoryListing(root.string()) != renamed);
  // Missing and not-a-directory differ from every listing, and a file is not a directory.
  CHECK(primec::hashDirectoryListing((root / "nowhere").string()) == "missing");
  CHECK(primec::hashDirectoryListing((root / "a.txt").string()) == "missing");
  CHECK(base != "missing");
}

TEST_SUITE_END();
