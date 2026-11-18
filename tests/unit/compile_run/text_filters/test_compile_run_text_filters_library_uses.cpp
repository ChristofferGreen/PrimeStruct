// `primec --emit-uses` (docs/spec/integration-and-tooling.md, Emit Uses): the library symbols a compile resolved, with
// the hashes of the library's symbol table, sorted, and without any effect on the other outputs.
#include "test_compile_run_text_filters_helpers.h"

#include "primec/testing/DumpNormalization.h"

using primec::testing::stripDumpTimings;

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

// Twenty functions would say the same as five; each one has its own body so its content hash differs.
const char *UiTemplate = R"([namespace] ui {
  [return<i32>] helper([i32] a) { return(a + 1i32) }
  [struct] Hidden() {
    [i32] n{3i32}
    [return<i32>] value() { return(this.n) }
  }
  [return<T>] pass_through<T>([T] a) { return(a) }
  [public struct]
  Box<T>() {
    [public T] value{}
    [public return<T>] get() { return(pass_through<T>(this.value)) }
    [public return<i32>] bumped() {
      [Hidden] h{Hidden{}}
      return(helper(h.value()))
    }
  }
  [public return<i32>] add([i32] a, [i32] b) { return(a + b) }
  [public return<i32>] sub([i32] a, [i32] b) { return(a - b) }
  [public return<i32>] mul([i32] a, [i32] b) { return(a * b@MUL@) }
  [public return<i32>] twice([i32] a) { return(add(a, a)) }
  [public return<i32>] count_up<T>([T] a, [i32] n) { return(helper(n)) }
})";

std::string uiSource(const std::string &mulTail = "") {
  std::string text = UiTemplate;
  text.replace(text.find("@MUL@"), 5, mulTail);
  return text;
}

const std::string TwoCalls = "import</ui>\nimport /ui/*\n[return<i32>]\nmain() {\n  "
                             "return(add(1i32, 2i32) + sub(5i32, 1i32))\n}\n";

const std::string Nothing =
    "import</ui>\nimport /ui/*\n[return<i32>]\nmain() {\n  return(3i32)\n}\n";

const std::string GenericUse =
    "import</ui>\nimport /ui/*\n[return<i32>]\nmain() {\n  [Box<i64>] b{Box<i64>{7i64}}\n"
    "  return(convert<i32>(b.get()) + b.bumped())\n}\n";

struct Run {
  int exitCode = 0;
  std::string out;
  std::string err;
};

Run runPrimec(const std::filesystem::path &dir, const std::string &arguments) {
  const std::string outPath = (dir / "run_out.txt").string();
  const std::string errPath = (dir / "run_err.txt").string();
  Run run;
  run.exitCode = runCommand("./primec " + arguments + " > " + quoteShellArg(outPath) + " 2> " +
                            quoteShellArg(errPath));
  run.out = readFile(outPath);
  run.err = readFile(errPath);
  return run;
}

void writeIn(const std::filesystem::path &dir, const std::string &name, const std::string &text) {
  std::filesystem::create_directories(dir);
  std::ofstream file(dir / name, std::ios::binary);
  REQUIRE(file.good());
  file << text;
}

std::string buildLibrary(const std::filesystem::path &dir, const std::string &source) {
  writeIn(dir, "ui.prime", source);
  const std::string archive = (dir / "ui.psa").string();
  const Run run = runPrimec(dir,
                            "--emit=lib " + quoteShellArg((dir / "ui.prime").string()) + " -o " +
                                quoteShellArg(archive));
  REQUIRE_MESSAGE(run.exitCode == 0, run.err);
  return archive;
}

// Compiles `program` against `archive` with `--emit-uses` and returns the file's lines.
std::vector<std::string>
usesOf(const std::filesystem::path &dir, const std::string &archive, const std::string &program) {
  writeIn(dir, "app.prime", program);
  const std::string usesPath = (dir / "app.uses").string();
  std::filesystem::remove(usesPath);
  const Run run =
      runPrimec(dir,
                "--emit=interface --library " + quoteShellArg(archive) + " --emit-uses " +
                    quoteShellArg(usesPath) + " " + quoteShellArg((dir / "app.prime").string()));
  REQUIRE_MESSAGE(run.exitCode == 0, run.err);
  REQUIRE(std::filesystem::exists(usesPath));
  std::vector<std::string> lines;
  std::istringstream in(readFile(usesPath));
  for (std::string line; std::getline(in, line);) {
    lines.push_back(line);
  }
  return lines;
}

std::string pathOf(const std::string &line) {
  return line.substr(0, line.find(' '));
}

std::vector<std::string> pathsOf(const std::vector<std::string> &lines) {
  std::vector<std::string> paths;
  for (const std::string &line : lines) {
    paths.push_back(pathOf(line));
  }
  return paths;
}

