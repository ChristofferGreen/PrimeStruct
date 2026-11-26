#include "test_compile_run_helpers.h"

#include "primec/pipeline/CompilePipeline.h"

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
  // usage). Lazy expansion's own manifested symbols are cheap, but
  // image.prime's header imports /std/math/*, /std/file/*, and
  // /std/collections/vector - those aren't manifested, so once any
  // image.prime symbol is actually used, the fix for TODO-5229's
  // transitive-import correctness gap (see docs/todo.md) pulls all three
  // of those sibling modules in via the normal, non-lazy path (~1700
  // lines combined). That's still a real, substantial reduction (~8.4x)
  // versus whole-file splice, just less dramatic than lazy expansion's
  // best case (near-zero) for a program that touches nothing outside the
  // manifested module itself. A generous bound catches a regression back
  // to whole-file-splice-equivalent cost without being a flaky
  // exact-count assertion.
  CHECK(callsVisited < 700);
}

TEST_CASE("lazy stdlib imports surface the underlying diagnostic when no manifested symbol matches") {
  // A wildcard import of a lazy-managed module that ends up splicing
  // nothing behaves like default whole-file splice's own guarantee for an
  // unused wildcard import: it's not itself an error (see TODO-5229 - a
  // real stdlib usage simply colliding with an unrelated compiler builtin
  // of the same spelling is one legitimate way this happens, not evidence
  // the closure scan missed something). So the call to a genuinely
  // nonexistent function surfaces its own, more specific "unknown call
  // target" diagnostic directly, rather than a generic "could not find a
  // manifested symbol" message manufactured for an import that was never
  // the actual problem.
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
  CHECK(output.find("unknown call target: totallyBogusImageFunctionThatDoesNotExist") != std::string::npos);
}

TEST_CASE("lazy stdlib imports resolve a templated method call on an experimental struct") {
  // Regression test: TemplateMonomorphMethodTargets.h's GfxError special
  // case hardcoded the non-experimental /std/gfx/GfxError base path for
  // why/status/result method calls, so a templated call
  // (err.result<i32>()) on an experimental GfxError requested
  // instantiation of a base path that doesn't exist and silently produced
  // no specialization. This was masked under whole-file splicing (some
  // other explicit-absolute-path call elsewhere in experimental.prime
  // happened to trigger the same specialization via the correct path) and
  // only surfaced once lazy expansion stopped including that unrelated
  // cover. Fixed to prefer whichever GfxError base path is actually
  // present.
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<i32, GfxError>] s{err.result<i32>()}
  [string] w{Result.why(s)}
  print_line(w)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("lazy_stdlib_imports_gfx_templated_method.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_lazy_gfx_templated_method").string();
  const std::string outPath = (testScratchPath("") / "primec_lazy_gfx_templated_method.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath +
      " --entry /main --experimental-lazy-stdlib-imports";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "queue_submit_failed\n");
}

TEST_CASE("a program that imports one math type publishes only the definitions it reaches") {
  const std::string source = R"(
import /std/math/Vec2

[return<int>]
main() {
  [Vec2] v{Vec2{[x] 3.0f32, [y] 4.0f32}}
  return(convert<i32>(v.length()))
}
)";
  const std::string srcPath = writeTemp("lazy_math_vec2_only.prime", source);
  const std::string dumpPath = (testScratchPath("") / "primec_lazy_math_vec2_only.txt").string();
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == 5);
  CHECK(runCommand("./primec --dump-stage=semantic-product " + srcPath + " --entry /main > " +
                   dumpPath) == 0);
  const std::string dump = readFile(dumpPath);
  size_t definitionLines = 0;
  for (size_t pos = dump.find("\n  definitions"); pos != std::string::npos;
       pos = dump.find("\n  definitions", pos + 1)) {
    ++definitionLines;
  }
  // The whole math module is 99 definitions; the scan takes Vec2 and the methods it calls, not the
  // `length` of every other math type, and no operator helper while the program calls no operator.
  // The 6 are `main`, `Vec2`, its two namespaces, `length` and `lengthSquared`.
  CHECK(definitionLines > 0);
  CHECK(definitionLines <= 6);
  CHECK(dump.find("/std/math/Vec3") == std::string::npos);
  CHECK(dump.find("/std/math/Quat") == std::string::npos);
  CHECK(dump.find("/std/math/Mat4") == std::string::npos);
}

