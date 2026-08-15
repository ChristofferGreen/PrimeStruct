#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

#include <utility>
#include <vector>

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

TEST_CASE("checked-in ast transform example runs in VM") {
  std::filesystem::path consumerPath =
      std::filesystem::path("..") / "examples" / "4.Transforms" / "trace_calls_consumer.prime";
  std::filesystem::path hookPath =
      std::filesystem::path("..") / "examples" / "4.Transforms" / "trace_calls_transform.prime";
  if (!std::filesystem::exists(consumerPath)) {
    consumerPath =
        std::filesystem::current_path() / "examples" / "4.Transforms" / "trace_calls_consumer.prime";
  }
  if (!std::filesystem::exists(hookPath)) {
    hookPath =
        std::filesystem::current_path() / "examples" / "4.Transforms" / "trace_calls_transform.prime";
  }
  REQUIRE(std::filesystem::exists(consumerPath));
  REQUIRE(std::filesystem::exists(hookPath));

  const std::string hookSource = readFile(hookPath.string());
  const std::string consumerSource = readFile(consumerPath.string());
  CHECK(hookSource.find("[public ast return<FunctionAst>]") != std::string::npos);
  CHECK(hookSource.find("trace_calls([FunctionAst] fn)") != std::string::npos);
  CHECK(hookSource.find("return(replace_body_with_return_i32(fn, 42i32))") != std::string::npos);
  CHECK(consumerSource.find("import<\"./trace_calls_transform.prime\">") != std::string::npos);
  CHECK(consumerSource.find("import /ast_hooks") != std::string::npos);
  CHECK(consumerSource.find("[trace_calls return<int>]") != std::string::npos);

  const std::string runCmd = "./primec --emit=vm " + quoteShellArg(consumerPath.string()) + " --entry /main";
  CHECK(runCommand(runCmd) == 42);
}

TEST_CASE("procedural generic docs example runs in VM and native") {
  auto resolveRepoPath = [](const std::string &name) -> std::filesystem::path {
    std::filesystem::path path = std::filesystem::path("..") / name;
    if (!std::filesystem::exists(path)) {
      path = std::filesystem::current_path() / name;
    }
    return path;
  };

  const std::filesystem::path examplePath =
      resolveRepoPath("examples/2.Inference/procedural_generic_box.prime");
  const std::filesystem::path syntaxSpecPath =
      resolveRepoPath("docs/PrimeStruct_SyntaxSpec.md");
  const std::filesystem::path codeExamplesPath =
      resolveRepoPath("docs/CodeExamples.md");
  REQUIRE(std::filesystem::exists(examplePath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(codeExamplesPath));

  const std::string syntaxSpec = readFile(syntaxSpecPath.string());
  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::vector<std::string> requiredSnippets = {
      "[type] ValueT { typeof<first> }",
      "[struct] BoxT",
      "[BoxT] box{BoxT{first}}",
      "return(remember_first<i32>(23i32, 41i32))"};
  for (const std::string &snippet : requiredSnippets) {
    CAPTURE(snippet);
    CHECK(codeExamples.find(snippet) != std::string::npos);
  }
  CHECK(syntaxSpec.find("Procedural Compile-Time Genericity") !=
        std::string::npos);
  CHECK(syntaxSpec.find("`typeof<symbol>` is a compile-time primitive") !=
        std::string::npos);
  CHECK(syntaxSpec.find("Local generated types cannot escape") !=
        std::string::npos);

  const std::string nativePath =
      (testScratchPath("") / "primec_procedural_generic_docs_native").string();
  const std::string runVmCmd =
      "./primec --emit=vm " + quoteShellArg(examplePath.string()) +
      " --entry /main";
  CHECK(runCommand(runVmCmd) == 23);

  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(examplePath.string()) +
      " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 23);
}

TEST_CASE("generic design examples stay documented and executable") {
  auto resolveRepoPath = [](const std::string &name) -> std::filesystem::path {
    std::filesystem::path path = std::filesystem::path("..") / name;
    if (!std::filesystem::exists(path)) {
      path = std::filesystem::current_path() / name;
    }
    return path;
  };

  const std::filesystem::path codeExamplesPath =
      resolveRepoPath("docs/CodeExamples.md");
  const std::filesystem::path primeStructPath =
      resolveRepoPath("docs/PrimeStruct.md");
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(primeStructPath));

  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::vector<std::string> requiredCodeExampleSnippets = {
      "## Generic Coding Style",
      "Use plain inference for pass-through helpers",
      "Use requirements when an unconstrained helper would fail later",
      "[return<T> require(typeof<left> == typeof<right>, N > 0)]",
      "return(ct_if(type_equals<typeof<value>, i32>())",
      "[struct] Pair<LeftT, RightT>",
      "`examples/2.Inference/generic_requirements_design.prime`"};
  for (const std::string &snippet : requiredCodeExampleSnippets) {
    CAPTURE(snippet);
    CHECK(codeExamples.find(snippet) != std::string::npos);
  }
  CHECK(primeStructDoc.find("High-level generic examples should follow the "
                            "style section in") != std::string::npos);
  CHECK(primeStructDoc.find("examples/2.Inference/"
                            "generic_pair_design.prime") !=
        std::string::npos);

  const std::vector<std::pair<std::string, int>> examples = {
      {"examples/2.Inference/generic_identity.prime", 42},
      {"examples/2.Inference/generic_pair_design.prime", 42},
      {"examples/2.Inference/generic_requirements_design.prime", 50}};
  for (const auto &[exampleName, expectedExit] : examples) {
    const std::filesystem::path examplePath = resolveRepoPath(exampleName);
    REQUIRE(std::filesystem::exists(examplePath));
    CAPTURE(exampleName);

    const std::string runVmCmd =
        "./primec --emit=vm " + quoteShellArg(examplePath.string()) +
        " --entry /main";
    CHECK(runCommand(runVmCmd) == expectedExit);

    const std::string nativePath =
        (testScratchPath("") /
         ("primec_" + examplePath.stem().string() + "_native"))
            .string();
    const std::string compileNativeCmd =
        "./primec --emit=native " + quoteShellArg(examplePath.string()) +
        " -o " + quoteShellArg(nativePath) + " --entry /main";
    CHECK(runCommand(compileNativeCmd) == 0);
    CHECK(runCommand(quoteShellArg(nativePath)) == expectedExit);
  }
}