const std::string *lineFor(const std::vector<std::string> &lines, const std::string &path) {
  for (const std::string &line : lines) {
    if (pathOf(line) == path) {
      return &line;
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("emit-uses lists exactly the library functions a program calls") {
  const std::filesystem::path dir = testScratchPath("emit_uses_two_calls");
  const std::string archive = buildLibrary(dir, uiSource());
  const std::vector<std::string> first = usesOf(dir, archive, TwoCalls);
  CHECK(pathsOf(first) == std::vector<std::string>{"/ui/add", "/ui/sub"});
  // path, library, signature hash, content hash.
  for (const std::string &line : first) {
    CAPTURE(line);
    CHECK(line.size() == pathOf(line).size() + 1 + 2 + 1 + 64 + 1 + 64);
    CHECK(line.compare(pathOf(line).size(), 4, " ui ") == 0);
  }
  CHECK(usesOf(dir, archive, TwoCalls) == first);
}

TEST_CASE("emit-uses lists nothing for a program that imports a library and uses nothing") {
  const std::filesystem::path dir = testScratchPath("emit_uses_nothing");
  const std::string archive = buildLibrary(dir, uiSource());
  CHECK(usesOf(dir, archive, Nothing).empty());
}

TEST_CASE(
    "emit-uses lists the generic a program instantiated and the private definitions it reaches") {
  const std::filesystem::path dir = testScratchPath("emit_uses_generic");
  const std::string archive = buildLibrary(dir, uiSource());
  const std::vector<std::string> uses = usesOf(dir, archive, GenericUse);
  CHECK(pathsOf(uses) ==
        std::vector<std::string>{
            "/ui/Box", "/ui/Hidden", "/ui/Hidden/value", "/ui/helper", "/ui/pass_through"});
}

TEST_CASE("emit-uses does not list what the body of a used library function calls") {
  const std::filesystem::path dir = testScratchPath("emit_uses_nested_call");
  const std::string archive = buildLibrary(dir, uiSource());
  const std::vector<std::string> uses =
      usesOf(dir,
             archive,
             "import</ui>\nimport /ui/*\n[return<i32>]\nmain() {\n  return(twice(4i32))\n}\n");
  // twice() is compiled in the library, so what its body calls is not the program's.
  CHECK(pathsOf(uses) == std::vector<std::string>{"/ui/twice"});
}

TEST_CASE("emit-uses hashes: a body change moves the content hash of that symbol alone") {
  const std::filesystem::path dir = testScratchPath("emit_uses_hashes");
  const std::string program = "import</ui>\nimport /ui/*\n[return<i32>]\nmain() {\n  "
                              "return(add(1i32, 2i32) + mul(2i32, 3i32))\n}\n";
  const std::vector<std::string> before = usesOf(dir, buildLibrary(dir, uiSource()), program);
  const std::vector<std::string> after =
      usesOf(dir, buildLibrary(dir, uiSource(" + 0i32")), program);
  REQUIRE(pathsOf(before) == pathsOf(after));
  REQUIRE(pathsOf(before) == std::vector<std::string>{"/ui/add", "/ui/mul"});
  CHECK(*lineFor(before, "/ui/add") == *lineFor(after, "/ui/add"));
  const std::string &mulBefore = *lineFor(before, "/ui/mul");
  const std::string &mulAfter = *lineFor(after, "/ui/mul");
  CHECK(mulBefore != mulAfter);
  // The signature hash, the third word, stays; the content hash, the fourth, moves.
  const size_t cut = mulBefore.rfind(' ');
  CHECK(mulBefore.substr(0, cut) == mulAfter.substr(0, cut));
}

TEST_CASE("emit-uses leaves every other output unchanged") {
  const std::filesystem::path dir = testScratchPath("emit_uses_same_outputs");
  const std::string archive = buildLibrary(dir, uiSource());
  writeIn(dir, "app.prime", GenericUse);
  const std::string app = quoteShellArg((dir / "app.prime").string());
  const std::string libraryFlag = "--library " + quoteShellArg(archive) + " ";
  const std::string usesFlag = "--emit-uses " + quoteShellArg((dir / "app.uses").string()) + " ";
  const std::string plainCpp = (dir / "plain.cpp").string();
  const std::string flagCpp = (dir / "flag.cpp").string();
  CHECK(runPrimec(dir, "--emit=optcpp " + libraryFlag + app + " -o " + quoteShellArg(plainCpp))
            .exitCode == 0);
  CHECK(runPrimec(dir,
                  "--emit=optcpp " + libraryFlag + usesFlag + app + " -o " + quoteShellArg(flagCpp))
            .exitCode == 0);
  CHECK(stripDumpTimings(readFile(plainCpp)) == stripDumpTimings(readFile(flagCpp)));
  CHECK(!readFile(plainCpp).empty());
  const Run plainDump = runPrimec(dir, "--dump-stage ast " + libraryFlag + app);
  const Run flagDump = runPrimec(dir, "--dump-stage ast " + libraryFlag + usesFlag + app);
  CHECK(plainDump.exitCode == 0);
  CHECK(stripDumpTimings(plainDump.out) == stripDumpTimings(flagDump.out));
  CHECK(!plainDump.out.empty());
}

TEST_CASE("emit-uses without a library writes an empty file and reports a file it cannot write") {
  const std::filesystem::path dir = testScratchPath("emit_uses_no_library");
  writeIn(dir, "app.prime", "[return<i32>]\nmain() {\n  return(1i32)\n}\n");
  const std::string app = quoteShellArg((dir / "app.prime").string());
  const std::string usesPath = (dir / "app.uses").string();
  CHECK(runPrimec(dir, "--emit=interface --emit-uses " + quoteShellArg(usesPath) + " " + app)
            .exitCode == 0);
  CHECK(readFile(usesPath).empty());
  const Run bad =
      runPrimec(dir, "--emit=interface --emit-uses /nonexistent_dir_primestruct/x.txt " + app);
  CHECK(bad.exitCode == 2);
  CHECK(bad.err.find("failed to write --emit-uses file") != std::string::npos);
}

TEST_SUITE_END();
