#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("compiles concrete examples to IR") {
  const std::filesystem::path examplesDir = std::filesystem::path("..") / "examples";
  REQUIRE(std::filesystem::exists(examplesDir));
  const std::vector<std::filesystem::path> exampleFiles = collectExamplePrimeFiles(examplesDir);
  REQUIRE(!exampleFiles.empty());
  compileExampleIrBatch(examplesDir, exampleFiles, "primec_examples_ir_concrete", {"0.Concrete/"}, true);
}

TEST_CASE("compiles template and inference examples to IR") {
  const std::filesystem::path examplesDir = std::filesystem::path("..") / "examples";
  REQUIRE(std::filesystem::exists(examplesDir));
  const std::vector<std::filesystem::path> exampleFiles = collectExamplePrimeFiles(examplesDir);
  REQUIRE(!exampleFiles.empty());
  compileExampleIrBatch(examplesDir, exampleFiles, "primec_examples_ir_template_inference",
                        {"1.Template/", "2.Inference/"}, true);
}

TEST_CASE("compiles surface examples to IR") {
  const std::filesystem::path examplesDir = std::filesystem::path("..") / "examples";
  REQUIRE(std::filesystem::exists(examplesDir));
  const std::vector<std::filesystem::path> exampleFiles = collectExamplePrimeFiles(examplesDir);
  REQUIRE(!exampleFiles.empty());
  compileExampleIrBatch(examplesDir, exampleFiles, "primec_examples_ir_surface", {"3.Surface/"}, true);
}

TEST_CASE("compiles web examples to IR") {
  const std::filesystem::path examplesDir = std::filesystem::path("..") / "examples";
  REQUIRE(std::filesystem::exists(examplesDir));
  const std::vector<std::filesystem::path> exampleFiles = collectExamplePrimeFiles(examplesDir);
  REQUIRE(!exampleFiles.empty());
  compileExampleIrBatch(examplesDir, exampleFiles, "primec_examples_ir_web", {"web/"},
                        spinningCubeBackendsSupportArrayReturns());
}

