// `primec --emit=lib` (docs/spec/integration-and-tooling.md, Library Archive): the archive of a module with every
// public function as a root, deterministic across directories, and read back by the reader.
#include "test_compile_run_text_filters_helpers.h"

#include "../../runtime/ir_link_test_support.h"

#include "primec/ir/IrLinker.h"
#include "primec/ir/IrValidation.h"
#include "primec/pipeline/LibraryArchive.h"
#include "primec/support/CompilerSourceId.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

// A module with a struct, a function nothing calls (`lonely`), a function that calls a private helper twice, a public
// generic and a stdlib import, and no `main`.
const std::string LibSource = R"(import<"parts/shapes.prime">

[namespace summary("Drawing.")]
ui {
  [public struct summary("A point on the grid.")]
  Point() {
    [public i32] x{1i32}
    [public i32] y{2i32}
    [public return<i32>]
    sum([Point] self) { return(self.x + self.y) }
  }

  [public effects(io_out) return<i32> summary("Adds two numbers.")]
  add([i32] a, [i32] b{2i32}) {
    return(a + b)
  }

  [public return<i32>]
  lonely() {
    return(helper() + helper())
  }

  [return<i32>]
  helper() {
    return(41i32)
  }

  [public return<T>]
  identity<T>([T] value) {
    return(value)
  }
}
)";

const std::string ShapesSource = R"(import /std/collections/vector

[public return<i32>]
corners() {
  return(4i32)
}

[public effects(heap_alloc) return<i32>]
count_items() {
  [vector<i32> mut] items{vector<i32>{}}
  items.push(3i32)
  return(items.count())
}
)";

struct LibRun {
  int exitCode = 0;
  std::string out;
  std::string err;
  std::string archivePath;
};

void writeIn(const std::filesystem::path &dir,
             const std::string &relative,
             const std::string &text) {
  const std::filesystem::path path = dir / relative;
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  REQUIRE(file.good());
  file << text;
}

// Writes the module into `dir` (`root.prime` and `parts/shapes.prime`) and builds its archive.
LibRun buildLib(const std::filesystem::path &dir, const std::string &extraArgs = "") {
  writeIn(dir, "ui.prime", LibSource);
  writeIn(dir, "parts/shapes.prime", ShapesSource);
  LibRun run;
  run.archivePath = (dir / "out" / "ui.psa").string();
  std::filesystem::create_directories(dir / "out");
  const std::string errPath = (dir / "err.txt").string();
  run.exitCode = runCommand("./primec --emit=lib " + quoteShellArg((dir / "ui.prime").string()) +
                            " -o " + quoteShellArg(run.archivePath) + " " + extraArgs + " 2> " +
                            quoteShellArg(errPath));
  run.err = readFile(errPath);
  return run;
}

std::string primecOutput(const std::filesystem::path &dir, const std::string &kind) {
  const std::string outPath = (dir / ("kind_" + kind + ".txt")).string();
  REQUIRE(runCommand("./primec --emit=" + kind + " " + quoteShellArg((dir / "ui.prime").string()) +
                     " > " + quoteShellArg(outPath)) == 0);
  return readFile(outPath);
}

std::string compilerBuildId() {
  return primec::compilerSourceBuildId();
}

primec::LibraryArchiveContent readArchive(const std::string &path) {
  primec::LibraryArchiveExpectations expected;
  expected.compilerBuildId = compilerBuildId();
  primec::LibraryArchiveContent content;
  std::string error;
  REQUIRE_MESSAGE(primec::readLibraryArchiveFile(path, expected, content, error), error);
  return content;
}

std::string bytesOf(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

int32_t functionIndexOf(const primec::IrModule &module, const std::string &name) {
  for (size_t i = 0; i < module.functions.size(); ++i) {
    if (module.functions[i].name == name) {
      return static_cast<int32_t>(i);
    }
  }
  return -1;
}

} // namespace

