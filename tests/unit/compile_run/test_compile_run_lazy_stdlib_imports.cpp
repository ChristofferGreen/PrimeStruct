#include "test_compile_run_helpers.h"

// TODO-5228 (docs/LibrarySymbolManifestLazyImports.md): opt-in iterative
// lazy stdlib import expansion. --experimental-lazy-stdlib-imports defaults
// off, so every other compile_run test is unaffected; these tests exercise
// the flag directly against the two manifested modules
// (stdlib/std/image/image.psmeta, stdlib/std/gfx/experimental.psmeta).

TEST_SUITE_BEGIN("primestruct.compile.run.lazy_stdlib_imports");

TEST_CASE("lazy stdlib imports produce identical output for a light-usage /std/image program") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  [ImageError] err{imageReadUnsupported()}
  [Result<ImageError>] status{imageErrorStatus(err)}
  [string] why{Result.why(status)}
  print_line(why)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("lazy_stdlib_imports_image_light_usage.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_lazy_image_light_usage").string();
  const std::string outPath = (testScratchPath("") / "primec_lazy_image_light_usage.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath +
      " --entry /main --experimental-lazy-stdlib-imports";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "image_read_unsupported\n");
}

TEST_CASE("lazy stdlib imports produce identical output for a light-usage /std/gfx/experimental program") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] status{err.status()}
  [string] why{Result.why(status)}
  print_line(why)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("lazy_stdlib_imports_gfx_light_usage.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_lazy_gfx_light_usage").string();
  const std::string outPath = (testScratchPath("") / "primec_lazy_gfx_light_usage.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath +
      " --entry /main --experimental-lazy-stdlib-imports";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "queue_submit_failed\n");
}

TEST_CASE("lazy stdlib imports drastically reduce semantic calls_visited for light usage") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  [ImageError] err{imageReadUnsupported()}
  [Result<ImageError>] status{imageErrorStatus(err)}
  [string] why{Result.why(status)}
  print_line(why)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("lazy_stdlib_imports_image_phase_counters.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_lazy_image_phase_counters.txt").string();

  const std::string lazyCmd = "./primec --emit=vm " + srcPath +
                              " --entry /main --experimental-lazy-stdlib-imports "
                              "--benchmark-semantic-phase-counters > " +
                              outPath + " 2>&1";
  CHECK(runCommand(lazyCmd) == 0);
  const std::string lazyOutput = readFile(outPath);

  const std::string marker = "\"calls_visited\":";
  const size_t markerPos = lazyOutput.find(marker);
  REQUIRE(markerPos != std::string::npos);
  const size_t numberStart = markerPos + marker.size();
  const size_t numberEnd = lazyOutput.find_first_not_of("0123456789", numberStart);
  REQUIRE(numberEnd != std::string::npos);
  const int callsVisited = std::stoi(lazyOutput.substr(numberStart, numberEnd - numberStart));

  // The default whole-file splice visits ~4900 calls for this exact
  // program (the entire /std/image module gets validated regardless of
  // usage); lazy expansion should stay in the low hundreds for a handful
  // of referenced symbols. A generous bound keeps this from being a flaky
  // exact-count assertion while still catching a regression back to
  // whole-file-splice-equivalent cost.
  CHECK(callsVisited < 200);
}

TEST_CASE("lazy stdlib imports surface a clear diagnostic when no manifested symbol matches") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  print_line(totallyBogusImageFunctionThatDoesNotExist())
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("lazy_stdlib_imports_unresolved.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_lazy_unresolved.txt").string();

  const std::string cmd = "./primec --emit=vm " + srcPath +
                          " --entry /main --experimental-lazy-stdlib-imports > " +
                          outPath + " 2>&1";
  CHECK(runCommand(cmd) != 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("unknown symbol in imported library /std/image") != std::string::npos);
}

TEST_SUITE_END();
