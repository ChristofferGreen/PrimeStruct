#include "embed_test_support.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/pipeline/CompilePipeline.h"
#include "primec/pipeline/LibraryArchive.h"
#include "primec/pipeline/LibraryBuild.h"
#include "primec/support/CompilerSourceId.h"
#include "primec/support/Options.h"

#include "third_party/doctest.h"

#include <string>
#include <variant>

using primec::embed::RunOptions;
using primec::embed::Script;
using primec::embed::ScriptEngine;

// `ScriptEngine::addLibrary`: a script of the embed API links a precompiled library in this process
// (docs/ModulesAndLibrariesPlan.md, section 14). Libraries come from `primec --emit=lib` and from the host's own
// compiler; the compiler build id is the id of the sources, which a host and `primec` of one tree share.
TEST_SUITE_BEGIN("primestruct.embed.libraries");

namespace {

const std::string LibrarySource = R"(
[namespace summary("A small library.")]
tools {
  [public return<i32>]
  add([i32] a, [i32] b) {
    return(a + b)
  }

  [public return<i32>]
  quad([i32] a) {
    return(twice(a) + twice(a))
  }

  [return<i32>]
  twice([i32] a) {
    return(add(a, a))
  }

  [public return<string>]
  greeting() {
    return("from the library"utf8)
  }
}
)";

const std::string ImportLines = "import</tools>\nimport /tools/*\n";

const std::string ScriptBody = R"(
[return<i32> effects(io_out)]
main() {
  print_line(greeting())
  print_line(quad(3i32))
  return(add(40i32, 2i32))
}
)";

struct Outcome {
  bool ok = false;
  int exitCode = 0;
  std::string out;
  std::string diagnostics;
};

Outcome runScript(const Script &script) {
  Outcome outcome;
  if (!script.valid()) {
    outcome.diagnostics = script.diagnostics();
    return outcome;
  }
  RunOptions options;
  options.output = [&](int, std::string_view chunk) { outcome.out.append(chunk); };
  const auto result = script.run({}, options);
  outcome.ok = result.ok;
  outcome.exitCode = result.exitCode;
  outcome.diagnostics = result.diagnostics;
  return outcome;
}

// Builds the archive of `LibrarySource` with the host's own compiler, the way `primec --emit=lib` does, recording
// `buildId` as the compiler that wrote it.
std::string buildWithHostCompiler(const std::filesystem::path &dir, const std::string &buildId) {
  const std::filesystem::path source = embedWriteFile(dir / "tools.prime", LibrarySource);
  primec::Options options;
  options.emitKind = "lib";
  options.inputPath = source.string();
  options.entryPath = "/main";
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);
  std::string error;
  primec::CompilePipelineErrorStage stage = primec::CompilePipelineErrorStage::None;
  primec::CompilePipelineDiagnosticInfo info;
  primec::CompilePipelineResult result = primec::runCompilePipelineResult(options, stage, error, &info);
  REQUIRE_MESSAGE(std::holds_alternative<primec::CompilePipelineSuccessResult>(result), error);
  primec::CompilePipelineOutput output = std::move(std::get<primec::CompilePipelineSuccessResult>(result).output);
  primec::LibraryArchiveContent content;
  REQUIRE_MESSAGE(primec::buildLibraryArchiveContent(output, options, buildId, content, error), error);
  const std::string archive = (dir / "tools.psa").string();
  REQUIRE_MESSAGE(primec::writeLibraryArchiveFile(archive, content, error), error);
  return archive;
}

} // namespace

TEST_CASE("a script calls a library the host's own compiler built") {
  const auto dir = embedTestDir("library_host_built");
  const std::string archive = buildWithHostCompiler(dir, primec::compilerSourceBuildId());
  ScriptEngine engine;
  engine.addLibrary(archive);
  const Outcome outcome = runScript(engine.compileSource("/library_host_built.prime", ImportLines + ScriptBody));
  CHECK_MESSAGE(outcome.ok, outcome.diagnostics);
  CHECK(outcome.exitCode == 42);
  CHECK(outcome.out == "from the library\n12\n");
}

