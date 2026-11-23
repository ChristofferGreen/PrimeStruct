// `primec --library` links a program with the code of a library (docs/spec/integration-and-tooling.md, Compiling
// against a library): the same output as the whole-program build, a refusal on a target without the address ABI, an
// unresolved import, and a library compiled against another library.
#include "test_compile_run_text_filters_helpers.h"

#include "primec/pipeline/LibraryArchive.h"
#include "primec/support/ExecutableBuildId.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

const std::string UiSource = R"(
[namespace summary("Drawing.")]
ui {
  [public struct]
  Point() {
    [public i32] x{1i32}
    [public i32] y{2i32}

    [public return<i32>]
    sum() { return(this.x + this.y) }

    [public mut]
    shift([i32] dx) { this.x = this.x + dx }
  }

  [public return<i32>]
  add([i32] a, [i32] b) {
    return(a + b)
  }

  [return<i32>]
  twice([i32] a) {
    return(add(a, a))
  }

  [public return<i32>]
  quad([i32] a) {
    return(twice(a) + twice(a))
  }

  [public effects(io_out) return<i32>]
  announce([i32] n) {
    print_line("announce"utf8)
    print_line(n)
    return(n)
  }

  [public return<Point>]
  moved([Point] p, [i32] dx) {
    return(Point{p.x + dx, p.y})
  }
}
)";

const std::string AppBody = R"(
[return<i32> effects(io_out, task)]
main() {
  [Point mut] p{Point{3i32, 4i32}}
  p.shift(1i32)
  [Point] q{moved(p, 10i32)}
  [Task<i32>] t{[spawn] add(q.x, 1i32)}
  print_line(q.sum())
  print_line(quad(2i32))
  print_line(wait(t))
  return(add(announce(1i32), 40i32))
}
)";

void writeIn(const std::filesystem::path &dir, const std::string &name, const std::string &text) {
  std::filesystem::create_directories((dir / name).parent_path());
  std::ofstream file(dir / name, std::ios::binary);
  REQUIRE(file.good());
  file << text;
}

struct Run {
  int exitCode = 0;
  std::string out;
  std::string err;
};

Run shell(const std::filesystem::path &dir, const std::string &command) {
  Run run;
  run.exitCode = runCommand(command + " > " + quoteShellArg((dir / "o.txt").string()) + " 2> " +
                            quoteShellArg((dir / "e.txt").string()));
  run.out = readFile((dir / "o.txt").string());
  run.err = readFile((dir / "e.txt").string());
  return run;
}

std::string buildLibrary(const std::filesystem::path &dir,
                         const std::string &name,
                         const std::string &source,
                         const std::string &extra = "") {
  writeIn(dir, name + ".prime", source);
  const std::string archive = (dir / (name + ".psa")).string();
  const Run run = shell(dir,
                        "./primec --emit=lib " + quoteShellArg((dir / (name + ".prime")).string()) +
                            " -o " + quoteShellArg(archive) + " " + extra);
  REQUIRE_MESSAGE(run.exitCode == 0, run.err);
  return archive;
}

// Compiles and runs `app.prime` with `emit` (vm runs it; optexe writes an executable that is run).
Run runApp(const std::filesystem::path &dir, const std::string &emit, const std::string &extra) {
  const std::string app = quoteShellArg((dir / "app.prime").string());
  if (emit == "vm") {
    return shell(dir, "./primec --emit=vm " + extra + " " + app + " --entry /main");
  }
  const std::string exe = (dir / ("app_" + emit)).string();
  const Run built = shell(dir,
                          "./primec --emit=" + emit + " " + extra + " " + app + " -o " +
                              quoteShellArg(exe) + " --entry /main");
  if (built.exitCode != 0) {
    return built;
  }
  return shell(dir, quoteShellArg(exe));
}

} // namespace