TEST_CASE("emit-lib holds every public function, one nothing calls included") {
  const std::filesystem::path dir = testScratchPath("emit_lib_all");
  const LibRun run = buildLib(dir);
  REQUIRE_MESSAGE(run.exitCode == 0, run.err);
  const primec::LibraryArchiveContent content = readArchive(run.archivePath);

  CHECK(content.manifest.name == "ui");
  CHECK(content.manifest.roots == std::vector<std::string>{"/corners", "/count_items", "/ui"});
  CHECK(content.manifest.compilerBuildId == compilerBuildId());
  CHECK(content.manifest.irSchemaVersion == primec::IrSchemaVersion);
  CHECK(content.manifest.libraries.empty());
  CHECK_FALSE(content.hasSources);
  // The interface and the symbol table are what `--emit=interface` and `--emit=symbols` print.
  CHECK(content.interfaceText == primecOutput(dir, "interface"));
  CHECK(content.symbolsText == primecOutput(dir, "symbols"));
  CHECK(content.symbolsText.find("\"interfaceHash\":\"" + content.manifest.interfaceHash + "\"") !=
        std::string::npos);

  const primec::IrModule &code = content.code;
  CHECK(code.entryIndex == -1);
  CHECK(code.schemaVersion == primec::IrSchemaVersion);
  for (const char *name : {"/ui/add", "/ui/lonely", "/ui/Point/sum", "/corners", "/count_items"}) {
    CAPTURE(name);
    CHECK(functionIndexOf(code, name) >= 0);
  }
  // `lonely` is called by nobody and is in the code; the entry stub and a generic definition are not.
  CHECK(functionIndexOf(code, "/main") == -1);
  CHECK(functionIndexOf(code, "/ui/identity") == -1);
  // The private helper that `lonely` calls twice is a function of the library, called through its index.
  CHECK(functionIndexOf(code, "/ui/helper") >= 0);
  for (const primec::IrFunction &function : code.functions) {
    CHECK_FALSE(function.isImport);
  }
}

TEST_CASE(
    "emit-lib code runs after the IR link: a library function and a function that calls a helper") {
  const std::filesystem::path dir = testScratchPath("emit_lib_run");
  const LibRun build = buildLib(dir);
  REQUIRE_MESSAGE(build.exitCode == 0, build.err);
  const primec::LibraryArchiveContent content = readArchive(build.archivePath);

  using namespace ir_link_test;
  // main: `/ui/add(40i32, 2i32)` and `/ui/lonely()` as imports the library defines.
  auto runWith = [&](std::vector<IrInstruction> body) {
    IrModule consumer = moduleOf({functionOf("/main", 0, std::move(body)),
                                  importOf("/ui/add", 2, "ui"),
                                  importOf("/ui/lonely", 0, "ui")},
                                 0);
    std::string error;
    REQUIRE_MESSAGE(primec::linkIrModule(consumer, content.code, error), error);
    REQUIRE_MESSAGE(primec::checkIrImportsResolved(consumer, error), error);
    REQUIRE_MESSAGE(primec::validateIrModule(consumer, primec::IrValidationTarget::Vm, error),
                    error);
    return run(consumer);
  };
  const Run add = runWith({{IrOpcode::PushI32, 40},
                           {IrOpcode::PushI32, 2},
                           {IrOpcode::Call, 1},
                           {IrOpcode::ReturnI32, 0}});
  CHECK(add.ok);
  CHECK(add.value == 42);
  const Run lonely = runWith({{IrOpcode::Call, 2}, {IrOpcode::ReturnI32, 0}});
  CHECK(lonely.ok);
  CHECK(lonely.value == 82);
}

TEST_CASE("emit-lib is byte-identical twice and from two directories") {
  const std::filesystem::path first = testScratchPath("emit_lib_same_a");
  const std::filesystem::path second = testScratchPath("emit_lib_same/nested/deeper/checkout_b");
  const LibRun a = buildLib(first, "--with-sources");
  const LibRun b = buildLib(second, "--with-sources");
  REQUIRE_MESSAGE(a.exitCode == 0, a.err);
  REQUIRE_MESSAGE(b.exitCode == 0, b.err);
  const std::string bytes = bytesOf(a.archivePath);
  CHECK(bytes.size() > 500);
  CHECK(bytes == bytesOf(b.archivePath));
  // Twice from one directory.
  const LibRun again = buildLib(first, "--with-sources");
  REQUIRE(again.exitCode == 0);
  CHECK(bytes == bytesOf(again.archivePath));
  // No path of either checkout is in the file.
  CHECK(bytes.find(first.string()) == std::string::npos);
  CHECK(bytes.find(second.string()) == std::string::npos);
  CHECK(bytes.find(testScratchPath("").string()) == std::string::npos);
}