TEST_CASE(
    "a method call takes the method of the receiver type and no other type's method of that name") {
  const std::string source = R"(
import /std/math/Vec3

[return<int>]
main() {
  [Vec3] v{Vec3{[x] 0.0f32, [y] 3.0f32, [z] 4.0f32}}
  return(convert<i32>(v.length()))
}
)";
  const std::string srcPath = writeTemp("lazy_math_vec3_method.prime", source);
  const std::string dumpPath = (testScratchPath("") / "primec_lazy_math_vec3_method.txt").string();
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == 5);
  CHECK(runCommand("./primec --dump-stage=semantic-product " + srcPath + " --entry /main > " +
                   dumpPath) == 0);
  const std::string dump = readFile(dumpPath);
  CHECK(dump.find("/std/math/Vec3/length") != std::string::npos);
  CHECK(dump.find("/std/math/Vec2/length") == std::string::npos);
  CHECK(dump.find("/std/math/Vec4/length") == std::string::npos);
  CHECK(dump.find("/std/math/Quat") == std::string::npos);
}

TEST_CASE("matrix, quaternion and color operators run through the lazy math import") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] m{Mat2{}}
  [Vec2] v{Vec2{[x] 1.0f32, [y] 2.0f32}}
  [Vec2] r{multiply(m, v)}
  [Quat] q{Quat{}}
  [Vec3] p{Vec3{[x] 1.0f32, [y] 0.0f32, [z] 0.0f32}}
  [Vec3] rotated{multiply(q, p)}
  [ColorRGBA] c{ColorRGBA{0.5f32, 0.5f32, 0.5f32, 1.0f32}}
  return(plus(plus(convert<i32>(r.x), convert<i32>(rotated.y)), convert<i32>(c.a)))
}
)";
  const std::string srcPath = writeTemp("lazy_math_operators.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_lazy_math_operators").string();
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == 1);
  CHECK(runCommand("./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main") == 0);
  CHECK(runCommand(exePath) == 1);
}

// Receivers whose type name is never spelled in the program text: the type is reached through an
// inferred local, a method result or an operator result, so the method entry must follow it.
TEST_CASE("a method chain reaches methods of types the text never spells") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [ColorRGB] c{ColorRGB{0.5f32, 0.5f32, 0.5f32}}
  [auto] back{c.toSRGB().toLinear()}
  return(convert<i32>(back.mulScalar(10.0f32).r))
}
)";
  const std::string srcPath = writeTemp("lazy_math_color_chain.prime", source);
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == 5);
  CHECK(runCommand("./primec --emit=vm --whole-file-stdlib-imports " + srcPath +
                   " --entry /main") == 5);
}

TEST_CASE("a method on an inferred vector result of a method chain runs") {
  const std::string source = R"(
import /std/math/Vec3

[return<int>]
main() {
  [auto] a{Vec3{[x] 1.0f32, [y] 0.0f32, [z] 0.0f32}}
  [auto] b{Vec3{[x] 0.0f32, [y] 1.0f32, [z] 0.0f32}}
  [auto] c{a.cross(b).mulScalar(5.0f32).add(a)}
  return(convert<i32>(c.length()))
}
)";
  const std::string srcPath = writeTemp("lazy_math_vec3_chain.prime", source);
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == 5);
  CHECK(runCommand("./primec --emit=vm --whole-file-stdlib-imports " + srcPath +
                   " --entry /main") == 5);
}

TEST_CASE("a method on a value inferred from a matrix or quaternion operator runs") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] q{Quat{}}
  [auto] m{quat_to_mat3(q)}
  [auto] r{multiply(m, Vec3{[x] 3.0f32, [y] 4.0f32, [z] 0.0f32})}
  [auto] rotated{multiply(q, Vec3{[x] 0.0f32, [y] 3.0f32, [z] 4.0f32})}
  return(plus(convert<i32>(r.length()), convert<i32>(rotated.normalize().mulScalar(7.0f32).length())))
}
)";
  const std::string srcPath = writeTemp("lazy_math_operator_results.prime", source);
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == 12);
  CHECK(runCommand("./primec --emit=vm --whole-file-stdlib-imports " + srcPath +
                   " --entry /main") == 12);
}

