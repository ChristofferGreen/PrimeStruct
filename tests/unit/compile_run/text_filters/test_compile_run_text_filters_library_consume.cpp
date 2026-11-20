// `primec --library` (docs/spec/integration-and-tooling.md, Compiling against a library): `import</root>` reads the
// interface of an archive, `[imported(<library>)]` is valid only there, and a set of archives is checked before use.
#include "test_compile_run_text_filters_helpers.h"

#include "primec/pipeline/LibraryArchive.h"
#include "primec/support/ExecutableBuildId.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

// A library of a struct with methods, functions and an effect.
const std::string UiSource = R"(
[namespace summary("Drawing.")]
ui {
  [public struct summary("A point on the grid.")]
  Point() {
    [public i32] x{1i32}
    [public i32] y{2i32}

    [public return<i32>]
    sum() { return(this.x + this.y) }

    [public mut]
    shift([i32] dx) { this.x = this.x + dx }
  }

  [public return<i32> summary("Adds two numbers.")]
  add([i32] a, [i32] b{2i32}) {
    return(a + b)
  }

  [public effects(io_out) return<i32>]
  announce([i32] n) {
    print_line(n)
    return(n)
  }
}
)";

const std::string BaseSource = "[public return<i32>]\nbase_value() {\n  return(7i32)\n}\n";