TEST_CASE("collection docs snippets stay code-examples style and executable") {
  auto resolveDocPath = [](const std::string &name) -> std::filesystem::path {
    std::filesystem::path path = std::filesystem::path("..") / "docs" / name;
    if (!std::filesystem::exists(path)) {
      path = std::filesystem::current_path() / "docs" / name;
    }
    return path;
  };

  const std::filesystem::path primeStructPath = resolveDocPath("PrimeStruct.md");
  const std::filesystem::path syntaxSpecPath = resolveDocPath("PrimeStruct_SyntaxSpec.md");
  const std::filesystem::path codeExamplesPath = resolveDocPath("CodeExamples.md");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(codeExamplesPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpecDoc = readFile(syntaxSpecPath.string());
  const std::string codeExamplesDoc = readFile(codeExamplesPath.string());

  const std::vector<std::string> requiredPrimeStructSnippets = {
      "value.push(item)", "value.at(index)", "value[index]", "value.count()",
      "particles.reserve(plus(particles.count(), spawnQueue.count()))",
      "Preferred update pattern is two-phase"};
  const std::vector<std::string> requiredSyntaxSpecSnippets = {
      "value.push(item)", "value.at(index)", "value[index]"};
  const std::vector<std::string> requiredCodeExamplesSnippets = {
      "### Collection Method Sugar",
      "values.push(3)",
      "values.reserve(8)",
      "values.at(0)",
      "values.at(2)",
      "values.count()"};

  for (const std::string &snippet : requiredPrimeStructSnippets) {
    CAPTURE(snippet);
    CHECK(primeStructDoc.find(snippet) != std::string::npos);
  }
  CHECK(primeStructDoc.find("map<string, i32>") != std::string::npos);
  CHECK(primeStructDoc.find("\"a\"utf8, 1i32") != std::string::npos);
  for (const std::string &snippet : requiredSyntaxSpecSnippets) {
    CAPTURE(snippet);
    CHECK(syntaxSpecDoc.find(snippet) != std::string::npos);
  }
  CHECK(syntaxSpecDoc.find("vector<i32>{") != std::string::npos);
  CHECK(syntaxSpecDoc.find("vector<i32>[") != std::string::npos);
  CHECK(syntaxSpecDoc.find("1i32, 2i32") != std::string::npos);
  CHECK(syntaxSpecDoc.find("map<i32, i32>(1i32, 2i32, 3i32, 4i32)") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("entry<i32, i32>(1i32, 2i32)") != std::string::npos);
  CHECK(syntaxSpecDoc.find("Structural mutation boundaries are") != std::string::npos);
  CHECK(syntaxSpecDoc.find("`remove_at`") != std::string::npos);
  CHECK(syntaxSpecDoc.find("`remove_swap`") != std::string::npos);
  CHECK(syntaxSpecDoc.find("`clear`") != std::string::npos);
  CHECK(syntaxSpecDoc.find("`to_soa(vector<T>)`") != std::string::npos);
  CHECK(syntaxSpecDoc.find("`to_aos(soa<T>)`") != std::string::npos);
  for (const std::string &snippet : requiredCodeExamplesSnippets) {
    CAPTURE(snippet);
    CHECK(codeExamplesDoc.find(snippet) != std::string::npos);
  }
  CHECK(codeExamplesDoc.find("[vector<int> mut]") != std::string::npos);
  CHECK(codeExamplesDoc.find("values{1, 2}") != std::string::npos);

  struct SnippetCase {
    std::string tempName;
    std::string source;
    int expectedExitCode;
  };
  const std::vector<SnippetCase> snippetCases = {
      {"docs_collections_prime_struct_doc_style.prime",
       R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>{1i32, 2i32, 3i32}}
  [vector<i32> mut] list{vector<i32>{4i32, 5i32}}
  list.push(6i32)
  list.reserve(8i32)
  return(values[1i32] + list.at(2i32) + values.count())
}
)",
       11},
      {"docs_collections_syntax_spec_doc_style.prime",
       R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>[1i32, 2i32]}
  [vector<i32> mut] list{vector<i32>[3i32, 4i32]}
  list.push(9i32)
  return(values[0i32] + list.at(2i32))
}
)",
       10},
      {"docs_collections_code_examples_doc_style.prime",
       R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] baseline{array<i32>{1, 2, 3}}
  [vector<i32> mut] values{1, 2}
  values.push(3)
  values.reserve(8)
  return(values[0] + values.at(2) + baseline.count())
}
)",
       7},
  };

  for (const auto &snippetCase : snippetCases) {
    CAPTURE(snippetCase.tempName);
    const std::string srcPath = writeTemp(snippetCase.tempName, snippetCase.source);
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
    CHECK(runCommand(runCmd) == snippetCase.expectedExitCode);
  }
}