TEST_CASE(
    "library link: a program built against a library behaves as the whole-program build, vm") {
  const std::filesystem::path dir = testScratchPath("library_link_vm");
  writeIn(dir, "ui.prime", UiSource);
  writeIn(dir, "app.prime", "import<\"ui.prime\">\nimport /ui/*\n" + AppBody);
  const Run whole = runApp(dir, "vm", "");
  REQUIRE_MESSAGE(whole.exitCode == 41, whole.err);
  CHECK(whole.out == "18\n8\n15\nannounce\n1\n");

  const std::string ui = buildLibrary(dir, "ui", UiSource);
  writeIn(dir, "app.prime", "import</ui>\nimport /ui/*\n" + AppBody);
  const Run linked = runApp(dir, "vm", "--library " + quoteShellArg(ui));
  CHECK_MESSAGE(linked.err.empty(), linked.err);
  CHECK(linked.exitCode == whole.exitCode);
  CHECK(linked.out == whole.out);
}

TEST_CASE("library link: the same program on optexe") {
  const std::filesystem::path dir = testScratchPath("library_link_optexe");
  writeIn(dir, "ui.prime", UiSource);
  writeIn(dir, "app.prime", "import<\"ui.prime\">\nimport /ui/*\n" + AppBody);
  const Run whole = runApp(dir, "optexe", "");
  REQUIRE_MESSAGE(whole.exitCode == 41, whole.err);

  const std::string ui = buildLibrary(dir, "ui", UiSource);
  writeIn(dir, "app.prime", "import</ui>\nimport /ui/*\n" + AppBody);
  const Run linked = runApp(dir, "optexe", "--library " + quoteShellArg(ui));
  CHECK_MESSAGE(linked.err.empty(), linked.err);
  CHECK(linked.exitCode == whole.exitCode);
  CHECK(linked.out == whole.out);
}

TEST_CASE("library link: a target without the address ABI is refused") {
  const std::filesystem::path dir = testScratchPath("library_link_target");
  const std::string ui = buildLibrary(dir, "ui", UiSource);
  writeIn(dir,
          "app.prime",
          "import</ui>\nimport /ui/*\n\n[return<i32>]\nmain() {\n  return(add(1i32, 2i32))\n}\n");
  const Run run = shell(dir,
                        "./primec --emit=wasm --library " + quoteShellArg(ui) + " " +
                            quoteShellArg((dir / "app.prime").string()) + " -o " +
                            quoteShellArg((dir / "app.wasm").string()));
  CHECK(run.exitCode != 0);
  CHECK(run.err.find("linking a library needs a target with the address ABI") != std::string::npos);
  CHECK(run.err.find("/ui/add is imported from library ui") != std::string::npos);
}

TEST_CASE("library link: a function the archive does not define is an undefined symbol") {
  const std::filesystem::path dir = testScratchPath("library_link_undefined");
  const std::string ui = buildLibrary(dir, "ui", UiSource);
  primec::LibraryArchiveExpectations expected;
  std::string idError;
  REQUIRE_MESSAGE(
      primec::executableBuildId(
          "./primec", primec::BuildIdSource::LinkerIdFirst, expected.compilerBuildId, idError),
      idError);
  primec::LibraryArchiveContent content;
  std::string error;
  REQUIRE_MESSAGE(primec::readLibraryArchiveFile(ui, expected, content, error), error);
  for (primec::IrFunction &function : content.code.functions) {
    if (function.name == "/ui/add") {
      function.name = "/ui/add_renamed";
    }
  }
  REQUIRE_MESSAGE(primec::writeLibraryArchiveFile(ui, content, error), error);
  writeIn(dir,
          "app.prime",
          "import</ui>\nimport /ui/*\n\n[return<i32>]\nmain() {\n  return(add(1i32, 2i32))\n}\n");
  const Run run = runApp(dir, "vm", "--library " + quoteShellArg(ui));
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("undefined symbol /ui/add (library ui)") != std::string::npos);
}