TEST_CASE(
    "emit-lib stores the sources only with --with-sources and names files from the root file") {
  const std::filesystem::path dir = testScratchPath("emit_lib_sources");
  const LibRun without = buildLib(dir);
  REQUIRE(without.exitCode == 0);
  const std::string withoutBytes = bytesOf(without.archivePath);
  CHECK_FALSE(readArchive(without.archivePath).hasSources);

  const LibRun with = buildLib(dir, "--with-sources");
  REQUIRE(with.exitCode == 0);
  const primec::LibraryArchiveContent content = readArchive(with.archivePath);
  REQUIRE(content.hasSources);
  REQUIRE(content.sources.size() == 2);
  CHECK(content.sources[0].path == "parts/shapes.prime");
  CHECK(content.sources[0].text == ShapesSource);
  CHECK(content.sources[1].path == "ui.prime");
  CHECK(content.sources[1].text == LibSource);
  CHECK(bytesOf(with.archivePath).size() > withoutBytes.size());

  // The source map names the files relative to the root file's directory.
  bool sawRoot = false;
  bool sawImport = false;
  for (const primec::IrInstructionSourceMapEntry &entry : content.code.instructionSourceMap) {
    CHECK(entry.sourceUnit.rfind('/', 0) != 0);
    sawRoot = sawRoot || entry.sourceUnit == "ui.prime";
    sawImport = sawImport || entry.sourceUnit == "parts/shapes.prime";
  }
  CHECK(sawRoot);
  CHECK(sawImport);
}

TEST_CASE("emit-lib names its default output, refuses a function the ABI cannot take and a module "
          "that fails") {
  const std::filesystem::path dir = testScratchPath("emit_lib_errors");
  writeIn(dir, "ok.prime", "[public return<i32>]\none() {\n  return(1i32)\n}\n");
  const std::string errPath = (dir / "err.txt").string();

  // Without -o the archive is `<name>.psa` in the output directory.
  CHECK(runCommand("./primec --emit=lib " + quoteShellArg((dir / "ok.prime").string()) +
                   " --out-dir " + quoteShellArg((dir / "default_out").string()) + " 2> " +
                   quoteShellArg(errPath)) == 0);
  CHECK(std::filesystem::exists(dir / "default_out" / "ok.psa"));

  writeIn(
      dir, "pack.prime", "[public return<i32>]\npacked([args<i32>] values) {\n  return(1i32)\n}\n");
  CHECK(runCommand("./primec --emit=lib " + quoteShellArg((dir / "pack.prime").string()) + " -o " +
                   quoteShellArg((dir / "pack.psa").string()) + " 2> " + quoteShellArg(errPath)) ==
        2);
  CHECK(readFile(errPath).find(
            "public function /packed cannot be compiled into a library: parameter `values` is an "
            "argument pack") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(dir / "pack.psa"));

  writeIn(dir, "bad.prime", "[public return<i32>]\nbroken() {\n  return(missing_name)\n}\n");
  CHECK(runCommand("./primec --emit=lib " + quoteShellArg((dir / "bad.prime").string()) + " -o " +
                   quoteShellArg((dir / "bad.psa").string()) + " 2> " + quoteShellArg(errPath)) ==
        2);
  CHECK_FALSE(std::filesystem::exists(dir / "bad.psa"));
}