TEST_CASE("sum docs snippets stay public style and executable") {
  auto resolveRepoPath = [](const std::string &name) -> std::filesystem::path {
    std::filesystem::path path = std::filesystem::path("..") / name;
    if (!std::filesystem::exists(path)) {
      path = std::filesystem::current_path() / name;
    }
    return path;
  };

  const std::filesystem::path readmePath = resolveRepoPath("README.md");
  const std::filesystem::path codeExamplesPath = resolveRepoPath("docs/CodeExamples.md");
  REQUIRE(std::filesystem::exists(readmePath));
  REQUIRE(std::filesystem::exists(codeExamplesPath));

  const std::string readme = readFile(readmePath.string());
  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::vector<std::string> requiredReadmeSnippets = {
      "Algebraic sum values:",
      "[sum]\nShape",
      "[Circle] explicit_circle_payload{[radius] 3}",
      "[Shape] explicit_shape{[circle] explicit_circle_payload}",
      "[Circle] inferred_circle_payload{[radius] 4}",
      "[Shape] inferred_shape{inferred_circle_payload}"};
  for (const std::string &snippet : requiredReadmeSnippets) {
    CAPTURE(snippet);
    CHECK(readme.find(snippet) != std::string::npos);
  }

  const std::vector<std::string> requiredCodeExamplesSnippets = {
      "### Sum Values with Exhaustive Pick",
      "[Circle] explicit_circle_payload{[radius] 3}",
      "[Shape] explicit_shape{[circle] explicit_circle_payload}",
      "[Rectangle] labeled_rectangle_payload{[width] 4, [height] 5}",
      "[Shape] labeled_shape{[rectangle] labeled_rectangle_payload}",
      "[Circle] inferred_circle_payload{[radius] 6}",
      "[Shape] inferred_shape{inferred_circle_payload}",
      "ambiguous inferred sum construction"};
  for (const std::string &snippet : requiredCodeExamplesSnippets) {
    CAPTURE(snippet);
    CHECK(codeExamples.find(snippet) != std::string::npos);
  }

  const std::string source = R"(
[struct]
Circle {
  [int] radius{0}
}

[struct]
Rectangle {
  [int] width{0}
  [int] height{0}
}

[sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
}

[int]
shape_score([Shape] shape) {
  return(pick(shape) {
    circle(c) {
      c.radius * c.radius
    }
    rectangle(r) {
      r.width * r.height
    }
  })
}

[int]
main() {
  [Circle] explicit_circle_payload{[radius] 3}
  [Rectangle] labeled_rectangle_payload{[width] 4, [height] 5}
  [Circle] inferred_circle_payload{[radius] 6}

  [Shape] explicit_shape{[circle] explicit_circle_payload}
  [Shape] labeled_shape{[rectangle] labeled_rectangle_payload}
  [Shape] inferred_shape{inferred_circle_payload}

  return(shape_score(explicit_shape) + shape_score(labeled_shape) + shape_score(inferred_shape))
}
)";
  const std::string srcPath = writeTemp("docs_sum_public_examples.prime", source);
  const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(runCmd) == 65);

  const std::string ambiguousSource = R"(