TEST_CASE("library link: a library compiled against a library links when both are supplied") {
  const std::filesystem::path dir = testScratchPath("library_link_chain");
  const std::string base =
      buildLibrary(dir, "base", "[public return<i32>]\nbase_value() {\n  return(7i32)\n}\n");
  const std::string mid = buildLibrary(dir,
                                       "mid",
                                       "import</base_value>\n\n[public return<i32>]\nmid_value() "
                                       "{\n  return(base_value() + 1i32)\n}\n",
                                       "--library " + quoteShellArg(base));
  primec::LibraryArchiveExpectations expected;
  std::string idError;
  REQUIRE_MESSAGE(
      primec::executableBuildId(
          "./primec", primec::BuildIdSource::LinkerIdFirst, expected.compilerBuildId, idError),
      idError);
  primec::LibraryArchiveContent content;
  std::string error;
  REQUIRE_MESSAGE(primec::readLibraryArchiveFile(mid, expected, content, error), error);
  REQUIRE(content.manifest.libraries.size() == 1);
  CHECK(content.manifest.libraries[0].name == "base");
  // The code of `mid` keeps `base_value` as an import.
  bool sawImport = false;
  for (const primec::IrFunction &function : content.code.functions) {
    sawImport = sawImport || (function.isImport && function.importLibrary == "base");
  }
  CHECK(sawImport);

  writeIn(dir,
          "app.prime",
          "import</mid_value>\n\n[return<i32>]\nmain() {\n  return(mid_value())\n}\n");
  const Run both =
      runApp(dir, "vm", "--library " + quoteShellArg(mid) + " --library " + quoteShellArg(base));
  CHECK_MESSAGE(both.err.empty(), both.err);
  CHECK(both.exitCode == 8);
  const Run missing = runApp(dir, "vm", "--library " + quoteShellArg(mid));
  CHECK(missing.exitCode == 2);
  CHECK(missing.err.find("library mid needs library base") != std::string::npos);
}

TEST_CASE("library link: a string result and a Pointer result are results of the archive") {
  // A `string` result is an index into the string table of the module that returned it, which the link relocates
  // with the library's literals; a `Pointer<i64>` result is an address of the heap the program shares.
  const std::string source = R"(
[namespace summary("One slot results.")]
slots {
  [public return<string>]
  pick([i32] n) {
    if(n > 1i32) {
      return("many"utf8)
    }
    return("one"utf8)
  }

  [public unsafe effects(heap_alloc) return<Pointer<i64>>]
  make_cell([i64] x) {
    [Pointer<i64> mut] cell{/std/intrinsics/memory/alloc<i64>(2i32)}
    assign(*cell, x)
    return(cell)
  }

  [public unsafe effects(heap_alloc) return<void>]
  free_cell([Pointer<i64>] cell) {
    /std/intrinsics/memory/free(cell)
  }
}
)";
  const std::string body = R"(
[unsafe return<i32> effects(io_out, heap_alloc)]
main() {
  print_line(pick(1i32))
  print_line("between"utf8)
  print_line(pick(2i32))
  [Pointer<i64>] first{make_cell(40i64)}
  [Pointer<i64>] second{make_cell(2i64)}
  print_line(*first + *second)
  free_cell(first)
  free_cell(second)
  return(0i32)
}
)";
  for (const std::string emit : {"vm", "optexe"}) {
    INFO(emit);
    const std::filesystem::path dir = testScratchPath("library_link_slot_results_" + emit);
    writeIn(dir, "slots.prime", source);
    writeIn(dir, "app.prime", "import<\"slots.prime\">\nimport /slots/*\n" + body);
    const Run whole = runApp(dir, emit, "");
    REQUIRE_MESSAGE(whole.exitCode == 0, whole.err);
    CHECK(whole.out == "one\nbetween\nmany\n42\n");

    const std::string archive = buildLibrary(dir, "slots", source);
    writeIn(dir, "app.prime", "import</slots>\nimport /slots/*\n" + body);
    const Run linked = runApp(dir, emit, "--library " + quoteShellArg(archive));
    CHECK_MESSAGE(linked.err.empty(), linked.err);
    CHECK(linked.exitCode == whole.exitCode);
    CHECK(linked.out == whole.out);
  }
}

TEST_SUITE_END();