#ifdef PRIMESTRUCT_TEST_PRIMEC_PATH
TEST_CASE("a script calls a library that primec built, and one compile serves many runs") {
  const auto dir = embedTestDir("library_primec_built");
  const std::string source = embedWriteFile(dir / "tools.prime", LibrarySource).string();
  const std::string archive = (dir / "tools.psa").string();
  const auto built = embedRunPrimec("--emit=lib \"" + source + "\" -o \"" + archive + "\"");
  REQUIRE_MESSAGE(built.exitCode == 0, built.output);

  ScriptEngine engine;
  engine.addLibrary(archive);
  const Script script = engine.compileSource("/library_primec_built.prime", ImportLines + ScriptBody);
  for (int run = 0; run < 2; ++run) {
    const Outcome outcome = runScript(script);
    CHECK_MESSAGE(outcome.ok, outcome.diagnostics);
    CHECK(outcome.exitCode == 42);
    CHECK(outcome.out == "from the library\n12\n");
  }
  // The same script the whole-program way (the library's source imported, no archive) gives the same result.
  ScriptEngine whole;
  const Outcome expected = runScript(whole.compileSource(
      (dir / "whole.prime").string(), "import<\"" + source + "\">\nimport /tools/*\n" + ScriptBody));
  CHECK_MESSAGE(expected.ok, expected.diagnostics);
  CHECK(expected.exitCode == 42);
  CHECK(expected.out == "from the library\n12\n");
}
#endif

TEST_CASE("a script that imports no library runs the same with a library added") {
  const auto dir = embedTestDir("library_unused");
  const std::string archive = buildWithHostCompiler(dir, primec::compilerSourceBuildId());
  const std::string plain = "[return<i32>]\nmain() {\n  return(7i32)\n}\n";
  ScriptEngine without;
  ScriptEngine with;
  with.addLibrary(archive);
  const Script first = without.compileSource("/library_unused.prime", plain);
  const Script second = with.compileSource("/library_unused.prime", plain);
  REQUIRE_MESSAGE(first.valid(), first.diagnostics());
  REQUIRE_MESSAGE(second.valid(), second.diagnostics());
  CHECK(first.run().exitCode == 7);
  CHECK(second.run().exitCode == 7);
}

TEST_CASE("a missing or damaged archive is a diagnostic of the script, not a crash") {
  const auto dir = embedTestDir("library_missing");
  ScriptEngine missing;
  missing.addLibrary((dir / "nothing.psa").string());
  const Script first = missing.compileSource("/library_missing.prime", ImportLines + ScriptBody);
  CHECK_FALSE(first.valid());
  CHECK(first.diagnostics().find("cannot use library") != std::string::npos);

  const std::string garbage = embedWriteFile(dir / "garbage.psa", "not an archive").string();
  ScriptEngine damaged;
  damaged.addLibrary(garbage);
  const Script second = damaged.compileSource("/library_damaged.prime", ImportLines + ScriptBody);
  CHECK_FALSE(second.valid());
  CHECK(second.diagnostics().find("cannot use library") != std::string::npos);
}

TEST_CASE("an archive another compiler build wrote is refused") {
  const auto dir = embedTestDir("library_other_build");
  const std::string archive = buildWithHostCompiler(dir, "src-" + std::string(64, '0'));
  ScriptEngine engine;
  engine.addLibrary(archive);
  const Script script = engine.compileSource("/library_other_build.prime", ImportLines + ScriptBody);
  CHECK_FALSE(script.valid());
  CHECK(script.diagnostics().find("library was built by another compiler build") != std::string::npos);
}

TEST_CASE("a script cannot reach a library the host did not add") {
  ScriptEngine engine;
  const Script script = engine.compileSource("/library_not_added.prime", ImportLines + ScriptBody);
  CHECK_FALSE(script.valid());
  CHECK_FALSE(script.diagnostics().empty());
}

namespace {

std::string interfaceHashOf(const std::string &archive) {
  primec::LibraryArchiveExpectations expected;
  expected.compilerBuildId = primec::compilerSourceBuildId();
  primec::LibraryArchiveContent content;
  std::string error;
  REQUIRE_MESSAGE(primec::readLibraryArchiveFile(archive, expected, content, error), error);
  return content.manifest.interfaceHash;
}

} // namespace