TEST_CASE("an operator reached only through an inferred receiver takes its helpers and runs") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] q{Quat{}}
  [auto] r{multiply(q, Vec3{[x] 0.0f32, [y] 3.0f32, [z] 4.0f32})}
  [auto] s{plus(r, r)}
  return(convert<i32>(s.length()))
}
)";
  const std::string srcPath = writeTemp("lazy_math_inferred_operator.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_lazy_math_inferred_operator").string();
  const int expected =
      runCommand("./primec --emit=vm --whole-file-stdlib-imports " + srcPath + " --entry /main");
  CHECK(expected > 0);
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == expected);
  CHECK(runCommand("./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main") == 0);
  CHECK(runCommand(exePath) == expected);
}

// The VM and optexe run a program through lazy import and through whole-file import with one result.
static void checkLazyMatchesWholeFile(const std::string &srcPath, const std::string &name) {
  const int expected =
      runCommand("./primec --emit=vm --whole-file-stdlib-imports " + srcPath + " --entry /main");
  CHECK(expected > 0);
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == expected);
  for (const char *kind : {"exe", "optexe"}) {
    for (const char *imports : {"", " --whole-file-stdlib-imports"}) {
      const std::string exePath =
          (testScratchPath("") / (std::string("primec_lazy_") + name + "_" + kind)).string();
      CHECK(runCommand(std::string("./primec --emit=") + kind + imports + " " + srcPath + " -o " +
                       exePath + " --entry /main") == 0);
      CHECK(runCommand(exePath) == expected);
    }
  }
}

TEST_CASE("operators written without spaces on vectors take their helpers and run") {
  const std::string source = R"(
import /std/math/Vec2

[return<int>]
main() {
  [Vec2] a{Vec2{[x] 1.0f32, [y] 2.0f32}}
  [Vec2] b{Vec2{[x] 3.0f32, [y] 4.0f32}}
  [Vec2] sum{a+b}
  [Vec2] difference{b-a}
  [Vec2] scaled{a*2.0f32}
  [Vec2] scaledLeft{2.0f32*b}
  [Vec2] halved{b/2.0f32}
  return(convert<i32>(sum.x+difference.y+scaled.y+scaledLeft.x+halved.y))
}
)";
  checkLazyMatchesWholeFile(writeTemp("lazy_math_unspaced_infix.prime", source), "unspaced_infix");
}

TEST_CASE("operator helpers are taken when definitions validate on several workers") {
  const std::string source = R"(
import /std/math/Vec2

[return<int>]
main() {
  [Vec2] a{Vec2{[x] 1.0f32, [y] 2.0f32}}
  [Vec2] b{Vec2{[x] 3.0f32, [y] 4.0f32}}
  [Vec2] sum{a+b}
  [Vec2] scaled{a*2.0f32}
  return(convert<i32>(sum.x+scaled.y))
}
)";
  const std::string srcPath = writeTemp("lazy_math_operator_workers.prime", source);
  const int expected = runCommand("./primec --emit=vm " + srcPath + " --entry /main");
  CHECK(expected == 8);
  for (const char *workers : {"2", "4", "8"}) {
    CHECK(runCommand(std::string("./primec --emit=vm "
                                 "--benchmark-semantic-definition-validation-workers ") +
                     workers + " " + srcPath + " --entry /main") == expected);
  }
}

namespace {

struct PipelineCounts {
  bool ok = false;
  uint32_t rounds = 0;
  uint32_t restarts = 0;
  std::size_t definitions = 0;
  std::string error;
};

// Compiles `source` through the lazy stdlib import and reports how often it was validated and re-imported.
PipelineCounts lazyPipelineCounts(const std::string &name,
                                  const std::string &source,
                                  const std::vector<std::string> &libraries = {}) {
  primec::Options options;
  options.inputPath = writeTemp(name, source);
  options.entryPath = "/main";
  options.experimentalLazyStdlibImports = true;
  options.libraryPaths = libraries;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);
  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  PipelineCounts counts;
  counts.ok = primec::runCompilePipeline(options, output, errorStage, counts.error);
  counts.rounds = output.pipelineRounds;
  counts.restarts = output.pipelineRestarts;
  counts.definitions = output.program.definitions.size();
  return counts;
}

} // namespace

