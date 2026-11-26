// Stdlib modules as libraries (docs/ModulesAndLibrariesPlan.md, sections 4 and 12): a library built from a
// stdlib file owns roots below `/std`, so several of them link, and a program that links one does not read the stdlib
// source of the module it stands for.
#include "test_compile_run_text_filters_helpers.h"

#include "primec/pipeline/LibraryArchive.h"
#include "primec/support/CompilerSourceId.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

// Uses one function of color and one of vector, through the whole `/std/math` import.
const std::string MathApp = R"(@IMPORT@
import /std/math/*

[return<i32>]
main() {
  [f32] x{linearToSrgbChannel(0.5f32)}
  [Vec2] v{Vec2{3.0f32, 4.0f32}}
  return(convert<i32>(x * 100.0f32 + v.length()))
}
)";

// Uses one function of path, whose module imports the stdlib text modules.
const std::string PathApp = R"(@IMPORT@
import /std/path/*
import /std/text/*
import /std/text/view/*

[return<i32>]
main() {
  [i32 mut] r{0i32}
  if(/std/path/is_absolute("/usr"utf8)) { r = r + 1i32 }
  if(/std/path/is_absolute("usr"utf8)) { r = r + 10i32 }
  return(r)
}
)";

struct Run {
  int exitCode = 0;
  std::string out;
  std::string err;
};

void writeIn(const std::filesystem::path &dir, const std::string &name, const std::string &text) {
  std::filesystem::create_directories((dir / name).parent_path());
  std::ofstream file(dir / name, std::ios::binary);
  REQUIRE(file.good());
  file << text;
}

Run shell(const std::filesystem::path &dir, const std::string &command) {
  Run run;
  run.exitCode = runCommand(command + " > " + quoteShellArg((dir / "o.txt").string()) + " 2> " +
                            quoteShellArg((dir / "e.txt").string()));
  run.out = readFile((dir / "o.txt").string());
  run.err = readFile((dir / "e.txt").string());
  return run;
}

std::string withImport(const std::string &app, const std::string &import) {
  const std::string marker = "@IMPORT@";
  std::string text = app;
  text.replace(text.find(marker), marker.size(), import);
  return text;
}

// Compiles and runs `app.prime` of `dir` on the VM, with `extra` arguments, and writes the source files it read to
// `deps.txt`.
Run runApp(const std::filesystem::path &dir, const std::string &extra) {
  return shell(dir,
               "./primec --emit=vm --emit-deps " + quoteShellArg((dir / "deps.txt").string()) +
                   " " + extra + " " + quoteShellArg((dir / "app.prime").string()) +
                   " --entry /main");
}

// The path of the stdlib file `std/<relative>` this build reads, found by asking a compile that imports it.
std::string stdlibFile(const std::filesystem::path &dir,
                       const std::string &import,
                       const std::string &relative) {
  // The math wildcard import is skipped when the program names no math symbol, so the probe names two.
  const std::string body = import.rfind("/std/math", 0) == 0
                               ? "  [f32] x{linearToSrgbChannel(0.5f32)}\n  [Vec2] v{Vec2{}}\n"
                               : "";
  writeIn(dir,
          "probe.prime",
          "import " + import + "\n[return<i32>]\nmain() {\n" + body + "  return(0i32)\n}\n");
  const Run run =
      shell(dir,
            "./primec --emit=vm --emit-deps " + quoteShellArg((dir / "probe_deps.txt").string()) +
                " " + quoteShellArg((dir / "probe.prime").string()) + " --entry /main");
  REQUIRE_MESSAGE(run.exitCode == 0, run.err);
  std::istringstream deps(readFile((dir / "probe_deps.txt").string()));
  std::string line;
  const std::string tail = "/stdlib/std/" + relative;
  while (std::getline(deps, line)) {
    if (line.size() >= tail.size() &&
        line.compare(line.size() - tail.size(), tail.size(), tail) == 0) {
      return line;
    }
  }
  FAIL("the stdlib file std/" << relative << " was not read");
  return {};
}

std::string
buildLibrary(const std::filesystem::path &dir, const std::string &source, const std::string &name) {
  const std::string archive = (dir / (name + ".psa")).string();
  const Run built =
      shell(dir, "./primec --emit=lib " + quoteShellArg(source) + " -o " + quoteShellArg(archive));
  REQUIRE_MESSAGE(built.exitCode == 0, built.err);
  return archive;
}

bool depsRead(const std::filesystem::path &dir, const std::string &fileName) {
  const std::string deps = readFile((dir / "deps.txt").string());
  return deps.find("/" + fileName + "\n") != std::string::npos;
}

primec::LibraryArchiveContent readArchive(const std::string &path) {
  primec::LibraryArchiveExpectations expected;
  expected.compilerBuildId = primec::compilerSourceBuildId();
  primec::LibraryArchiveContent content;
  std::string error;
  REQUIRE_MESSAGE(primec::readLibraryArchiveFile(path, expected, content, error), error);
  return content;
}

} // namespace

TEST_CASE("stdlib libraries for color and vector link together and match the whole build") {
  const std::filesystem::path dir = testScratchPath("library_stdlib_color_vector");
  const std::string color =
      buildLibrary(dir, stdlibFile(dir, "/std/math/*", "math/color.prime"), "color");
  const std::string vector =
      buildLibrary(dir, stdlibFile(dir, "/std/math/*", "math/vector.prime"), "vector");

  writeIn(dir, "app.prime", withImport(MathApp, ""));
  const Run whole = runApp(dir, "");
  REQUIRE_MESSAGE(whole.err.empty(), whole.err);
  REQUIRE(whole.exitCode == 78);

  writeIn(dir, "app.prime", withImport(MathApp, "import</std/math>"));
  const Run both =
      runApp(dir, "--library " + quoteShellArg(color) + " --library " + quoteShellArg(vector));
  CHECK_MESSAGE(both.err.empty(), both.err);
  CHECK(both.exitCode == whole.exitCode);
  CHECK(both.out == whole.out);
  const Run onlyColor = runApp(dir, "--library " + quoteShellArg(color));
  CHECK_MESSAGE(onlyColor.err.empty(), onlyColor.err);
  CHECK(onlyColor.exitCode == whole.exitCode);
}

TEST_CASE("a stdlib library owns roots below std and its stdlib module") {
  const std::filesystem::path dir = testScratchPath("library_stdlib_manifest");
  const primec::LibraryArchiveContent content =
      readArchive(buildLibrary(dir, stdlibFile(dir, "/std/math/*", "math/color.prime"), "color"));
  REQUIRE(!content.manifest.roots.empty());
  for (const std::string &root : content.manifest.roots) {
    CHECK(root.rfind("/std/math/", 0) == 0);
    CHECK(root != "/std/math");
  }
  REQUIRE(content.manifest.stdlibModules.size() == 1);
  CHECK(content.manifest.stdlibModules.front().file == "std/math/color.prime");
  CHECK(content.manifest.stdlibModules.front().imports.empty());
}

TEST_CASE("the stdlib source of a module a linked library owns is not read") {
  const std::filesystem::path dir = testScratchPath("library_stdlib_not_read");
  const std::string color =
      buildLibrary(dir, stdlibFile(dir, "/std/math/*", "math/color.prime"), "color");

  writeIn(dir, "app.prime", withImport(MathApp, ""));
  const Run whole = runApp(dir, "");
  REQUIRE_MESSAGE(whole.err.empty(), whole.err);
  CHECK(depsRead(dir, "stdlib/std/math/color.prime"));
  CHECK(depsRead(dir, "stdlib/std/math/vector.prime"));

  writeIn(dir, "app.prime", withImport(MathApp, "import</std/math>"));
  const Run linked = runApp(dir, "--library " + quoteShellArg(color));
  REQUIRE_MESSAGE(linked.err.empty(), linked.err);
  CHECK(linked.exitCode == whole.exitCode);
  CHECK_FALSE(depsRead(dir, "stdlib/std/math/color.prime"));
  CHECK(depsRead(dir, "stdlib/std/math/vector.prime"));
  CHECK(depsRead(dir, "stdlib/std/math/matrix.prime"));

  // The same without `import</std/math>`: the stdlib pass splices the interface of the library it skips for.
  writeIn(dir, "app.prime", withImport(MathApp, ""));
  const Run plain = runApp(dir, "--library " + quoteShellArg(color));
  CHECK_MESSAGE(plain.err.empty(), plain.err);
  CHECK(plain.exitCode == whole.exitCode);
  CHECK_FALSE(depsRead(dir, "stdlib/std/math/color.prime"));
}

TEST_CASE("libraries of one stdlib file are refused with both names") {
  const std::filesystem::path dir = testScratchPath("library_stdlib_overlap");
  const std::string source = stdlibFile(dir, "/std/math/*", "math/color.prime");
  const std::string color = buildLibrary(dir, source, "color");
  writeIn(dir, "color_copy.prime", readFile(source));
  const std::string copy = buildLibrary(dir, (dir / "color_copy.prime").string(), "color_copy");

  writeIn(dir, "app.prime", withImport(MathApp, "import</std/math>"));
  const Run run =
      runApp(dir, "--library " + quoteShellArg(color) + " --library " + quoteShellArg(copy));
  CHECK(run.exitCode != 0);
  CHECK(run.err.find("library color and library color_copy both own /std/math/ColorRGB") !=
        std::string::npos);
}

TEST_CASE("a stdlib library keeps the stdlib imports of its module") {
  const std::filesystem::path dir = testScratchPath("library_stdlib_path");
  const std::string path =
      buildLibrary(dir, stdlibFile(dir, "/std/path/*", "path/path.prime"), "path");
  const primec::LibraryArchiveContent content = readArchive(path);
  REQUIRE(content.manifest.stdlibModules.size() == 1);
  CHECK(!content.manifest.stdlibModules.front().imports.empty());

  writeIn(dir, "app.prime", withImport(PathApp, ""));
  const Run whole = runApp(dir, "");
  REQUIRE_MESSAGE(whole.err.empty(), whole.err);
  CHECK(depsRead(dir, "stdlib/std/path/path.prime"));

  writeIn(dir, "app.prime", withImport(PathApp, "import</std/path>"));
  const Run linked = runApp(dir, "--library " + quoteShellArg(path));
  CHECK_MESSAGE(linked.err.empty(), linked.err);
  CHECK(linked.exitCode == whole.exitCode);
  CHECK(linked.out == whole.out);
  CHECK_FALSE(depsRead(dir, "stdlib/std/path/path.prime"));
  CHECK(depsRead(dir, "stdlib/std/text/view.prime"));

  writeIn(dir, "app.prime", withImport(PathApp, ""));
  const Run plain = runApp(dir, "--library " + quoteShellArg(path));
  CHECK_MESSAGE(plain.err.empty(), plain.err);
  CHECK(plain.exitCode == whole.exitCode);
  CHECK_FALSE(depsRead(dir, "stdlib/std/path/path.prime"));
}

TEST_CASE("a stdlib file that defines its names at the root is an importable module") {
  const std::filesystem::path dir = testScratchPath("library_stdlib_equality_import");
  writeIn(dir,
          "app.prime",
          "import /std/collections/equality/*\n[return<i32>]\nmain() {\n"
          "  if(7i32.equal(7i32)) { return(42i32) }\n  return(1i32)\n}\n");
  const Run run = runApp(dir, "");
  CHECK_MESSAGE(run.err.empty(), run.err);
  CHECK(run.exitCode == 42);

  writeIn(dir,
          "typo.prime",
          "import /std/collections/nosuch/*\n[return<i32>]\nmain() { return(0i32) }\n");
  const Run typo = shell(
      dir, "./primec --emit=vm " + quoteShellArg((dir / "typo.prime").string()) + " --entry /main");
  CHECK(typo.exitCode != 0);
  CHECK(typo.err.find("unknown import path: /std/collections/nosuch/*") != std::string::npos);
}

TEST_CASE("the map module builds as a stdlib library") {
  const std::filesystem::path dir = testScratchPath("library_stdlib_map");
  const std::string path =
      buildLibrary(dir, stdlibFile(dir, "/std/collections/map/*", "collections/map.prime"), "map");
  const primec::LibraryArchiveContent content = readArchive(path);
  REQUIRE(content.manifest.stdlibModules.size() == 1);
  CHECK(content.manifest.stdlibModules.front().file == "std/collections/map.prime");
}

TEST_CASE("a string literal converts to a StringView for a local of a struct of the namespace") {
  const std::filesystem::path dir = testScratchPath("library_stdlib_local_literal");
  writeIn(
      dir,
      "app.prime",
      "import /std/text/*\nimport /std/text/view/*\n"
      "namespace app {\n"
      "  [public struct]\n  Tool() {\n    [public i32 mut] n{0i32}\n"
      "    [mut]\n    emit([StringView] kind) { this.n = kind.count() }\n  }\n"
      "  [public struct]\n  Runner() {\n    [public i32 mut] m{0i32}\n"
      "    [mut]\n    run([Tool copy] tool) {\n"
      "      [Tool mut] settings{tool}\n      settings.emit(\"lib\")\n"
      "      this.m = settings.n\n    }\n  }\n}\n"
      "import /app/*\n"
      "[return<i32>]\nmain() {\n  [Runner mut] r{Runner{}}\n  r.run(Tool{})\n  return(r.m)\n}\n");
  const Run run = runApp(dir, "");
  CHECK_MESSAGE(run.err.empty(), run.err);
  CHECK(run.exitCode == 3);
}

TEST_CASE("the build and text view modules are not refused for a missing name as roots") {
  const std::filesystem::path dir = testScratchPath("library_stdlib_build_view");
  buildLibrary(dir, stdlibFile(dir, "/std/build/*", "build/build.prime"), "build");
  const Run view = shell(dir,
                         "./primec --emit=lib " +
                             quoteShellArg(stdlibFile(dir, "/std/text/view/*", "text/view.prime")) +
                             " -o " + quoteShellArg((dir / "view.psa").string()));
  CHECK(view.exitCode != 0);
  CHECK(view.err.find("parameter `parts` is an argument pack") != std::string::npos);
  CHECK(view.err.find("unsupported binding type") == std::string::npos);
}

TEST_CASE("stdlib files that need the other files of their module are roots of libraries") {
  const std::filesystem::path dir = testScratchPath("library_stdlib_module_roots");
  const std::string color = stdlibFile(dir, "/std/math/*", "math/color.prime");
  const std::string stdDir = color.substr(0, color.size() - std::string("math/color.prime").size());

  // The files whose module has other files build.
  for (const char *file : {"math/matrix.prime", "math/quaternion.prime"}) {
    const std::string name = std::filesystem::path(file).stem().string();
    const primec::LibraryArchiveContent content =
        readArchive(buildLibrary(dir, stdDir + file, name));
    REQUIRE(content.manifest.stdlibModules.size() == 1);
    CHECK(content.manifest.stdlibModules.front().file == std::string("std/") + file);
  }

  // The files whose public functions take a `string` are refused by the shape rule, not for a name they cannot see.
  const std::pair<const char *, const char *> refused[] = {
      {"image/image.prime", "parameter `path` is a `string`"},
      {"web/web.prime", "parameter `target` is a `string`"},
      {"ui/native/native.prime", "parameter `message` is a `string`"},
  };
  for (const auto &[file, rule] : refused) {
    const Run run = shell(dir,
                          "./primec --emit=lib " + quoteShellArg(stdDir + file) + " -o " +
                              quoteShellArg((dir / "refused.psa").string()));
    CHECK(run.exitCode != 0);
    CHECK_MESSAGE(run.err.find("cannot be compiled into a library") != std::string::npos, run.err);
    CHECK_MESSAGE(run.err.find(rule) != std::string::npos, run.err);
    CHECK(run.err.find("unknown ") == std::string::npos);
    CHECK(run.err.find("unsupported binding type") == std::string::npos);
  }
}

TEST_SUITE_END();