TEST_CASE("the bytecode of a script records its libraries and runs without the archive") {
  const auto dir = embedTestDir("library_bytecode");
  const std::string archive = buildWithHostCompiler(dir, primec::compilerSourceBuildId());
  ScriptEngine engine;
  engine.addLibrary(archive);
  const Script script = engine.compileSource("/library_bytecode.prime", ImportLines + ScriptBody);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  REQUIRE(script.libraries().size() == 1);
  CHECK(script.libraries()[0].name == "tools");
  CHECK(script.libraries()[0].interfaceHash == interfaceHashOf(archive));

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(script.saveBytecode(bytes, error), error);
  // The same bytes every time, and a host that has no archive and no engine loads them.
  std::vector<uint8_t> again;
  REQUIRE_MESSAGE(script.saveBytecode(again, error), error);
  CHECK(bytes == again);
  const Script loaded = Script::loadBytecode(bytes, "library_bytecode");
  REQUIRE_MESSAGE(loaded.valid(), loaded.diagnostics());
  REQUIRE(loaded.libraries().size() == 1);
  CHECK(loaded.libraries()[0].name == "tools");
  CHECK(loaded.libraries()[0].interfaceHash == script.libraries()[0].interfaceHash);
  const Outcome outcome = runScript(loaded);
  CHECK_MESSAGE(outcome.ok, outcome.diagnostics);
  CHECK(outcome.exitCode == 42);
  CHECK(outcome.out == "from the library\n12\n");

  // A loaded script saves again to the same bytes.
  std::vector<uint8_t> resaved;
  REQUIRE_MESSAGE(loaded.saveBytecode(resaved, error), error);
  CHECK(resaved == bytes);
}

TEST_CASE("the bytecode of a script that imports no library is the plain module") {
  const auto dir = embedTestDir("library_bytecode_unused");
  const std::string archive = buildWithHostCompiler(dir, primec::compilerSourceBuildId());
  ScriptEngine without;
  ScriptEngine with;
  with.addLibrary(archive);
  const std::string plain = "[return<i32>]\nmain() {\n  return(7i32)\n}\n";
  const Script first = without.compileSource("/library_bytecode_unused.prime", plain);
  const Script second = with.compileSource("/library_bytecode_unused.prime", plain);
  CHECK(second.libraries().empty());
  std::vector<uint8_t> firstBytes;
  std::vector<uint8_t> secondBytes;
  std::string error;
  REQUIRE_MESSAGE(first.saveBytecode(firstBytes, error), error);
  REQUIRE_MESSAGE(second.saveBytecode(secondBytes, error), error);
  // No library is imported, so none is linked and none is recorded: the bytes are those of the script alone.
  CHECK(firstBytes == secondBytes);
  CHECK(Script::loadBytecode(secondBytes).run().exitCode == 7);
}

TEST_CASE("a bytecode bundle with a damaged library record is refused") {
  const auto dir = embedTestDir("library_bytecode_damaged");
  const std::string archive = buildWithHostCompiler(dir, primec::compilerSourceBuildId());
  ScriptEngine engine;
  engine.addLibrary(archive);
  const Script script =
      engine.compileSource("/library_bytecode_damaged.prime", ImportLines + ScriptBody);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(script.saveBytecode(bytes, error), error);

  // Cut inside the record of the library, and add bytes after it.
  std::vector<uint8_t> truncated(bytes.begin(), bytes.end() - 3);
  const Script cut = Script::loadBytecode(truncated);
  CHECK_FALSE(cut.valid());
  CHECK(cut.libraries().empty());
  CHECK(cut.diagnostics().find("invalid bytecode bundle") != std::string::npos);

  std::vector<uint8_t> extended = bytes;
  extended.push_back(0);
  const Script trailing = Script::loadBytecode(extended);
  CHECK_FALSE(trailing.valid());
  CHECK(trailing.diagnostics().find("trailing bytes") != std::string::npos);
}

TEST_CASE("a script with an exported function keeps its libraries in the bytecode") {
  const auto dir = embedTestDir("library_bytecode_exports");
  const std::string archive = buildWithHostCompiler(dir, primec::compilerSourceBuildId());
  ScriptEngine engine;
  engine.addLibrary(archive);
  engine.exportFunction<int32_t(int32_t)>("quad_of");
  const std::string source = ImportLines +
                             "[return<i32>]\nquad_of([i32] value) {\n  return(quad(value))\n}\n"
                             "[return<i32>]\nmain() {\n  return(0i32)\n}\n";
  const Script script = engine.compileSource("/library_bytecode_exports.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  REQUIRE(script.libraries().size() == 1);
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(script.saveBytecode(bytes, error), error);
  const Script loaded = Script::loadBytecode(bytes);
  REQUIRE_MESSAGE(loaded.valid(), loaded.diagnostics());
  REQUIRE(loaded.libraries().size() == 1);
  const auto result = loaded.call<int32_t>("quad_of", 5);
  CHECK_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.value == 20);
}

TEST_SUITE_END();