[struct]
Circle {
  [int] radius{0}
}

[sum]
AmbiguousShape {
  [Circle] primary
  [Circle] secondary
}

[int]
main() {
  [Circle] payload{[radius] 2}
  [AmbiguousShape] bad{payload}
  return(0)
}
)";
  const std::string ambiguousSrcPath = writeTemp("docs_sum_ambiguous_payload_negative.prime", ambiguousSource);
  const std::string errPath =
      (testScratchPath("") / "primec_docs_sum_ambiguous_payload_negative.err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + quoteShellArg(ambiguousSrcPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("ambiguous inferred sum construction") != std::string::npos);
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
  CHECK(readFile(errPath).find("unknown call target: push") != std::string::npos);
}

TEST_CASE("spinning cube shared source reflects current profile support") {
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
  const std::string wasmErrPath =
      (testScratchPath("") / "primec_spinning_cube_browser_smoke.err.txt").string();
  const std::string metalErrPath =
      (testScratchPath("") / "primec_spinning_cube_metal_smoke.err.txt").string();

  const std::string nativeMainCmd = "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o /dev/null --entry /main 2> " +
                                    quoteShellArg(nativeMainErrPath);
  CHECK(runCommand(nativeMainCmd) == 2);
  const std::string nativeMainErr = readFile(nativeMainErrPath);
  CHECK(nativeMainErr.find("native backend does not support return type on /cubeInit") !=
        std::string::npos);

  const std::string nativeCmd = "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
                                quoteShellArg(nativePath) + " --entry /mainNative";
  CHECK(runCommand(nativeCmd) == 0);
  CHECK(std::filesystem::exists(nativePath));

  const std::string wasmBrowserCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(wasmErrPath);
  CHECK(runCommand(wasmBrowserCmd) == 2);
  const std::string wasmErr = readFile(wasmErrPath);
  CHECK(wasmErr.find("graphics stdlib runtime substrate unavailable for wasm-browser target: /std/gfx/*") !=
        std::string::npos);
  CHECK_FALSE(std::filesystem::exists(wasmPath));

  const std::string metalCmd = "./primec --emit=metal " + quoteShellArg(cubePath.string()) +
                               " -o /dev/null --entry /main 2> " + quoteShellArg(metalErrPath);
  CHECK(runCommand(metalCmd) == 2);
  const std::string metalErr = readFile(metalErrPath);
  CHECK(metalErr.find("unknown option: --emit=metal") != std::string::npos);
  CHECK(metalErr.find("Usage: primec") != std::string::npos);
}