TEST_CASE("a program with a vector operator is validated in one round") {
  const PipelineCounts plusSite = lazyPipelineCounts("lazy_rounds_vec_plus.prime", R"(
import /std/math/Vec2

[return<int>]
main() {
  [Vec2] a{Vec2{[x] 1.0f32, [y] 2.0f32}}
  [Vec2] s{a + a}
  [Vec2] t{a*2.0f32}
  return(convert<i32>(s.x + t.y))
}
)");
  CHECK_MESSAGE(plusSite.ok, plusSite.error);
  CHECK(plusSite.rounds == 1);
  CHECK(plusSite.restarts == 1);

  const PipelineCounts noOperator = lazyPipelineCounts("lazy_rounds_vec_length.prime", R"(
import /std/math/Vec2

[return<int>]
main() {
  [Vec2] a{Vec2{[x] 3.0f32, [y] 4.0f32}}
  return(convert<i32>(a.length()))
}
)");
  CHECK_MESSAGE(noOperator.ok, noOperator.error);
  CHECK(noOperator.rounds == 1);
  CHECK(noOperator.restarts == 0);
  CHECK(noOperator.definitions <= 6);
}

TEST_CASE("an operator site the scan cannot type is found by the validator in a second round") {
  const std::string source = R"(
import /std/math/Vec2

[return<Vec2>]
make_vector() {
  return(Vec2{[x] 1.0f32, [y] 2.0f32})
}

[return<int>]
main() {
  [auto] s{make_vector() + make_vector()}
  return(convert<i32>(s.y))
}
)";
  const PipelineCounts counts = lazyPipelineCounts("lazy_rounds_untyped_site.prime", source);
  CHECK_MESSAGE(counts.ok, counts.error);
  CHECK(counts.rounds == 2);
  const std::string srcPath = writeTemp("lazy_rounds_untyped_site_run.prime", source);
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == 4);
}

TEST_CASE("a program that links the vector library is validated in one round with no re-import") {
  const std::string depsPath = (testScratchPath("") / "lazy_rounds_vector_deps.txt").string();
  CHECK(runCommand("./primec --emit=vm --whole-file-stdlib-imports --emit-deps " + depsPath + " " +
                   writeTemp("lazy_rounds_vector_probe.prime", R"(import /std/math/*

[return<int>]
main() {
  [Vec2] v{Vec2{}}
  return(convert<i32>(linearToSrgbChannel(0.5f32)))
}
)") + " --entry /main") == 0);
  std::string vectorFile;
  {
    std::istringstream deps(readFile(depsPath));
    std::string line;
    const std::string tail = "/stdlib/std/math/vector.prime";
    while (std::getline(deps, line)) {
      if (line.size() >= tail.size() &&
          line.compare(line.size() - tail.size(), tail.size(), tail) == 0) {
        vectorFile = line;
      }
    }
  }
  REQUIRE(!vectorFile.empty());
  const std::string archive = (testScratchPath("") / "lazy_rounds_vector.psa").string();
  CHECK(runCommand("./primec --emit=lib " + vectorFile + " -o " + archive) == 0);
  const PipelineCounts counts = lazyPipelineCounts("lazy_rounds_vector_library.prime",
                                                   R"(import</std/math>
import /std/math/*

[return<int>]
main() {
  [Vec2] a{Vec2{[x] 1.0f32, [y] 2.0f32}}
  [Vec2] s{a + a}
  return(convert<i32>(s.x))
}
)",
                                                   {archive});
  CHECK_MESSAGE(counts.ok, counts.error);
  CHECK(counts.rounds == 1);
  CHECK(counts.restarts == 0);
}

TEST_CASE("an operator inside a module the program imports takes its helper and runs") {
  const std::string moduleSource = R"(
import /std/math/Vec2

[public return<Vec2>]
sum_pair([Vec2] a, [Vec2] b) {
  return(a+b)
}
)";
  const std::string modulePath = writeTemp("lazy_math_operator_module.prime", moduleSource);
  const std::string source = "import<\"" + modulePath + R"(">
import /std/math/Vec2

[return<int>]
main() {
  [Vec2] s{sum_pair(Vec2{[x] 1.0f32, [y] 2.0f32}, Vec2{[x] 3.0f32, [y] 4.0f32})}
  return(convert<i32>(s.x))
}
)";
  checkLazyMatchesWholeFile(writeTemp("lazy_math_operator_in_module.prime", source),
                            "operator_module");
}

TEST_CASE("a method on an image error returned by a function runs without naming the type") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  [auto] err{imageReadUnsupported()}
  print_line(err.why())
  print_line(imageWriteUnsupported().why())
  return(plus(err.status(), imageInvalidOperation().status()))
}
)";
  const std::string srcPath = writeTemp("lazy_image_error_methods.prime", source);
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == 4);
  CHECK(runCommand("./primec --emit=vm --whole-file-stdlib-imports " + srcPath +
                   " --entry /main") == 4);
}

TEST_SUITE_END();
