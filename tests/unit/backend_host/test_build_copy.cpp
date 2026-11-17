// Copy steps and the generated C++ of executable steps (TODO-6017, TODO-6018; docs/BuildSystemPlan.md section 3).
// A project in a scratch directory has one IR compile step and `b.copy` steps; a run prints `built <id>` or
// `up to date <id>` per step. The cases compare those lines and the files in the output directory. No case depends on
// timestamps or sleeps: the cache compares content hashes only.
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_copy");

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

struct BuildResult {
  int status = 0;
  std::string out;
  std::string err;
};

class CopyProject {
public:
  explicit CopyProject(const std::string &name, const std::string &emit = "ir")
      : dir_(primec::testing::testScratchDir(name)) {
    std::filesystem::remove_all(dir_);
    writeFile(dir_ / "src/alpha.prime", "[return<i32>]\nmain() {\n  return(2i32)\n}\n");
    writeFile(dir_ / "assets/data.txt", "data one\n");
    writeFile(dir_ / "assets/other.txt", "other one\n");
    writeScript(emit,
                "  b.copy(\"blob\", \"assets/data.txt\", \"share/data.txt\")\n"
                "  b.copy(\"other\", \"assets/other.txt\", \"other.txt\")\n");
  }

  // `copies` are the lines of the script after the compile step.
  void writeScript(const std::string &emit, const std::string &copies) {
    writeFile(dir_ / "make.prime",
              "import /std/build/*\n\n[return<Graph> effects(heap_alloc)]\nbuild([Build mut] b) {\n"
              "  [Compiler mut] cc{b.compiler()}\n  cc.emit(\"" +
                  emit + "\")\n  b.compile(\"alpha\", cc, \"src/alpha.prime\")\n" + copies +
                  "  return(b.graph())\n}\n");
  }

  std::filesystem::path dir() const {
    return dir_;
  }
  std::filesystem::path outDir() const {
    return dir_ / "out";
  }

  BuildResult build() const {
    BuildResult result;
    const std::string out = (dir_ / "build.stdout").string();
    result.status = std::system(("'" + Primec + "' build '" + dir_.string() + "' --jobs 1 > '" +
                                 out + "' 2> '" + out + ".err'")
                                    .c_str());
    result.status = WIFEXITED(result.status) ? WEXITSTATUS(result.status) : -1;
    result.out = readFile(out);
    result.err = readFile(out + ".err");
    return result;
  }

private:
  std::filesystem::path dir_;
};

const std::string BuiltAll = "built compile:alpha\nbuilt copy:blob\nbuilt copy:other\n";
const std::string AllUpToDate =
    "up to date compile:alpha\nup to date copy:blob\nup to date copy:other\n";
} // namespace

TEST_CASE("a copy step puts a file into the output directory and an unchanged second run copies "
          "nothing") {
  CopyProject project("build_copy_basic");
  const BuildResult first = project.build();
  REQUIRE_MESSAGE(first.status == 0, first.err);
  CHECK(first.out == BuiltAll);
  CHECK(readFile(project.outDir() / "share/data.txt") == "data one\n");
  CHECK(readFile(project.outDir() / "other.txt") == "other one\n");

  const BuildResult second = project.build();
  CHECK_MESSAGE(second.status == 0, second.err);
  CHECK(second.out == AllUpToDate);
}

TEST_CASE("an edited copy source re-runs only its copy step") {
  CopyProject project("build_copy_edit_source");
  REQUIRE(project.build().status == 0);
  writeFile(project.dir() / "assets/data.txt", "data two\n");
  const BuildResult again = project.build();
  CHECK_MESSAGE(again.status == 0, again.err);
  CHECK(again.out == "up to date compile:alpha\nbuilt copy:blob\nup to date copy:other\n");
  CHECK(readFile(project.outDir() / "share/data.txt") == "data two\n");
}

TEST_CASE("a deleted or edited copy re-runs only its copy step") {
  CopyProject project("build_copy_damaged_output");
  REQUIRE(project.build().status == 0);
  std::filesystem::remove(project.outDir() / "other.txt");
  writeFile(project.outDir() / "share/data.txt", "tampered\n");
  const BuildResult again = project.build();
  CHECK_MESSAGE(again.status == 0, again.err);
  CHECK(again.out == "up to date compile:alpha\nbuilt copy:blob\nbuilt copy:other\n");
  CHECK(readFile(project.outDir() / "share/data.txt") == "data one\n");
  CHECK(readFile(project.outDir() / "other.txt") == "other one\n");
}

TEST_CASE("a copy destination outside the output directory is a diagnostic") {
  CopyProject project("build_copy_escape");
  project.writeScript("ir", "  b.copy(\"escape\", \"assets/data.txt\", \"../escaped.txt\")\n");
  const BuildResult result = project.build();
  CHECK(result.status != 0);
  CHECK(result.err.find("is not inside the output directory") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(project.dir() / "escaped.txt"));
}

TEST_CASE("an absolute copy destination is a diagnostic") {
  CopyProject project("build_copy_absolute");
  const std::string target = (project.dir() / "absolute.txt").string();
  project.writeScript("ir", "  b.copy(\"abs\", \"assets/data.txt\", \"" + target + "\")\n");
  const BuildResult result = project.build();
  CHECK(result.status != 0);
  CHECK(result.err.find("must be a relative path inside the output directory") !=
        std::string::npos);
  CHECK_FALSE(std::filesystem::exists(target));
}

TEST_CASE("a copy of a missing file is a diagnostic naming the step") {
  CopyProject project("build_copy_missing");
  project.writeScript("ir", "  b.copy(\"gone\", \"assets/missing.txt\", \"gone.txt\")\n");
  const BuildResult result = project.build();
  CHECK(result.status != 0);
  CHECK(result.err.find("step gone (copy:gone) failed") != std::string::npos);
  CHECK(result.err.find("is not a file") != std::string::npos);
}

TEST_CASE("an optexe step leaves the executable and no generated C++ and is up to date on the "
          "second run") {
  CopyProject project("build_copy_no_cpp", "optexe");
  project.writeScript("optexe", "");
  const BuildResult first = project.build();
  REQUIRE_MESSAGE(first.status == 0, first.err);
  CHECK(first.out == "built compile:alpha\n");
  CHECK(std::filesystem::exists(project.outDir() / "alpha"));
  CHECK_FALSE(std::filesystem::exists(project.outDir() / "alpha.cpp"));

  const BuildResult second = project.build();
  CHECK_MESSAGE(second.status == 0, second.err);
  CHECK(second.out == "up to date compile:alpha\n");
  CHECK_FALSE(std::filesystem::exists(project.outDir() / "alpha.cpp"));
}

TEST_SUITE_END();