TEST_CASE("collection docs snippets stay c++ style and executable" * doctest::skip(true)) {
  auto resolveDocPath = [](const std::string &name) -> std::filesystem::path {
    std::filesystem::path path = std::filesystem::path("..") / "docs" / name;
    if (!std::filesystem::exists(path)) {
      path = std::filesystem::current_path() / "docs" / name;
    }
    return path;
  };

  const std::filesystem::path primeStructPath = resolveDocPath("PrimeStruct.md");
  const std::filesystem::path syntaxSpecPath = resolveDocPath("PrimeStruct_SyntaxSpec.md");
  const std::filesystem::path guidelinesPath = resolveDocPath("Coding_Guidelines.md");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(guidelinesPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpecDoc = readFile(syntaxSpecPath.string());
  const std::string guidelinesDoc = readFile(guidelinesPath.string());

  const std::vector<std::string> requiredPrimeStructSnippets = {
      "value.push(item)", "value.at(index)", "value[index]", "value.count()",
      "map<string, i32>{\"a\"utf8=1i32}", "to_soa(spawnQueue)",
      "two-phase: run a stable-size update pass first"};
  const std::vector<std::string> requiredSyntaxSpecSnippets = {
      "vector<i32>{1i32, 2i32}", "vector<i32>[1i32, 2i32]",
      "map<i32, i32>{1i32=2i32, 3i32=4i32}", "value.push(item)",
      "value.at(index)", "value[index]", "Structural mutation boundaries are `push`, `reserve`, `to_soa`, and `to_aos`."};
  const std::vector<std::string> requiredGuidelinesSnippets = {
      "value.push(x)", "value.at(i)", "value[i]", "value.count()",
      "[vector<i32> mut] values{1, 2}"};

  for (const std::string &snippet : requiredPrimeStructSnippets) {
    CAPTURE(snippet);
    CHECK(primeStructDoc.find(snippet) != std::string::npos);
  }
  for (const std::string &snippet : requiredSyntaxSpecSnippets) {
    CAPTURE(snippet);
    CHECK(syntaxSpecDoc.find(snippet) != std::string::npos);
  }
  for (const std::string &snippet : requiredGuidelinesSnippets) {
    CAPTURE(snippet);
    CHECK(guidelinesDoc.find(snippet) != std::string::npos);
  }

  struct SnippetCase {
    std::string tempName;
    std::string source;
    int expectedExitCode;
  };
  const std::vector<SnippetCase> snippetCases = {
      {"docs_collections_prime_struct_style.prime",
       R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>{1i32, 2i32, 3i32}}
  [vector<i32> mut] list{vector<i32>{4i32, 5i32}}
  [map<i32, i32>] pairs{map<i32, i32>{7i32=8i32, 9i32=10i32}}
  list.push(6i32)
  list.reserve(8i32)
  return(values[1i32] + list.at(2i32) + values.count() + at(pairs, 9i32))
}
)",
       21},
      {"docs_collections_syntax_spec_style.prime",
       R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>[1i32, 2i32]}
  [vector<i32> mut] list{vector<i32>[3i32, 4i32]}
  [map<i32, i32>] pairs{map<i32, i32>[5i32=6i32, 7i32=8i32]}
  list.push(9i32)
  return(values.at(0i32) + list[2i32] + at_unsafe(pairs, 7i32))
}
)",
       18},
      {"docs_collections_guidelines_style.prime",
       R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] baseline{array<i32>{1, 2, 3}}
  [vector<i32> mut] values{1, 2}
  [map<i32, i32>] pairs{map<i32, i32>{7=10}}
  values.push(3)
  values.reserve(8)
  return(values[0] + values.at(2) + baseline.count() + at(pairs, 7))
}
)",
       17},
  };

  for (const auto &snippetCase : snippetCases) {
    CAPTURE(snippetCase.tempName);
    const std::string srcPath = writeTemp(snippetCase.tempName, snippetCase.source);
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
    CHECK(runCommand(runCmd) == snippetCase.expectedExitCode);
  }
}

TEST_CASE("collection docs snippets reject mutators in value position") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>{1i32}}
  [i32] bad{values.push(2i32)}
  return(bad)
}
)";
  const std::string srcPath = writeTemp("docs_collections_statement_only_negative.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_docs_collections_statement_only_negative.err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("spinning cube shared source compiles across profile targets") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube backend matrix until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path cubePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  REQUIRE(std::filesystem::exists(cubePath));

  const std::string nativePath =
      (testScratchPath("") / "primec_spinning_cube_native_smoke").string();
  const std::string nativeMainErrPath =
      (testScratchPath("") / "primec_spinning_cube_native_main_reject.err.txt").string();
  const std::string wasmPath =
      (testScratchPath("") / "primec_spinning_cube_browser_smoke.wasm").string();
  const std::string metalErrPath =
      (testScratchPath("") / "primec_spinning_cube_metal_smoke.err.txt").string();

  const std::string nativeMainCmd = "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
                                    quoteShellArg(nativePath) + " --entry /main 2> " +
                                    quoteShellArg(nativeMainErrPath);
  CHECK(runCommand(nativeMainCmd) == 2);
  CHECK(readFile(nativeMainErrPath).find("native backend does not support return type on /cubeInit") !=
        std::string::npos);

  const std::string nativeCmd = "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
                                quoteShellArg(nativePath) + " --entry /mainNative";
  CHECK(runCommand(nativeCmd) == 0);
  CHECK(std::filesystem::exists(nativePath));

  const std::string wasmBrowserCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath) + " --entry /main";
  CHECK(runCommand(wasmBrowserCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  const std::string metalCmd = "./primec --emit=metal " + quoteShellArg(cubePath.string()) +
                               " -o /dev/null --entry /main 2> " + quoteShellArg(metalErrPath);
  CHECK(runCommand(metalCmd) == 2);
  CHECK(readFile(metalErrPath).find("Usage: primec") != std::string::npos);
}


TEST_SUITE_END();