// A program that uses the library through its declarations.
const std::string UsesUi = R"(import</ui>
import /ui/*

[return<i32> effects(io_out)]
main() {
  [Point mut] p{Point{3i32, 4i32}}
  p.shift(1i32)
  return(add(p.sum(), announce(1i32)))
}
)";

void writeIn(const std::filesystem::path &dir,
             const std::string &relative,
             const std::string &text) {
  const std::filesystem::path path = dir / relative;
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  REQUIRE(file.good());
  file << text;
}

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

// Builds `<name>.psa` from `source` written as `<name>.prime` in `dir`.
std::string
buildLibrary(const std::filesystem::path &dir, const std::string &name, const std::string &source) {
  writeIn(dir, name + ".prime", source);
  const std::string archive = (dir / (name + ".psa")).string();
  const Run run = runPrimec(dir,
                            "--emit=lib " + quoteShellArg((dir / (name + ".prime")).string()) +
                                " -o " + quoteShellArg(archive));
  REQUIRE_MESSAGE(run.exitCode == 0, run.err);
  return archive;
}

primec::LibraryArchiveExpectations expectations() {
  primec::LibraryArchiveExpectations expected;
  std::string idError;
  REQUIRE_MESSAGE(
      primec::executableBuildId(
          "./primec", primec::BuildIdSource::LinkerIdFirst, expected.compilerBuildId, idError),
      idError);
  return expected;
}

primec::LibraryArchiveContent readArchive(const std::string &path) {
  primec::LibraryArchiveContent content;
  std::string error;
  REQUIRE_MESSAGE(primec::readLibraryArchiveFile(path, expectations(), content, error), error);
  return content;
}

void rewriteArchive(const std::string &path, const primec::LibraryArchiveContent &content) {
  std::string error;
  REQUIRE_MESSAGE(primec::writeLibraryArchiveFile(path, content, error), error);
}

// `--emit=interface` of a consumer validates it without lowering it.
Run checkConsumer(const std::filesystem::path &dir,
                  const std::string &source,
                  const std::string &libraries) {
  writeIn(dir, "app.prime", source);
  return runPrimec(
      dir, "--emit=interface " + libraries + " " + quoteShellArg((dir / "app.prime").string()));
}

std::string libraryArgument(const std::string &archive) {
  return "--library " + quoteShellArg(archive);
}

} // namespace

TEST_CASE("library: a program validates against the printed interface, methods included") {
  const std::filesystem::path dir = testScratchPath("library_consume_validates");
  const std::string ui = buildLibrary(dir, "ui", UiSource);
  const std::string interfaceText = readArchive(ui).interfaceText;
  // The declarations carry the marker, and a method sits inside its struct so `this` stays implicit.
  CHECK(interfaceText.find("[public, return<i32>, imported(ui), summary(\"Adds two "
                           "numbers.\")] add(") != std::string::npos);
  CHECK(interfaceText.find("[public, mut, return<void>, imported(ui)] shift([i32] "
                           "dx) {}") != std::string::npos);
  CHECK(interfaceText.find("/ui/Point/sum") == std::string::npos);

  const Run run = checkConsumer(dir, UsesUi, libraryArgument(ui));
  CHECK_MESSAGE(run.exitCode == 0, run.err);
  CHECK(run.err.empty());
}

TEST_CASE("library: an import of a root no library owns is not found") {
  const std::filesystem::path dir = testScratchPath("library_consume_no_library");
  const Run run = checkConsumer(dir, UsesUi, "");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("failed to read import: /ui") != std::string::npos);
}

TEST_CASE("library: the marker in a program source is a diagnostic") {
  const std::filesystem::path dir = testScratchPath("library_consume_marker_in_program");
  const Run run = checkConsumer(dir,
                                "[return<i32> imported(ui)]\nfetch() {}\n\n[return<i32>]\nmain() "
                                "{\n  return(0i32)\n}\n",
                                "");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("imported is only valid in a library interface: /fetch") != std::string::npos);
}

TEST_CASE("library: the marker names the library whose interface holds it") {
  const std::filesystem::path dir = testScratchPath("library_consume_marker_name");
  const std::string ui = buildLibrary(dir, "ui", UiSource);
  primec::LibraryArchiveContent content = readArchive(ui);
  const size_t at = content.interfaceText.find("imported(ui)");
  REQUIRE(at != std::string::npos);
  content.interfaceText.replace(at, std::string("imported(ui)").size(), "imported(other)");
  rewriteArchive(ui, content);
  const Run run = checkConsumer(dir, UsesUi, libraryArgument(ui));
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("imported(other) in the interface of library ui") != std::string::npos);
}

TEST_CASE(
    "library: an imported declaration has an empty body, a return type and one library name") {
  const std::filesystem::path dir = testScratchPath("library_consume_declaration_shape");
  struct Case {
    const char *to;
    const char *message;
  };
  const Case cases[] = {
      {"[public, return<i32>, imported(base)] base_value() { return(1i32) }",
       "imported declaration must have an empty body: /base_value"},
      {"[public, imported(base)] base_value() {}",
       "imported declaration requires an explicit return type: /base_value"},
      {"[public, return<i32>, imported(base, other)] base_value() {}",
       "imported takes one library name, as in imported(ui): /base_value"},
  };
  for (const Case &testCase : cases) {
    CAPTURE(std::string(testCase.to));
    const std::string base = buildLibrary(dir, "base", BaseSource);
    primec::LibraryArchiveContent content = readArchive(base);
    const std::string from = "[public, return<i32>, imported(base)] base_value() {}";
    const size_t at = content.interfaceText.find(from);
    REQUIRE(at != std::string::npos);
    content.interfaceText.replace(at, from.size(), testCase.to);
    rewriteArchive(base, content);
    const Run run =
        checkConsumer(dir,
                      "import</base_value>\n\n[return<i32>]\nmain() {\n  return(0i32)\n}\n",
                      libraryArgument(base));
    CAPTURE(run.err);
    CHECK(run.exitCode == 2);
    CHECK(run.err.find(testCase.message) != std::string::npos);
  }
}

TEST_CASE("library: a missing transitive library and a stale one are diagnostics") {
  const std::filesystem::path dir = testScratchPath("library_consume_transitive");
  const std::string base = buildLibrary(dir, "base", BaseSource);
  const std::string ui = buildLibrary(dir, "ui", UiSource);
  // `ui` was compiled against `base`: the manifest records it.
  primec::LibraryArchiveContent content = readArchive(ui);
  content.manifest.libraries.push_back({"base", readArchive(base).manifest.interfaceHash});
  rewriteArchive(ui, content);

  const Run missing = checkConsumer(dir, UsesUi, libraryArgument(ui));
  CHECK(missing.exitCode == 2);
  CHECK(missing.err.find("library ui needs library base") != std::string::npos);

  const Run both = checkConsumer(dir, UsesUi, libraryArgument(ui) + " " + libraryArgument(base));
  CHECK_MESSAGE(both.exitCode == 0, both.err);

  // `base` is rebuilt with another interface after `ui` was compiled.
  const std::string changed =
      buildLibrary(dir, "base", "[public return<i64>]\nbase_value() {\n  return(7i64)\n}\n");
  const Run stale =
      checkConsumer(dir, UsesUi, libraryArgument(ui) + " " + libraryArgument(changed));
  CHECK(stale.exitCode == 2);
  CHECK(stale.err.find("library base changed since library ui was compiled; rebuild") !=
        std::string::npos);

  const Run twice = checkConsumer(dir, UsesUi, libraryArgument(base) + " " + libraryArgument(base));
  CHECK(twice.exitCode == 2);
  CHECK(twice.err.find("library base is supplied twice") != std::string::npos);
}

TEST_CASE("library: a damaged archive is refused and a library built alone records no library") {
  const std::filesystem::path dir = testScratchPath("library_consume_damaged");
  const std::string ui = buildLibrary(dir, "ui", UiSource);
  CHECK(readArchive(ui).manifest.libraries.empty());
  std::string bytes;
  {
    std::ifstream in(ui, std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }
  const size_t at = bytes.find("\"symbols\"");
  REQUIRE(at != std::string::npos);
  bytes[at] = '#';
  const std::string damaged = (dir / "damaged.psa").string();
  {
    std::ofstream out(damaged, std::ios::binary);
    out << bytes;
  }
  const Run run = checkConsumer(dir, UsesUi, libraryArgument(damaged));
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("cannot use library " + damaged) != std::string::npos);
  CHECK(run.err.find("rebuild the library") != std::string::npos);
}

TEST_CASE(
    "library: the interface of a module with an enum, a nested namespace and an inferred return "
    "validates as a program") {
  const std::filesystem::path dir = testScratchPath("library_consume_shapes");
  const std::string shapes = buildLibrary(dir, "shapes", R"(
[namespace summary("Drawing.")]
canvas {
  [public enum]
  Color {
    Red
    Green = 5
  }

  [public]
  inferred() {
    return(7i32)
  }

  [return<i32>]
  hidden() {
    return(1i32)
  }

  [namespace]
  inner {
    [public return<bool>]
    deep() { return(true) }
  }
}
)");
  const Run run = checkConsumer(dir,
                                "import</canvas>\nimport /canvas/*\n\n[return<i32>]\nmain() {\n  "
                                "[Color] c{Color.Green}\n  return(inferred())\n}\n",
                                libraryArgument(shapes));
  CHECK_MESSAGE(run.exitCode == 0, run.err);
  const std::string interfaceText = readArchive(shapes).interfaceText;
  CHECK(interfaceText.find("hidden") == std::string::npos);
  CHECK(interfaceText.find("[namespace] inner {") != std::string::npos);
}

namespace {

const std::string MainSource = "[return<i32>]\nmain() {\n  return(0i32)\n}\n";

const std::string DrawInUi =
    "[namespace]\nui {\n  [return<i32>]\n  draw() {\n    return(1i32)\n  }\n}\n\n" + MainSource;

} // namespace

TEST_CASE("library: two libraries that own the same root are refused") {
  const std::filesystem::path dir = testScratchPath("library_owner_overlap");
  const std::string ui = buildLibrary(dir, "ui", UiSource);
  const std::string other = buildLibrary(
      dir,
      "other",
      "[namespace]\nui {\n  [public return<i32>]\n  extra() {\n    return(2i32)\n  }\n}\n");
  const Run run =
      checkConsumer(dir, MainSource, libraryArgument(ui) + " " + libraryArgument(other));
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("library other and library ui both own /ui") != std::string::npos);
}

TEST_CASE("library: a program defining below a library root is owned-path diagnostic") {
  const std::filesystem::path dir = testScratchPath("library_owner_program_defines");
  const std::string ui = buildLibrary(dir, "ui", UiSource);
  // Without importing the library, and with it.
  const Run alone = checkConsumer(dir, DrawInUi, libraryArgument(ui));
  CHECK(alone.exitCode == 2);
  CHECK(alone.err.find("/ui/draw is owned by library ui") != std::string::npos);
  const Run imported = checkConsumer(dir, "import</ui>\n" + DrawInUi, libraryArgument(ui));
  CHECK(imported.exitCode == 2);
  CHECK(imported.err.find("/ui/draw is owned by library ui") != std::string::npos);
  // Without `--library` the same program compiles.
  const Run without = checkConsumer(dir, DrawInUi, "");
  CHECK_MESSAGE(without.exitCode == 0, without.err);
}

TEST_CASE("library: names next to a library root compile") {
  const std::filesystem::path dir = testScratchPath("library_owner_neighbours");
  const std::string ui = buildLibrary(dir, "ui", UiSource);
  const std::string source =
      "[namespace]\nui2 {\n  [return<i32>]\n  draw() {\n    return(1i32)\n  }\n}\n\n"
      "[namespace]\napp {\n  [return<i32>]\n  start() {\n    return(2i32)\n  }\n}\n\n" +
      MainSource;
  const Run run = checkConsumer(dir, source, libraryArgument(ui));
  CHECK_MESSAGE(run.exitCode == 0, run.err);
}

TEST_SUITE_END();