TEST_CASE("safe extent and cursor docs examples stay documented and executable") {
  // TODO-4612 added the first four verified examples below; TODO-5248
  // promoted "Optional Pointer" out of the "Proposed" sketches into a fifth
  // verified one. All five must stay byte-identical (modulo surrounding
  // markdown fences) with docs/CodeExamples.md's "Safe Extents And Cursor
  // Examples" section, and must keep compiling and returning the documented
  // result on both backends.
  auto resolveDocPath = [](const std::string &name) -> std::filesystem::path {
    std::filesystem::path path = std::filesystem::path("..") / "docs" / name;
    if (!std::filesystem::exists(path)) {
      path = std::filesystem::current_path() / "docs" / name;
    }
    return path;
  };

  const std::filesystem::path codeExamplesPath = resolveDocPath("CodeExamples.md");
  const std::filesystem::path safeExtentsPath = resolveDocPath("SafeArrayExtentViews.md");
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(safeExtentsPath));

  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::string safeExtentsDoc = readFile(safeExtentsPath.string());

  const std::vector<std::string> requiredCodeExampleSnippets = {
      "## Safe Extents And Cursor Examples",
      "### Runtime Extent Contract",
      "[return<int> require(count(values) > 0)]",
      "### Checked Slice Loop",
      "[array<i32>] window{slice(values, start, end)}",
      "### Forward Cursor Loop",
      "[Cursor<i32> mut] it{startVector<i32>(values)}",
      "### Reverse Cursor Loop",
      "[Cursor<i32> mut] it{reverseStartVector<i32>(values)}",
      "### Optional Pointer",
      "return(some<Pointer<i32>>(slot))",
      "### Proposed: Capability-Parameterized View",
      "[Reference<array<i32>, Read>] values"};
  for (const std::string &snippet : requiredCodeExampleSnippets) {
    CAPTURE(snippet);
    CHECK(codeExamples.find(snippet) != std::string::npos);
  }
  CHECK(safeExtentsDoc.find("read-only forward cursor (`Cursor<T>`, "
                            "matching the \"Cursors And Pointer\n"
                            "Arithmetic\" section below) is now implemented "
                            "for both `array<T>` and\n"
                            "`vector<T>`") != std::string::npos);

  auto runVmAndNative = [](const std::string &stem, const std::string &source,
                          int expectedExit) {
    CAPTURE(stem);
    const std::string srcPath = writeTemp(stem + ".prime", source);
    const std::string runVmCmd =
        "./primec --emit=vm " + srcPath + " --entry /main";
    CHECK(runCommand(runVmCmd) == expectedExit);

    const std::string nativePath = (testScratchPath("") / ("primec_" + stem)).string();
    const std::string compileNativeCmd =
        "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
    CHECK(runCommand(compileNativeCmd) == 0);
    CHECK(runCommand(nativePath) == expectedExit);
  };

  runVmAndNative("docs_safe_extents_require_contract", R"(
[return<int> require(count(values) > 0)]
first_value([array<i32>] values) {
  return(values[0i32])
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(41i32, 7i32)}
  return(first_value(values))
}
)", 41);

  runVmAndNative("docs_safe_extents_slice_loop", R"(
[return<int>]
sum_window([array<i32>] values, [i32] start, [i32] end) {
  [array<i32>] window{slice(values, start, end)}
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < count(window)) {
    total = total + window[i]
    i = i + 1i32
  }
  return(total)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32, 11i32)}
  return(sum_window(values, 1i32, 3i32))
}
)", 16);

  runVmAndNative("docs_safe_extents_forward_cursor", R"(
import /std/cursor/*

[return<int>]
sum_forward([vector<i32>] values) {
  [Cursor<i32> mut] it{startVector<i32>(values)}
  [Cursor<i32>] lim{limitVector<i32>(values)}
  [i32 mut] total{0i32}

  while(cursorNotEqual<i32>(it, lim)) {
    total = total + readVector<i32>(values, it)
    it = advance<i32>(it)
  }

  return(total)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32, 9i32, 11i32)}
  return(sum_forward(values))
}
)", 31);

  runVmAndNative("docs_safe_extents_reverse_cursor", R"(
import /std/collections/*
import /std/cursor/*

[return<int>]
sum_reverse([vector<i32>] values) {
  [Cursor<i32> mut] it{reverseStartVector<i32>(values)}
  [Cursor<i32>] lim{reverseLimitVector<i32>(values)}
  [i32 mut] total{0i32}

  while(cursorNotEqual<i32>(it, lim)) {
    total = total + readVector<i32>(values, it)
    it = retreat<i32>(it)
  }

  return(total)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32, 100i32)}
  return(sum_reverse(values))
}
)", 106);

  runVmAndNative("docs_safe_extents_optional_pointer", R"(
import /std/maybe/*

[return<Maybe<Pointer<i32>>> effects(heap_alloc)]
try_alloc_slot([i32] initial) {
  [mut] slot{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(slot), initial)
  return(some<Pointer<i32>>(slot))
}

[return<int> effects(heap_alloc)]
main() {
  [Maybe<Pointer<i32>>] slot{try_alloc_slot(42i32)}
  return(pick(slot) {
    none {
      return(-1i32)
    }
    some(ptr) {
      [i32] value{dereference(ptr)}
      /std/intrinsics/memory/free(ptr)
      return(value)
    }
  })
}
)", 42);
}

TEST_SUITE_END();