TEST_CASE("emit-lib names the result shape a refused public function has") {
  const std::filesystem::path dir = testScratchPath("emit_lib_result_shapes");
  const std::string errPath = (dir / "err.txt").string();
  const std::vector<std::tuple<std::string, std::string, std::string>> refused = {
      {"reference",
       "[public return<Reference<i32>>]\nview([Reference<i32>] value) {\n  return(value)\n}\n",
       "public function /view cannot be compiled into a library: its result `Reference<i32>` is "
       "not "
       "one a real call returns: a scalar, `void`, `never`, a `string`, a `Pointer`, a struct "
       "and, for a function of a library, a sum, a packed `Result` and a `vector<T>` are; a "
       "`Reference` is not"},
      {"vector",
       "import /std/collections/vector\n\n[public effects(heap_alloc) return<vector<i32>>]\nmake() "
       "{\n"
       "  return(vector<i32>(1i32))\n}\n",
       "public function /make cannot be compiled into a library: its result `vector<i32>` is not "
       "always a `mut` vector local the function built"},
      // A borrowed sum parameter stays inline in every program, so a library root cannot take it either.
      {"sum_parameter",
       "[sum] Pick { [i32] number [bool] flag }\n\n[public return<i32>]\nuse_pick([Pick] value) {\n"
       "  return(1i32)\n}\n",
       "public function /use_pick cannot be compiled into a library: parameter `value` is a sum, "
       "which a real call does not take: a function with a sum parameter stays inline"},
      {"sum_reference_parameter",
       "[sum] Pick { [i32] number [bool] flag }\n\n[public return<i32>]\n"
       "use_ref([Reference<Pick>] value) {\n  return(2i32)\n}\n",
       "public function /use_ref cannot be compiled into a library: parameter `value` is a sum, "
       "which a real call does not take"},
  };
  for (const auto &[name, source, message] : refused) {
    INFO(name);
    writeIn(dir, name + ".prime", source);
    CHECK(runCommand("./primec --emit=lib " + quoteShellArg((dir / (name + ".prime")).string()) +
                     " -o " + quoteShellArg((dir / (name + ".psa")).string()) + " 2> " +
                     quoteShellArg(errPath)) == 2);
    CHECK(readFile(errPath).find(message) != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(dir / (name + ".psa")));
  }
  // The same function in a whole-program build is inlined and runs.
  writeIn(dir,
          "whole.prime",
          "[sum] Pick { [i32] number [bool] flag }\n\n[return<i32>]\nuse_pick([Pick] value) {\n"
          "  return(41i32)\n}\n\n[return<i32>]\nmain() {\n  return(use_pick(Pick{[number] 1i32}) + "
          "1i32)\n}\n");
  CHECK(runCommand("./primec --emit=vm " + quoteShellArg((dir / "whole.prime").string()) +
                   " --entry /main") == 42);

  // The functions the refusal names still run in a whole program, on the VM and on optexe.
  const std::vector<std::pair<std::string, std::string>> whole = {
      {"sum_result",
       "[sum] Pick { [i32] number [bool] flag }\n\n[return<Pick>]\nmake_pick() {\n"
       "  return(Pick{[number] 41i32})\n}\n\n[return<i32>]\nmain() {\n"
       "  [Pick] picked{make_pick()}\n"
       "  return(pick(picked) {\n    number(value) {\n      plus(value, 1i32)\n    }\n"
       "    flag(value) {\n      0i32\n    }\n  })\n}\n"},
      {"packed_result",
       "[return<Result<i32, i32>>]\nmake_result() {\n  return(Result.ok(42i32))\n}\n\n"
       "[return<Result<i32, i32>>]\nmain() {\n  return(make_result())\n}\n"},
      {"error_struct_result",
       "import /std/file/*\n\n[return<FileError>]\nmake_eof() {\n  return(FileError.eof())\n}\n\n"
       "[return<i32>]\nmain() {\n  [FileError] err{make_eof()}\n"
       "  if(FileError.isEof(err), then() { return(42i32) }, else() { return(1i32) })\n"
       "  return(2i32)\n}\n"},
  };
  for (const auto &[name, source] : whole) {
    INFO(name);
    writeIn(dir, "whole_" + name + ".prime", source);
    const std::string wholePath = quoteShellArg((dir / ("whole_" + name + ".prime")).string());
    CHECK(runCommand("./primec --emit=vm " + wholePath + " --entry /main") == 42);
    const std::string exePath = (dir / ("whole_" + name + ".exe")).string();
    REQUIRE(runCommand("./primec --emit=optexe " + wholePath + " -o " + quoteShellArg(exePath) +
                       " --entry /main") == 0);
    CHECK(runCommand(quoteShellArg(exePath)) == 42);
  }
}

TEST_CASE("emit-lib: a module with no public function has an empty code module, and a changed file "
          "is refused") {
  const std::filesystem::path dir = testScratchPath("emit_lib_empty");
  writeIn(dir, "quiet.prime", "[return<i32>]\nprivate_only() {\n  return(1i32)\n}\n");
  const std::string archive = (dir / "quiet.psa").string();
  REQUIRE(runCommand("./primec --emit=lib " + quoteShellArg((dir / "quiet.prime").string()) +
                     " -o " + quoteShellArg(archive)) == 0);
  const primec::LibraryArchiveContent content = readArchive(archive);
  CHECK(content.manifest.name == "quiet");
  CHECK(content.manifest.roots.empty());
  CHECK(content.code.functions.empty());
  CHECK(content.code.entryIndex == -1);

  // One flipped byte of the symbol table: the reader names the section and says to rebuild.
  std::string bytes = bytesOf(archive);
  const size_t at = bytes.find("\"symbols\"");
  REQUIRE(at != std::string::npos);
  bytes[at] = '#';
  const std::string damaged = (dir / "damaged.psa").string();
  {
    std::ofstream out(damaged, std::ios::binary);
    out << bytes;
  }
  primec::LibraryArchiveExpectations expected;
  expected.compilerBuildId = compilerBuildId();
  primec::LibraryArchiveContent unused;
  std::string error;
  CHECK_FALSE(primec::readLibraryArchiveFile(damaged, expected, unused, error));
  CHECK(error.find("library section `symbols` changed") != std::string::npos);
  CHECK(error.find("rebuild the library") != std::string::npos);

  // Another compiler build.
  expected.compilerBuildId = std::string(64, '0');
  CHECK_FALSE(primec::readLibraryArchiveFile(archive, expected, unused, error));
  CHECK(error.find("another compiler build") != std::string::npos);
}
