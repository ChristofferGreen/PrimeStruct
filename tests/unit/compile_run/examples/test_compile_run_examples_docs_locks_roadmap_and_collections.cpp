#include "third_party/doctest.h"

#include "test_compile_run_examples_docs_locks_shared.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("contributor doctest guardrails stay source locked") {
  std::filesystem::path agentsPath = std::filesystem::path("..") / "AGENTS.md";
  if (!std::filesystem::exists(agentsPath)) {
    agentsPath = std::filesystem::current_path() / "AGENTS.md";
  }
  REQUIRE(std::filesystem::exists(agentsPath));

  const std::string agents = readFile(agentsPath.string());
  CHECK(agents.find("**Doctest size guardrail:**") != std::string::npos);
  CHECK(agents.find("beyond 10 `SUBCASE` blocks or equivalent subtests") != std::string::npos);
  CHECK(agents.find("multiple focused `TEST_CASE`s or suite shards") != std::string::npos);
  CHECK(agents.find("**Doctest runtime guardrail:**") != std::string::npos);
  CHECK(agents.find("multiple subcases takes more than 5 seconds") != std::string::npos);
  CHECK(agents.find("single-focus doctest still takes more than 5 seconds") != std::string::npos);
  CHECK(agents.find("optimize it or add a brief justification") != std::string::npos);
}

TEST_CASE("focused backend rerun helper stays documented") {
  std::filesystem::path agentsPath = std::filesystem::path("..") / "AGENTS.md";
  std::filesystem::path helperPath =
      std::filesystem::path("..") / "scripts" / "rerun_backend_shard.sh";
  if (!std::filesystem::exists(agentsPath)) {
    agentsPath = std::filesystem::current_path() / "AGENTS.md";
  }
  if (!std::filesystem::exists(helperPath)) {
    helperPath = std::filesystem::current_path() / "scripts" / "rerun_backend_shard.sh";
  }
  REQUIRE(std::filesystem::exists(agentsPath));
  REQUIRE(std::filesystem::exists(helperPath));

  const std::string agents = readFile(agentsPath.string());
  const std::string helper = readFile(helperPath.string());

  CHECK(agents.find("**Focused backend rerun helper:**") != std::string::npos);
  CHECK(agents.find("scripts/rerun_backend_shard.sh vm-math") != std::string::npos);
  CHECK(agents.find("`build-release/` cwd") != std::string::npos);
  CHECK(agents.find("`PrimeStruct_compile_run_tests` direct command") !=
        std::string::npos);
  CHECK(agents.find("add `--run` to execute the focused CTest") != std::string::npos);
  CHECK(helper.find("vm-math    VM math compile-run backend shard") != std::string::npos);
  CHECK(helper.find("Build cwd: $build_dir") != std::string::npos);
  CHECK(helper.find("Release binary: $binary") != std::string::npos);
  CHECK(helper.find("ctest_regex='^PrimeStruct_primestruct_compile_run_vm_math_math_helpers_'") !=
        std::string::npos);
  CHECK(helper.find("binary='PrimeStruct_compile_run_tests'") != std::string::npos);
  CHECK(helper.find("--test-suite=$suite --order-by=file") != std::string::npos);
  CHECK(helper.find("exec ctest -R \"$ctest_regex\" --output-on-failure") !=
        std::string::npos);
}

TEST_CASE("skipped doctest debt stays absent from unit shards") {
  const std::filesystem::path testsPath = resolveUnitTestsPath();
  REQUIRE(std::filesystem::exists(testsPath));

  const auto skippedFiles = filesWithRetainedDoctestSkips(testsPath);
  for (const auto &path : skippedFiles) {
    INFO("Retained doctest::skip(true) must map to an active TODO: " << path.string());
  }
  CHECK(skippedFiles.empty());
}

TEST_CASE("spinning cube native-window status avoids inactive TODO pointers") {
  std::filesystem::path readmePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "README.md";
  std::filesystem::path todoPath = std::filesystem::path("..") / "docs" / "todo.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  if (!std::filesystem::exists(readmePath)) {
    readmePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "README.md";
  }
  if (!std::filesystem::exists(todoPath)) {
    todoPath = std::filesystem::current_path() / "docs" / "todo.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  REQUIRE(std::filesystem::exists(readmePath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string readme = readFile(readmePath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(readme.find("The archived native-window roadmap has landed its v1 macOS host target") !=
        std::string::npos);
  CHECK(readme.find("add\n  a concrete TODO before tracking another native-window parity milestone") !=
        std::string::npos);
  CHECK(readme.find("tracked in `docs/todo.md` under `Native Windowed Spinning Cube (Roadmap)`") ==
        std::string::npos);
  CHECK(todo.find("Native Windowed Spinning Cube (Roadmap)") == std::string::npos);
  CHECK(todo.find("TODO-4188") == std::string::npos);
  CHECK(todoFinished.find("TODO-4188: Align spinning-cube roadmap docs") !=
        std::string::npos);
}

TEST_CASE("vector dynamic-storage docs lock completed first slice") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path syntaxSpecPath = std::filesystem::path("..") / "docs" / "PrimeStruct_SyntaxSpec.md";
  std::filesystem::path todoPath = std::filesystem::path("..") / "docs" / "todo.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  std::filesystem::path vmVectorLimitsPath =
      std::filesystem::path("..") / "tests" / "unit" / "compile_run" / "vm" / "test_compile_run_vm_collections_vector_limits_pop_shadow.cpp";
  std::filesystem::path nativeVectorLimitsPath =
      std::filesystem::path("..") / "tests" / "unit" / "compile_run" / "native_backend" / "test_compile_run_native_backend_collections_mutators_and_limits_auto_inferred.cpp";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(syntaxSpecPath)) {
    syntaxSpecPath = std::filesystem::current_path() / "docs" / "PrimeStruct_SyntaxSpec.md";
  }
  if (!std::filesystem::exists(todoPath)) {
    todoPath = std::filesystem::current_path() / "docs" / "todo.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  if (!std::filesystem::exists(vmVectorLimitsPath)) {
    vmVectorLimitsPath = std::filesystem::current_path() / "tests" / "unit" / "compile_run" / "vm" / "test_compile_run_vm_collections_vector_limits_pop_shadow.cpp";
  }
  if (!std::filesystem::exists(nativeVectorLimitsPath)) {
    nativeVectorLimitsPath =
        std::filesystem::current_path() / "tests" / "unit" / "compile_run" / "native_backend" / "test_compile_run_native_backend_collections_mutators_and_limits_auto_inferred.cpp";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));
  REQUIRE(std::filesystem::exists(vmVectorLimitsPath));
  REQUIRE(std::filesystem::exists(nativeVectorLimitsPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpecDoc = readFile(syntaxSpecPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());
  const std::string vmVectorLimits = readFile(vmVectorLimitsPath.string());
  const std::string nativeVectorLimits = readFile(nativeVectorLimitsPath.string());

  CHECK(primeStructDoc.find("VM/native now use a heap-backed\n    `count/capacity/data_ptr` record") !=
        std::string::npos);
  CHECK(primeStructDoc.find("push/reserve growth\n    reallocates backing storage while preserving existing elements") !=
        std::string::npos);
  CHECK(primeStructDoc.find("current deterministic `1024` local dynamic-capacity limit") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("VM/native now use heap-backed vector locals with a\n`count/capacity/data_ptr` record") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("deterministic local\ndynamic-capacity limit (`1024`)") !=
        std::string::npos);
  CHECK(vmVectorLimits.find("runs vm vector reserve growth through count and capacity helpers") !=
        std::string::npos);
  CHECK(vmVectorLimits.find("preserves vm vector values across push growth") !=
        std::string::npos);
  CHECK(nativeVectorLimits.find("grows native vector push beyond initial capacity") !=
        std::string::npos);
  CHECK(nativeVectorLimits.find("preserves native vector values across reserve growth") !=
        std::string::npos);
  CHECK(primeStructDoc.find("No active TODO currently tracks migration to dynamic storage") ==
        std::string::npos);
  CHECK(primeStructDoc.find("No active TODO currently tracks full dynamic vector") ==
        std::string::npos);
  CHECK(syntaxSpecDoc.find("No active TODO\ncurrently tracks full dynamic vector runtime parity") ==
        std::string::npos);
  CHECK(todo.find("TODO-4245") == std::string::npos);
  CHECK(todoFinished.find("TODO-4245: Plan dynamic vector growth and runtime storage support") !=
        std::string::npos);
  CHECK(todo.find("TODO-4281: Lift vector dynamic capacity limit") == std::string::npos);
  CHECK(todoFinished.find("TODO-4281: Lift vector dynamic capacity limit") !=
        std::string::npos);
  CHECK(todo.find("TODO-4189") == std::string::npos);
  CHECK(todo.find("TODO-4192") == std::string::npos);
  CHECK(todoFinished.find("TODO-4189: Align vector dynamic-storage docs") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4192: Align vector conformance TODO docs") !=
        std::string::npos);
}

TEST_CASE("semantic-product docs avoid inactive Group 12 pointers") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path todoPath = std::filesystem::path("..") / "docs" / "todo.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(todoPath)) {
    todoPath = std::filesystem::current_path() / "docs" / "todo.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(primeStructDoc.find("active queue no longer tracks Group 12 entrypoint retirement") !=
        std::string::npos);
  CHECK(primeStructDoc.find("add a concrete TODO before changing any of those seams") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The `primec` and `primevm` runtime entrypoints, plus focused post-semantics") !=
        std::string::npos);
  CHECK(primeStructDoc.find("success-only dump and conformance helpers") != std::string::npos);
  CHECK(primeStructDoc.find("`TODO-4241` tracks the remaining compatibility caller migration") ==
        std::string::npos);
  CHECK(primeStructDoc.find("semantic-product dump/report API boundary is now versioned") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`TODO-4228` tracks the semantic-product dump/report API factoring") ==
        std::string::npos);
  CHECK(primeStructDoc.find("Pipeline-facing/backend conformance now covers semantic-product facts beyond") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Missing collection-specialization facts for\n  collection bindings now fail closed") !=
        std::string::npos);
  CHECK(primeStructDoc.find("remaining live Group 12 work is now") == std::string::npos);
  CHECK(primeStructDoc.find("remaining CLI/runtime plumbing work is limited") == std::string::npos);
  CHECK(primeStructDoc.find("remaining live\n  Group 12 coverage work") == std::string::npos);
  CHECK(todo.find("TODO-4190") == std::string::npos);
  CHECK(todoFinished.find("TODO-4190: Align semantic-product Group 12 docs") !=
        std::string::npos);
}

TEST_CASE("reflection metadata docs avoid inactive roadmap pointers") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(primeStructDoc.find("execution semantics now evaluate at compile time") !=
        std::string::npos);
  CHECK(primeStructDoc.find("add a concrete reflection TODO before") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Query execution semantics are implemented in follow-up roadmap items") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4195: Align reflection metadata docs") !=
        std::string::npos);
}

TEST_CASE("result payload docs avoid inactive follow-up pointers") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(primeStructDoc.find("Native executable `Result<Buffer<T>, GfxError>` values preserve") !=
        std::string::npos);
  CHECK(primeStructDoc.find("add a concrete Result payload TODO before widening") !=
        std::string::npos);
  CHECK(primeStructDoc.find("remains follow-up work on IR-backed paths") ==
        std::string::npos);
  CHECK(primeStructDoc.find("unsupported wider payloads stay follow-up work") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4196: Align Result payload docs") !=
        std::string::npos);
}

TEST_CASE("graphics UI docs avoid inactive follow-up pointers") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path todoPath = std::filesystem::path("..") / "docs" / "todo.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(todoPath)) {
    todoPath = std::filesystem::current_path() / "docs" / "todo.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(primeStructDoc.find("No active TODO currently tracks broader backend/runtime package-path") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Add a concrete TODO before changing that graphics backend/runtime seam") !=
        std::string::npos);
  CHECK(primeStructDoc.find("No active TODO currently tracks platform/runtime consumption") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Add a\n"
                            "  concrete TODO before changing that UI runtime seam") !=
        std::string::npos);
  CHECK(primeStructDoc.find("broader backend/runtime follow-up work is still staged") ==
        std::string::npos);
  CHECK(primeStructDoc.find("planned follow-up layers now center") == std::string::npos);
  CHECK(todo.find("TODO-4191") == std::string::npos);
  CHECK(todoFinished.find("TODO-4191: Align graphics UI follow-up docs") !=
        std::string::npos);
}

TEST_CASE("coding guidelines avoid inactive surface status pointers") {
  std::filesystem::path codingGuidelinesPath = std::filesystem::path("..") / "docs" / "Coding_Guidelines.md";
  std::filesystem::path todoPath = std::filesystem::path("..") / "docs" / "todo.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  if (!std::filesystem::exists(codingGuidelinesPath)) {
    codingGuidelinesPath = std::filesystem::current_path() / "docs" / "Coding_Guidelines.md";
  }
  if (!std::filesystem::exists(todoPath)) {
    todoPath = std::filesystem::current_path() / "docs" / "todo.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  REQUIRE(std::filesystem::exists(codingGuidelinesPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string codingGuidelines = readFile(codingGuidelinesPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(codingGuidelines.find("treat `Maybe<T>`, `vector<T>`, `map<K, V>`, and `soa<T>` as") !=
        std::string::npos);
  CHECK(codingGuidelines.find("stdlib-owned surfaces") != std::string::npos);
  CHECK(codingGuidelines.find("No active TODO currently tracks broader backend") !=
        std::string::npos);
  CHECK(codingGuidelines.find("add a concrete gfx conformance TODO before changing") !=
        std::string::npos);
  CHECK(codingGuidelines.find("incubating `soa<T>`") == std::string::npos);
  CHECK(codingGuidelines.find("planned `soa<T>`") == std::string::npos);
  CHECK(codingGuidelines.find("broader backend conformance remains staged") ==
        std::string::npos);
  CHECK(todo.find("TODO-4193") == std::string::npos);
  CHECK(todoFinished.find("TODO-4193: Align coding guidelines TODO docs") !=
        std::string::npos);
}

TEST_CASE("stdlib style boundary docs stay source locked") {
  std::filesystem::path codeExamplesPath = std::filesystem::path("..") / "docs" / "CodeExamples.md";
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path agentsPath = std::filesystem::path("..") / "AGENTS.md";
  if (!std::filesystem::exists(codeExamplesPath)) {
    codeExamplesPath = std::filesystem::current_path() / "docs" / "CodeExamples.md";
  }
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(agentsPath)) {
    agentsPath = std::filesystem::current_path() / "AGENTS.md";
  }
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(agentsPath));

  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string agents = readFile(agentsPath.string());

  CHECK(codeExamples.find("## Stdlib Style Boundary") != std::string::npos);
  CHECK(codeExamples.find("Style-aligned surface code:") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/math/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/maybe/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/file/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/image/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/ui/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/scene/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/vector.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/map.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/errors.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/soa.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/soa_vector.prime`") ==
        std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/soa_conversions.prime`") ==
        std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/gfx/gfx.prime`") != std::string::npos);
  CHECK(codeExamples.find("Internal implementation, bridge, or substrate-oriented code:") != std::string::npos);
  CHECK(codeExamples.find("Intentionally canonical or substrate-oriented code:") == std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/bench_non_math/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/collections.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/experimental_vector.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/experimental_map.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/experimental_soa_vector.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/experimental_soa_vector_conversions.prime`") !=
        std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/internal_*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/gfx/experimental.prime`") != std::string::npos);
  CHECK(codeExamples.find("SoA public example rule: `soa<T>` is the promoted") !=
        std::string::npos);
  CHECK(codeExamples.find("type spelling for user-facing examples") !=
        std::string::npos);
  CHECK(codeExamples.find("`/std/collections/soa/*` for construction") !=
        std::string::npos);
  CHECK(codeExamples.find("Existing `soa_vector<T>` examples are") !=
        std::string::npos);
  CHECK(codeExamples.find("Retired SoA compatibility rule: direct imports of") !=
        std::string::npos);
  CHECK(codeExamples.find("`/std/collections/soa_vector*` and `/std/collections/experimental_soa_vector*`") !=
        std::string::npos);
  CHECK(codeExamples.find("are rejected compatibility spellings") !=
        std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections` is intentionally mixed") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/gfx` is intentionally mixed") != std::string::npos);

  CHECK(primeStructDoc.find("### Stdlib Surface-Style Boundary") != std::string::npos);
  CHECK(primeStructDoc.find("This boundary is the scope reference for the stdlib surface-style cleanup lane in") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/scene/*`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/vector.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/soa.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/collections.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/experimental_vector.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/experimental_map.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/experimental_soa_vector.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/experimental_soa_vector_conversions.prime`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/internal_*`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/gfx/experimental.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections` and `stdlib/std/gfx`") != std::string::npos);

  CHECK(agents.find("For stdlib style work, follow the exact file-level boundary in") !=
        std::string::npos);
  CHECK(agents.find("`stdlib/std/ui/*`, `stdlib/std/scene/*`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/soa.prime`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/collections.prime`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/experimental_vector.prime`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/experimental_map.prime`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/experimental_soa_vector.prime`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/experimental_soa_vector_conversions.prime`,") !=
        std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/internal_*`, and") != std::string::npos);
  CHECK(agents.find("`stdlib/std/gfx/experimental.prime` as internal, bridge, substrate, migration,") !=
        std::string::npos);
}

TEST_CASE("vector map bridge boundary docs stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path todoPath = std::filesystem::path("..") / "docs" / "todo.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(todoPath)) {
    todoPath = std::filesystem::current_path() / "docs" / "todo.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(primeStructDoc.find("### Vector/Map Bridge Contract") != std::string::npos);
  CHECK(primeStructDoc.find("**Bridge-owned public contract:** exact and wildcard `/std/collections`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Stdlib-owned surface metadata:** canonical vector and map\n"
                            "  helper/import/constructor metadata") !=
        std::string::npos);
  CHECK(primeStructDoc.find("TODO-4635 deleted\n"
                            "  `stdlib/std/collections/surfaces.psmeta`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Migration-only seams:** rooted `/map/*` spellings plus") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Rooted\n  `/vector/*` helper spellings no longer act as builtin vector compatibility") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The vector/map adapter cutover is complete") !=
        std::string::npos);
  CHECK(primeStructDoc.find("direct experimental vector source imports are rejected") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Compatibility adapter inventory:") !=
        std::string::npos);
  CHECK(primeStructDoc.find("map insert helper compatibility no\n"
                            "  longer lives in the central surface manifest") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Definition/execution intra-body diagnostics no longer carry special\n"
                            "  removed-map helper classification branches, and semantic pre-dispatch helper\n"
                            "  path candidates no longer mirror rooted and canonical map helpers") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Native tail\n"
                            "  map-access helper probes no longer count rooted `/map/*` imports or\n"
                            "  definitions as canonical map helper availability, and semantic helper-path\n"
                            "  preference no longer cross-resolves rooted `/map/*` and canonical\n"
                            "  `/std/collections/map/*` definitions, and semantic method resolution no\n"
                            "  longer treats explicit rooted `/map/*` method targets as canonical\n"
                            "  `/std/collections/map/*` helper calls, and inline/native dispatch no longer\n"
                            "  treats rooted `/map/*` or experimental map helper raw paths as canonical map\n"
                            "  helper aliases") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Template\n"
                            "  monomorphization still asks the registry for preferred experimental\n"
                            "  vector/SoA") !=
        std::string::npos);
  CHECK(primeStructDoc.find("SoA public helper, constructor,\n"
                            "  import-alias, field-view, and conversion metadata is derived from\n"
                            "  `[public]` stdlib declarations by `StdlibSurfaceRegistry`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("it no longer owns SoA public collection member lists") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Gfx Buffer helper compatibility is routed through\n"
                            "  `StdlibSurfaceRegistry::GfxBufferHelpers`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("later cutover TODOs delete them") == std::string::npos);
  CHECK(primeStructDoc.find("**Out of scope for this bridge lane:** `array<T>` core ownership,") !=
        std::string::npos);

  CHECK(todo.find("Do not keep completed-task summaries, historical rollout notes, or closed\n"
                  "  coverage snapshots in this file.") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4570: Retire duplicate map2 candidate surface") ==
        std::string::npos);
  CHECK(todo.find("map2 replacement candidate") == std::string::npos);
  CHECK(todo.find("- TODO-4571: Add compiler-knowledge inventory for map/vector | track: collection-audit") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4572: Remove vector statement-helper compiler path | track: vector-special-case-deletion") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4573: Remove compiler-owned map literal lowering | track: map-special-case-deletion") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4575: Remove map helper/access compiler classifiers | track: map-special-case-deletion") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4574: Remove vector count/access compiler classifiers | track: vector-helper-classifier-deletion") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4600: Migrate IR lowerer collection surface lookups | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4600: Migrate IR lowerer collection surface lookups") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4602: Remove semantic vector-literal compiler traces") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4577: Remove vector backing-type compiler classification | track: "
                  "vector-backing-classifier-deletion") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4577: Remove vector backing-type compiler classification") ==
        std::string::npos);
  CHECK(todo.find("Map/vector compiler-independence: TODO-4570 retired the duplicate `map2`\n"
                  "  surface, TODO-4571 added the compiler-knowledge inventory categories") !=
        std::string::npos);
  CHECK(todo.find("TODO-4574: Remove vector count/access compiler classifiers") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4574: Remove vector count/access compiler classifiers") !=
        std::string::npos);
  CHECK(todo.find("### Vector/Map Bridge Contract Summary") == std::string::npos);
  CHECK(todo.find("later cutover TODOs retire them") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4042:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4043:") == std::string::npos);
  CHECK(todo.find("TODO-4044") == std::string::npos);
  CHECK(todo.find("TODO-4187") == std::string::npos);
  CHECK(todoFinished.find("TODO-4570: Retire duplicate map2 candidate surface") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4571: Add compiler-knowledge inventory for map/vector") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4573: Remove compiler-owned map literal lowering") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4575: Remove map helper/access compiler classifiers") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4187: Align vector-map cutover docs") !=
        std::string::npos);
}

TEST_CASE("stdlib de-experimentalization policy docs stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path todoPath = std::filesystem::path("..") / "docs" / "todo.md";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(todoPath)) {
    todoPath = std::filesystem::current_path() / "docs" / "todo.md";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(todoPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string todo = readFile(todoPath.string());

  CHECK(primeStructDoc.find("### Stdlib De-Experimentalization Policy") != std::string::npos);
  CHECK(primeStructDoc.find("Canonical public API:") != std::string::npos);
  CHECK(primeStructDoc.find("Temporary compatibility namespace:") != std::string::npos);
  CHECK(primeStructDoc.find("Internal substrate/helper namespace:") != std::string::npos);
  CHECK(primeStructDoc.find("Canonical `/std/collections/vector/*` is now the sole public namespaced vector contract.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Canonical `/std/collections/map/*` is now the sole public namespaced map contract.") !=
        std::string::npos);
  CHECK(primeStructDoc.find(
            "`/std/collections/vector/*` family owns the internal backing adapter behind that") !=
        std::string::npos);
  CHECK(primeStructDoc.find(
            "`/std/collections/experimental_map/*` is\n"
            "  rejected as a source import and remains only as a legacy forwarding shim") !=
        std::string::npos);
  CHECK(primeStructDoc.find("no `experimental` namespace counts as canonical public API") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/vector/*` | Internal substrate/helper namespace | Internal vector backing adapter used by canonical `/std/collections/vector/*`; it preserves the current compatibility `Vector<T>` type identity until the final vector surface audit. | TODO-4373 |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/experimental_vector/*` | Rejected compatibility namespace | Direct source imports are rejected; the shim remains only as legacy forwarding storage identity behind `/std/collections/vector/*` until the final vector surface audit. | TODO-4373 |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/experimental_map/*` | Rejected compatibility namespace | Direct source imports are rejected; the shim remains only as legacy forwarding storage identity behind `/std/collections/map/*` until the final map surface audit. | TODO-4464 |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/gfx/experimental/*` | Temporary compatibility namespace | Legacy compatibility shim over canonical `/std/gfx/*`; no longer part of the public gfx contract and retained only for targeted compatibility coverage while the residual seam remains importable. | none |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/buffer_checked/*` | Internal substrate/helper namespace | Explicitly internal checked buffer plumbing for container conformance and memory-wrapper flows, not a stable user-facing stdlib API. Renamed from `internal_buffer_checked` in TODO-4634. | none |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Canonical and bare statement calls to vector mutators such\n"
                            "    as `push`, `pop`, `reserve`, `clear`, `remove_at`, and `remove_swap`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("resolve through visible `.prime` helper definitions and deterministic\n"
                            "    missing-import diagnostics instead of a compiler-owned vector\n"
                            "    statement-helper emitter") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Wrapper-layer\n"
                            "    `/std/collections/vectorPush`-style mutator aliases are removed from the") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Direct `/std/collections/experimental_vector/vectorPush`-style source\n"
                            "    imports now fail with an import diagnostic") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Experimental\n"
                            "    `Vector<T>.set_field_count` and\n"
                            "    `set_field_capacity` statement calls plus `field_count`, `field_capacity`,") !=
        std::string::npos);

  CHECK(todo.find("### Stdlib De-Experimentalization Policy Summary") == std::string::npos);
  CHECK(todo.find("## Open Tasks") != std::string::npos);
  CHECK(todo.find("- TODO-4570: Retire duplicate map2 candidate surface") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4571: Add compiler-knowledge inventory for map/vector") ==
        std::string::npos);
  CHECK(todo.find("Stdlib de-experimentalization: TODO-4059") == std::string::npos);
  CHECK(todo.find("TODO-4103: Rename the remaining experimental SoA storage namespace") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4059:") == std::string::npos);
  CHECK(todo.find("Legacy gfx compatibility seam: `/std/gfx/experimental/*` remains importable") ==
        std::string::npos);
  CHECK(todo.find("Accepted temporary compatibility namespace:") == std::string::npos);
  CHECK(todo.find("`/std/collections/soa*` and `/std/collections/experimental_soa*`") ==
        std::string::npos);
  CHECK(todo.find("SoA compatibility shim: direct") == std::string::npos);
  CHECK(todo.find("imports are rejected; canonical public code uses `/std/collections/soa/*`") ==
        std::string::npos);
  CHECK(todo.find("`/std/collections/internal_soa_conversions/*`,") ==
        std::string::npos);
  CHECK(todo.find("no active\n  TODO currently targets them") ==
        std::string::npos);
  CHECK(todo.find("until their explicit shim, rename, or maturity TODOs land") ==
        std::string::npos);
  CHECK(todo.find("/std/collections/internal_buffer_checked/*`,") == std::string::npos);
  CHECK(todo.find("/std/collections/internal_buffer_unchecked/*`,") == std::string::npos);
  CHECK(todo.find("/std/collections/soa_storage/*` are implementation-facing") ==
        std::string::npos);
  CHECK(todo.find("accepted compatibility exception explicitly") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4103:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4052:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4053:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4054:") == std::string::npos);
  CHECK(todo.find("TODO-4055") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4056:") == std::string::npos);
}

TEST_CASE("generic contiguous buffer substrate docs and coverage stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path checkedPointerHelpersPath =
      std::filesystem::path("..") / "tests" / "unit" / "compile_run" /
      "test_compile_run_checked_pointer_conformance_helpers.h";
  std::filesystem::path nativeCompatTestPath =
      std::filesystem::path("..") / "tests" / "unit" / "compile_run" / "native_backend" / "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(checkedPointerHelpersPath)) {
    checkedPointerHelpersPath = std::filesystem::current_path() / "tests" / "unit" / "compile_run" /
                                "test_compile_run_checked_pointer_conformance_helpers.h";
  }
  if (!std::filesystem::exists(nativeCompatTestPath)) {
    nativeCompatTestPath = std::filesystem::current_path() / "tests" / "unit" / "compile_run" / "native_backend" / "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(checkedPointerHelpersPath));
  REQUIRE(std::filesystem::exists(nativeCompatTestPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string checkedPointerHelpers = readFile(checkedPointerHelpersPath.string());
  const std::string vmCompatTest = readRepoShardsConcat(
      resolveRepoPath(std::filesystem::path("tests") / "unit" / "compile_run" / "vm"),
      "test_compile_run_vm_collections_wrapper_temporaries_reject_count");
  const std::string nativeCompatTest = readFile(nativeCompatTestPath.string());

  CHECK(primeStructDoc.find("VM/native conformance now also covers a non-vector") !=
        std::string::npos);
  CHECK(primeStructDoc.find("fixture that allocates raw slots, initializes values with `init(...)`, moves a") !=
        std::string::npos);
  CHECK(primeStructDoc.find("dynamic prefix between two buffers with `take(...)` plus `init(...)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`pointer index out of bounds` rather than vector-specific diagnostics") !=
        std::string::npos);
  CHECK(checkedPointerHelpers.find("makeCheckedPointerUninitializedPrefixMoveSource") !=
        std::string::npos);
  CHECK(checkedPointerHelpers.find("move_prefix([Pointer<uninitialized<Token>> mut] dst,") !=
        std::string::npos);
  CHECK(checkedPointerHelpers.find("init(dereference(dstSlot), take(dereference(srcSlot)))") !=
        std::string::npos);
  CHECK(checkedPointerHelpers.find("[Reference<Token>] borrowed{borrow(dereference(token_slot(dst, 1i32)))}") !=
        std::string::npos);
  CHECK(checkedPointerHelpers.find("expectCheckedPointerUninitializedOutOfBoundsConformance") !=
        std::string::npos);
  CHECK(vmCompatTest.find("expectCheckedPointerUninitializedPrefixMoveConformance(\"vm\")") !=
        std::string::npos);
  CHECK(nativeCompatTest.find("expectCheckedPointerUninitializedPrefixMoveConformance(\"native\")") !=
        std::string::npos);
}

TEST_CASE("soa public collection docs stay source locked") {
  std::filesystem::path codeExamplesPath = std::filesystem::path("..") / "docs" / "CodeExamples.md";
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path syntaxSpecPath = std::filesystem::path("..") / "docs" / "PrimeStruct_SyntaxSpec.md";
  std::filesystem::path todoPath = std::filesystem::path("..") / "docs" / "todo.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  std::filesystem::path experimentalSoaVectorPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_soa.prime";
  std::filesystem::path soaPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "soa.prime";
  std::filesystem::path soaExamplePath =
      std::filesystem::path("..") / "examples" / "3.Surface" / "soa_ecs.prime";
  std::filesystem::path cppCompatTestPath =
      std::filesystem::path("..") / "tests" / "unit" / "compile_run" / "imports" / "test_compile_run_imports_operations.cpp";
  std::filesystem::path nativeCompatTestPath =
      std::filesystem::path("..") / "tests" / "unit" / "compile_run" / "native_backend" / "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp";
  if (!std::filesystem::exists(codeExamplesPath)) {
    codeExamplesPath = std::filesystem::current_path() / "docs" / "CodeExamples.md";
  }
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(syntaxSpecPath)) {
    syntaxSpecPath = std::filesystem::current_path() / "docs" / "PrimeStruct_SyntaxSpec.md";
  }
  if (!std::filesystem::exists(todoPath)) {
    todoPath = std::filesystem::current_path() / "docs" / "todo.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  if (!std::filesystem::exists(experimentalSoaVectorPath)) {
    experimentalSoaVectorPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "experimental_soa.prime";
  }
  if (!std::filesystem::exists(soaPath)) {
    soaPath = std::filesystem::current_path() / "stdlib" / "std" / "collections" / "soa.prime";
  }
  if (!std::filesystem::exists(soaExamplePath)) {
    soaExamplePath = std::filesystem::current_path() / "examples" / "3.Surface" / "soa_ecs.prime";
  }
  if (!std::filesystem::exists(cppCompatTestPath)) {
    cppCompatTestPath = std::filesystem::current_path() / "tests" / "unit" / "compile_run" / "imports" / "test_compile_run_imports_operations.cpp";
  }
  if (!std::filesystem::exists(nativeCompatTestPath)) {
    nativeCompatTestPath = std::filesystem::current_path() / "tests" / "unit" / "compile_run" / "native_backend" / "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp";
  }
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));
  // experimental_soa.prime retired and merged into soa.prime (TODO-4633)
  CHECK(!std::filesystem::exists(experimentalSoaVectorPath));
  REQUIRE(std::filesystem::exists(soaPath));
  REQUIRE(std::filesystem::exists(soaExamplePath));
  REQUIRE(std::filesystem::exists(cppCompatTestPath));
  REQUIRE(std::filesystem::exists(nativeCompatTestPath));

  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpecDoc = readFile(syntaxSpecPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());
  const std::string soaStdlib = readFile(soaPath.string());
  const std::string soaExample = readFile(soaExamplePath.string());
  const std::string cppCompatTest = readFile(cppCompatTestPath.string());
  const std::string vmCompatTest = readRepoShardsConcat(
      resolveRepoPath(std::filesystem::path("tests") / "unit" / "compile_run" / "vm"),
      "test_compile_run_vm_collections_wrapper_temporaries_reject_count");
  const std::string nativeCompatTest = readFile(nativeCompatTestPath.string());

  CHECK(primeStructDoc.find("### SoA Public Collection Contract") != std::string::npos);
  CHECK(primeStructDoc.find("`soa<T>` is the promoted stdlib-owned public collection spelling") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Current user-facing surface:** `/std/collections/soa/*`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`soa_vector<T>` and direct experimental SoA imports are rejected") !=
        std::string::npos);
  CHECK(primeStructDoc.find("type spelling and normalizes onto the existing SoA backing identity") !=
        std::string::npos);
  CHECK(primeStructDoc.find("One source-locked wildcard canonical\n  parity program runs") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`examples/3.Surface/soa_ecs.prime`") !=
        std::string::npos);
  // experimental_soa* rows updated to "Retired" after TODO-4633 merge
  CHECK(primeStructDoc.find("| `/std/collections/experimental_soa_vector/*` | Retired compatibility namespace |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Retired and merged into `/std/collections/soa/*` (TODO-4633)") !=
        std::string::npos);
  // internal_soa* rows removed after TODO-4633 merge
  CHECK(primeStructDoc.find("| `/std/collections/internal_soa/*` |") ==
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/internal_soa_conversions/*` |") ==
        std::string::npos);
  // soa_storage renamed to soa_storage after TODO-4633
  CHECK(primeStructDoc.find("/std/collections/soa_storage/*") != std::string::npos);
  CHECK(primeStructDoc.find("This section is the scope reference for the promoted `soa<T>` public") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Focused rejection tests keep their diagnostics stable") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Rejected compatibility seams:** `/std/collections/soa_vector*`") !=
        std::string::npos);
  // Internal substrate section updated to reflect TODO-4633 merge
  CHECK(primeStructDoc.find("**Internal substrate:**") != std::string::npos);
  CHECK(primeStructDoc.find("merged from `internal_soa_vector`") != std::string::npos);
  CHECK(primeStructDoc.find("**Internal substrate namespaces:** `/std/collections/internal_soa/*`") ==
        std::string::npos);
  CHECK(primeStructDoc.find("**Promoted contract:** public behavior is owned by canonical stdlib surfaces") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Construction, read/ref, mutator, and field-view helpers are spelled through") !=
        std::string::npos);
  CHECK(primeStructDoc.find("AoS/SoA conversions are spelled through") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Borrowed `ref(...)` values, field views, structural mutation") !=
        std::string::npos);
  CHECK(primeStructDoc.find("C++ emitter, VM, and native coverage exercise the same canonical") !=
        std::string::npos);
  CHECK(primeStructDoc.find("hidden raw-builtin behavior") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Promotion blockers:** before final promotion") ==
        std::string::npos);
  CHECK(primeStructDoc.find("TODO-4252") == std::string::npos);
  CHECK(primeStructDoc.find("Ordinary public code should not import either experimental SoA namespace") ==
        std::string::npos);
  CHECK(primeStructDoc.find("add a separate concrete SoA cleanup TODO before changing behavior outside that scope") ==
        std::string::npos);
  CHECK(primeStructDoc.find("`TODO-4250`") == std::string::npos);
  CHECK(primeStructDoc.find("`TODO-4251`") == std::string::npos);
  CHECK(primeStructDoc.find("SoA promotion tasks still track receiver ownership") ==
        std::string::npos);
  CHECK(primeStructDoc.find("tie that state to a follow-up TODO") == std::string::npos);
  CHECK(primeStructDoc.find("are now tracked as separate cleanup follow-ups") ==
        std::string::npos);
  CHECK(primeStructDoc.find("none active; add a concrete TODO only before retiring, accepting, or reclassifying") ==
        std::string::npos);

  CHECK(todo.find("### SoA Public Collection Summary") == std::string::npos);
  CHECK(todo.find("Rename direction: `soa<T>` is retired as the public collection") ==
        std::string::npos);
  CHECK(todo.find("/std/collections/soa*`, rooted") == std::string::npos);
  CHECK(todo.find("Retired compatibility spellings are `soa<T>`") ==
        std::string::npos);
  CHECK(todo.find("Rejection seams: C++/VM/native tests lock the direct-import rejection") ==
        std::string::npos);
  CHECK(todo.find("Internal substrate namespaces: `/std/collections/internal_soa/*`") ==
        std::string::npos);
  CHECK(todo.find("owns canonical wrapper implementation forwarding") ==
        std::string::npos);
  CHECK(todo.find("`/std/collections/internal_soa_conversions/*` owns canonical") ==
        std::string::npos);
  CHECK(todo.find("Promoted contract complete: the canonical public helper wrapper is") ==
        std::string::npos);
  CHECK(todo.find("construction/read/ref/mutator/conversion helper") ==
        std::string::npos);
  CHECK(todo.find("hidden raw fallbacks") == std::string::npos);
  CHECK(todo.find("`TODO-4250` through `TODO-4252`") == std::string::npos);
  CHECK(todo.find("raw-builtin bridge\n  normalization, parity coverage") ==
        std::string::npos);
  CHECK(todo.find("Promotion requires borrowed-view/lifetime rules, backend/runtime parity") ==
        std::string::npos);
  CHECK(todo.find("code no longer needs `experimental_soa`") ==
        std::string::npos);
  CHECK(todo.find("`soa<T>` remains an\n  incubating canonical experiment") ==
        std::string::npos);
  CHECK(todo.find("The canonical wrapper routes through\n  `/std/collections/internal_soa/*`") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4058:") == std::string::npos);
  CHECK(todo.find("TODO-4103") == std::string::npos);
  CHECK(todo.find("TODO-4059") == std::string::npos);
  CHECK(todo.find("TODO-4181") == std::string::npos);
  CHECK(todo.find("TODO-4182") == std::string::npos);
  CHECK(todo.find("TODO-4185") == std::string::npos);
  CHECK(todo.find("TODO-4186") == std::string::npos);
  CHECK(todo.find("TODO-4244") == std::string::npos);
  CHECK(todoFinished.find("TODO-4244: Decide the `soa_vector` maturity exit") !=
        std::string::npos);
  CHECK(todo.find("TODO-4246") == std::string::npos);
  CHECK(todoFinished.find("TODO-4246: Define final `soa_vector` promotion contract") !=
        std::string::npos);
  CHECK(todo.find("TODO-4247") == std::string::npos);
  CHECK(todoFinished.find("TODO-4247: Move canonical SoA wrapper off experimental implementation imports") !=
        std::string::npos);
  CHECK(todo.find("TODO-4248") == std::string::npos);
  CHECK(todoFinished.find("TODO-4248: Move canonical SoA conversions off experimental conversion imports") !=
        std::string::npos);
  CHECK(todo.find("TODO-4249") == std::string::npos);
  CHECK(todoFinished.find("TODO-4249: Retire direct experimental SoA public imports") !=
        std::string::npos);
  CHECK(todo.find("TODO-4250") == std::string::npos);
  CHECK(todoFinished.find("TODO-4250: Normalize raw builtin `soa_vector` bridges onto canonical wrappers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4251") == std::string::npos);
  CHECK(todoFinished.find("TODO-4251: Add full cross-backend SoA parity coverage") !=
        std::string::npos);
  CHECK(todo.find("TODO-4252") == std::string::npos);
  CHECK(todoFinished.find("TODO-4252: Promote `soa_vector` docs after compatibility cleanup") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4185: Align SoA compatibility follow-up docs") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4186: Align SoA TODO summary wording") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4194: Align SoA compiler-cleanup docs") !=
        std::string::npos);

  CHECK(syntaxSpecDoc.find("The current public spelling is the canonical") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("/std/collections/soa_vector_conversions/*") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("direct experimental SoA imports are rejected compatibility spellings") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("routes\ncanonical construction, read/ref, field-view, mutation, and conversion helper") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("incubating canonical experiment") ==
        std::string::npos);
  CHECK(codeExamples.find("SoA public example rule: `soa<T>` is the promoted") !=
        std::string::npos);
  CHECK(codeExamples.find("Retired SoA compatibility rule: direct imports of") !=
        std::string::npos);
  CHECK(codeExamples.find("are rejected compatibility spellings") !=
        std::string::npos);
  CHECK(soaExample.find("import /std/collections/soa/*") != std::string::npos);
  CHECK(soaExample.find("import /std/collections/soa_conversions/*") ==
        std::string::npos);
  CHECK(soaExample.find("import /std/collections/experimental_soa/*") ==
        std::string::npos);
  CHECK(soaExample.find("import /std/collections/experimental_soa_conversions/*") ==
        std::string::npos);
  // experimental_soa.prime retired (TODO-4633): check merged soa.prime content instead
  CHECK(soaStdlib.find("// Canonical standalone SoA module with merged implementation.") !=
        std::string::npos);
  CHECK(soaStdlib.find("import /std/collections/soa_storage/*") != std::string::npos);
  CHECK(soaStdlib.find("import /std/collections/internal_soa") == std::string::npos);
  CHECK(soaStdlib.find("import /std/collections/experimental_soa") == std::string::npos);
  CHECK(cppCompatTest.find("TEST_CASE(\"rejects experimental soa stdlib helpers in C++ emitter\")") !=
        std::string::npos);
  CHECK(cppCompatTest.find("import /std/collections/experimental_soa/*") !=
        std::string::npos);
  CHECK(vmCompatTest.find("TEST_CASE(\"rejects vm experimental soa stdlib helpers\")") !=
        std::string::npos);
  CHECK(vmCompatTest.find("import /std/collections/experimental_soa/*") !=
        std::string::npos);
  CHECK(nativeCompatTest.find("TEST_CASE(\"rejects native experimental soa stdlib helpers\")") !=
        std::string::npos);
  CHECK(nativeCompatTest.find("import /std/collections/experimental_soa/*") !=
        std::string::npos);
}

TEST_CASE("legacy soa compatibility rejection matrix stays source locked") {
  const std::filesystem::path cppParityPath = resolveRepoPath(
      std::filesystem::path("tests") / "unit" / "compile_run" / "imports" / "test_compile_run_imports_operations.cpp");
  const std::filesystem::path nativeParityPath = resolveRepoPath(
      std::filesystem::path("tests") / "unit" / "compile_run" / "native_backend" / "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp");
  REQUIRE(std::filesystem::exists(cppParityPath));
  REQUIRE(std::filesystem::exists(nativeParityPath));

  const std::string cppParity = readFile(cppParityPath.string());
  const std::string vmParity = readRepoShardsConcat(
      resolveRepoPath(std::filesystem::path("tests") / "unit" / "compile_run" / "vm"),
      "test_compile_run_vm_collections_wrapper_temporaries_reject_count");
  const std::string nativeParity = readFile(nativeParityPath.string());
  const std::string parityProgram = R"(import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{soaVectorNew<Particle>()}
  reserve(values, 2i32)
  push(values, Particle(4i32, 6i32))
  push(values, Particle(9i32, 11i32))
  [Particle] first{get(values, 0i32)}
  [Reference<Particle>] second{ref(values, 1i32)}
  [vector<Particle>] unpacked{to_aos(values)}
  return(plus(plus(count(values), plus(first.x, second.x)), count(unpacked)))
})";

  CHECK(parityProgram.find("experimental_soa") == std::string::npos);
  CHECK(parityProgram.find("soaVectorNew<Particle>()") != std::string::npos);
  CHECK(parityProgram.find("reserve(values, 2i32)") != std::string::npos);
  CHECK(parityProgram.find("push(values, Particle(4i32, 6i32))") != std::string::npos);
  CHECK(parityProgram.find("get(values, 0i32)") != std::string::npos);
  CHECK(parityProgram.find("ref(values, 1i32)") != std::string::npos);
  CHECK(parityProgram.find("to_aos(values)") != std::string::npos);

  // Note: unlike native (still rejects), the vm/exe paths for this exact
  // program were re-pinned this session to their verified current
  // behavior (they now run successfully, exit 17) rather than rejecting -
  // see docs/todo.md's TODO-4741-adjacent history for
  // test_compile_run_imports_operations.cpp and
  // test_compile_run_vm_collections_wrapper_temporaries_reject_count.cpp.
  CHECK(cppParity.find(
            "TEST_CASE(\"runs legacy soa compatibility helpers in C++ emitter\")") !=
        std::string::npos);
  const std::size_t cppParityProgramOffset = cppParity.find(parityProgram);
  CHECK(cppParityProgramOffset != std::string::npos);
  CHECK(cppParity.find("CHECK(runCommand(compileCmd) == 17);", cppParityProgramOffset) !=
        std::string::npos);

  CHECK(vmParity.find("TEST_CASE(\"vm runs legacy soa compatibility helpers\")") !=
        std::string::npos);
  const std::size_t vmParityProgramOffset = vmParity.find(parityProgram);
  CHECK(vmParityProgramOffset != std::string::npos);
  CHECK(vmParity.find("CHECK(runCommand(runCmd) == 17);", vmParityProgramOffset) !=
        std::string::npos);

  CHECK(nativeParity.find("TEST_CASE(\"native legacy soa compatibility helpers reject\")") !=
        std::string::npos);
  const std::size_t nativeParityProgramOffset = nativeParity.find(parityProgram);
  CHECK(nativeParityProgramOffset != std::string::npos);
  CHECK(nativeParity.find("CHECK(runCommand(compileCmd) == 2);", nativeParityProgramOffset) !=
        std::string::npos);
  CHECK(nativeParity.find("meta.field_count requires struct type argument: type:Particle", nativeParityProgramOffset) !=
        std::string::npos);
}

TEST_CASE("soa compatibility fixture migration boundary stays source locked") {
  const std::filesystem::path testsPath = resolveUnitTestsPath();
  const std::filesystem::path examplesPath = resolveRepoPath("examples");
  const std::filesystem::path cppParityPath = resolveRepoPath(
      std::filesystem::path("tests") / "unit" / "compile_run" / "imports" / "test_compile_run_imports_operations.cpp");
  const std::filesystem::path nativeParityPath = resolveRepoPath(
      std::filesystem::path("tests") / "unit" / "compile_run" / "native_backend" / "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp");
  REQUIRE(std::filesystem::exists(testsPath));
  REQUIRE(std::filesystem::exists(examplesPath));
  REQUIRE(std::filesystem::exists(cppParityPath));
  REQUIRE(std::filesystem::exists(nativeParityPath));

  const auto violations = directOldSoaImportFixtureViolations(testsPath);
  for (const std::string &violation : violations) {
    INFO("Direct old SoA import must stay in an explicitly named rejection fixture: "
         << violation);
  }
  CHECK(violations.empty());

  for (const auto &entry : std::filesystem::recursive_directory_iterator(examplesPath)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".prime") {
      continue;
    }
    const std::string source = readFile(entry.path().string());
    INFO("Public examples should not use old experimental SoA imports: "
         << entry.path().lexically_relative(examplesPath).generic_string());
    CHECK(source.find("import /std/collections/experimental_soa") ==
          std::string::npos);
  }

  const std::string cppParity = readFile(cppParityPath.string());
  const std::string vmParity = readRepoShardsConcat(
      resolveRepoPath(std::filesystem::path("tests") / "unit" / "compile_run" / "vm"),
      "test_compile_run_vm_collections_wrapper_temporaries_reject_count");
  const std::string nativeParity = readFile(nativeParityPath.string());
  CHECK(cppParity.find("TEST_CASE(\"public soa count helper") !=
        std::string::npos);
  CHECK(cppParity.find("TEST_CASE(\"canonical soa count helper") ==
        std::string::npos);
  CHECK(cppParity.find("TEST_CASE(\"public soa to_aos explicit helper is a vector target") !=
        std::string::npos);
  CHECK(vmParity.find("TEST_CASE(\"runs vm public soa count helper") !=
        std::string::npos);
  CHECK(vmParity.find("TEST_CASE(\"runs vm canonical soa count helper") ==
        std::string::npos);
  CHECK(nativeParity.find("TEST_CASE(\"native public soa count helper") !=
        std::string::npos);
  CHECK(nativeParity.find(
            "TEST_CASE(\"native canonical soa count helper") ==
        std::string::npos);
}

TEST_CASE("arg-pack docs do not point at inactive TODO slices") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path syntaxSpecPath = std::filesystem::path("..") / "docs" / "PrimeStruct_SyntaxSpec.md";
  std::filesystem::path todoPath = std::filesystem::path("..") / "docs" / "todo.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(syntaxSpecPath)) {
    syntaxSpecPath = std::filesystem::current_path() / "docs" / "PrimeStruct_SyntaxSpec.md";
  }
  if (!std::filesystem::exists(todoPath)) {
    todoPath = std::filesystem::current_path() / "docs" / "todo.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpecDoc = readFile(syntaxSpecPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(primeStructDoc.find("backend/runtime materialization remains partial") !=
        std::string::npos);
  CHECK(primeStructDoc.find("should get a new explicit TODO before further implementation") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Newly discovered unsupported non-string pack gaps") !=
        std::string::npos);
  CHECK(primeStructDoc.find("concrete TODOs only when backed by a reproducible") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("materialization is partial; add a concrete TODO only") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("newly reproduced unsupported element kind") !=
        std::string::npos);
  CHECK(primeStructDoc.find("follow-up arg-pack TODO slice below") ==
        std::string::npos);
  CHECK(primeStructDoc.find("Borrowed/pointer `Result` packs and the remaining unsupported") ==
        std::string::npos);
  CHECK(primeStructDoc.find("remaining unsupported\n    non-string packs stay follow-up work") ==
        std::string::npos);
  CHECK(syntaxSpecDoc.find("materialization remains follow-up work beyond") ==
        std::string::npos);
  CHECK(todo.find("arg-pack") == std::string::npos);
  CHECK(todo.find("TODO-4183") == std::string::npos);
  CHECK(todo.find("TODO-4184") == std::string::npos);
  CHECK(todoFinished.find("TODO-4183: Remove stale arg-pack TODO wording") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4184: Align arg-pack remaining-gap docs") !=
        std::string::npos);
}

TEST_CASE("generic soa substrate boundary stays source locked") {
  const std::filesystem::path primeStructPath =
      resolveRepoPath(std::filesystem::path("docs") / "PrimeStruct.md");
  const std::filesystem::path syntaxSpecPath =
      resolveRepoPath(std::filesystem::path("docs") / "PrimeStruct_SyntaxSpec.md");
  const std::filesystem::path todoPath =
      resolveRepoPath(std::filesystem::path("docs") / "todo.md");
  const std::filesystem::path todoFinishedPath =
      resolveRepoPath(std::filesystem::path("docs") / "todo_finished.md");
  // internal_soa_storage.prime renamed to soa_storage.prime (TODO-4633)
  const std::filesystem::path internalStoragePath =
      resolveRepoPath(std::filesystem::path("stdlib") / "std" / "collections" /
                      "internal_soa_storage.prime");
  const std::filesystem::path soaStoragePath =
      resolveRepoPath(std::filesystem::path("stdlib") / "std" / "collections" /
                      "soa_storage.prime");
  const std::filesystem::path reflectionRuntimePath =
      resolveRepoPath(std::filesystem::path("tests") / "unit" /
                      "compile_run" / "test_compile_run_reflection_codegen_runtime.cpp");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));
  CHECK(!std::filesystem::exists(internalStoragePath));
  REQUIRE(std::filesystem::exists(soaStoragePath));
  REQUIRE(std::filesystem::exists(reflectionRuntimePath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpecDoc = readFile(syntaxSpecPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());
  const std::string internalStorage = readFile(soaStoragePath.string());
  const std::string reflectionRuntime = readFile(reflectionRuntimePath.string());

  CHECK(primeStructDoc.find("### Generic SoA Substrate Boundary") !=
        std::string::npos);
  CHECK(primeStructDoc.find("compiler/runtime-owned SoA\nbehavior separate from the public `soa<T>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Allowed compiler/runtime substrate:** field-layout/codegen/introspection") !=
        std::string::npos);
  CHECK(primeStructDoc.find("generated `SoaSchema*` metadata, `SoaColumn<T>` column storage") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`SoaFieldView<T>` non-owning field views") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Public construction,\n  count/get/ref, push/reserve, field-view") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Current helper-lowering gap:**") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("Generic SoA substrate remains separate from that public collection surface") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("`SoaColumn<T>` storage, `SoaFieldView<T>` non-owning views") !=
        std::string::npos);
  CHECK(todo.find("Generic substrate boundary: compiler/runtime-owned SoA behavior is limited") ==
        std::string::npos);
  CHECK(todo.find("- [~] TODO-4306: Stabilize generic SoA substrate boundaries") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4306: Stabilize generic SoA substrate boundaries") !=
        std::string::npos);

  // soa_storage.prime (renamed from soa_storage in TODO-4633): verify key content
  CHECK(internalStorage.find("[public struct collection_type]\n  SoaColumn<T>()") !=
        std::string::npos);
  CHECK(internalStorage.find("[public struct]\n  SoaFieldView<T>()") !=
        std::string::npos);
  CHECK(internalStorage.find("soaColumnFieldSlotUnsafe<Struct, Field>(") !=
        std::string::npos);
  CHECK(internalStorage.find("/Struct/SoaSchemaFieldOffset(fieldIndex)") !=
        std::string::npos);
  CHECK(internalStorage.find("soaFieldViewRead<T>([SoaFieldView<T>] values") !=
        std::string::npos);
  CHECK(internalStorage.find("soaFieldViewRef<T>([SoaFieldView<T>] values") !=
        std::string::npos);
  CHECK(internalStorage.find("/std/collections/soa/") == std::string::npos);
  CHECK(internalStorage.find("soaVectorNew") == std::string::npos);
  CHECK(internalStorage.find("SoaVector<T>") == std::string::npos);
  CHECK(internalStorage.find("namespace soa_storage") != std::string::npos);

  CHECK(reflectionRuntime.find("reflection SoaSchema helper runtime stays aligned across backends") !=
        std::string::npos);
  CHECK(reflectionRuntime.find("reflection SoaSchema storage helper runtime stays aligned across backends") !=
        std::string::npos);
  // reflection test updated to import soa_storage/* (TODO-4633)
  CHECK(reflectionRuntime.find("import /std/collections/soa_storage/*") !=
        std::string::npos);
  CHECK(reflectionRuntime.find("SoaSchemaStorageReserve(storage, 5i32)") !=
        std::string::npos);
  CHECK(reflectionRuntime.find("import /std/collections/soa/*") ==
        std::string::npos);
}

TEST_CASE("canonical soa example stays source locked") {
  std::filesystem::path examplePath =
      std::filesystem::path("..") / "examples" / "3.Surface" / "soa_ecs.prime";
  std::filesystem::path oldExamplePath =
      std::filesystem::path("..") / "examples" / "3.Surface" / "soa_vector_ecs.prime";
  std::filesystem::path examplesReadmePath = std::filesystem::path("..") / "examples" / "README.md";
  std::filesystem::path exampleSweepPath =
      std::filesystem::path("..") / "tests" / "unit" / "compile_run" / "bindings" / "test_compile_run_bindings_and_examples.cpp";
  if (!std::filesystem::exists(examplePath)) {
    examplePath = std::filesystem::current_path() / "examples" / "3.Surface" / "soa_ecs.prime";
  }
  if (!std::filesystem::exists(oldExamplePath)) {
    oldExamplePath =
        std::filesystem::current_path() / "examples" / "3.Surface" / "soa_vector_ecs.prime";
  }
  if (!std::filesystem::exists(examplesReadmePath)) {
    examplesReadmePath = std::filesystem::current_path() / "examples" / "README.md";
  }
  if (!std::filesystem::exists(exampleSweepPath)) {
    exampleSweepPath = std::filesystem::current_path() / "tests" / "unit" / "compile_run" / "bindings" / "test_compile_run_bindings_and_examples.cpp";
  }
  REQUIRE(std::filesystem::exists(examplePath));
  CHECK(!std::filesystem::exists(oldExamplePath));
  REQUIRE(std::filesystem::exists(examplesReadmePath));
  REQUIRE(std::filesystem::exists(exampleSweepPath));

  const std::string example = readFile(examplePath.string());
  const std::string examplesReadme = readFile(examplesReadmePath.string());
  const std::string exampleSweep = readFile(exampleSweepPath.string());

  CHECK(example.find("import /std/collections/soa/*") != std::string::npos);
  CHECK(example.find("import /std/collections/soa_conversions/*") ==
        std::string::npos);
  CHECK(example.find("[struct reflect]") != std::string::npos);
  CHECK(example.find("[auto mut] particles{soa</Particle>()}") !=
        std::string::npos);
  CHECK(example.find("particles.reserve(plus(particles.count(), spawnQueue.count()))") !=
        std::string::npos);
  CHECK(example.find("soaVectorNew<Particle>()") == std::string::npos);
  CHECK(example.find("soaVectorToAos<Particle>(particles)") == std::string::npos);
  CHECK(example.find("to_aos(particles)") != std::string::npos);
  CHECK(example.find("experimental_soa") == std::string::npos);
  CHECK(example.find("soa_ecs") == std::string::npos);
  CHECK(examplesReadme.find("examples/3.Surface/soa_ecs.prime") !=
        std::string::npos);
  CHECK(examplesReadme.find("soa_vector_ecs.prime") == std::string::npos);
  CHECK(exampleSweep.find("soa_vector_ecs.prime") == std::string::npos);
}

TEST_CASE("source lock inventory keeps replacement surfaces explicit") {
  std::filesystem::path inventoryPath =
      std::filesystem::path("..") / "docs" / "source_lock_inventory.md";
  if (!std::filesystem::exists(inventoryPath)) {
    inventoryPath = std::filesystem::current_path() / "docs" / "source_lock_inventory.md";
  }
  REQUIRE(std::filesystem::exists(inventoryPath));

  const std::string inventory = readFile(inventoryPath.string());

  CHECK(inventory.find("# Source-Lock Inventory") != std::string::npos);
  CHECK(inventory.find("tests/unit/test_compile_run_examples_docs_locks.cpp") !=
        std::string::npos);
  CHECK(inventory.find("tests/unit/test_ir_pipeline_backends_graph_contexts.h") !=
        std::string::npos);
  CHECK(inventory.find("temporary migration lock") != std::string::npos);
  CHECK(inventory.find("CompilePipelineResult") != std::string::npos);
  CHECK(inventory.find("direct variant contract test") != std::string::npos);
  CHECK(inventory.find(
            "expression-emitter lock retired") != std::string::npos);
  CHECK(inventory.find(
            "source C++ emitter emits block-argument expression wrappers without source locks") !=
        std::string::npos);
  CHECK(inventory.find(
            "Guards expression-emitter delegation while C++/GLSL/VM emission "
            "boundaries are still being separated") ==
        std::string::npos);
}

TEST_CASE("status-only result bridge docs stay source locked") {
  const std::filesystem::path syntaxSpecPath =
      resolveRepoPath("docs/PrimeStruct_SyntaxSpec.md");
  const std::filesystem::path todoPath = resolveRepoPath("docs/todo.md");
  const std::filesystem::path todoFinishedPath =
      resolveRepoPath("docs/todo_finished.md");
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string syntaxSpec = readFile(syntaxSpecPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(syntaxSpec.find("IR-backed `Result.error(...)` /\n"
                        "`Result.why(...)` also inspect local, direct-call, "
                        "and dereferenced local") != std::string::npos);
  CHECK(syntaxSpec.find("Imported status-only `Result<Error>` is pickable, "
                        "and IR-backed `try(...)`, postfix") !=
        std::string::npos);
  CHECK(syntaxSpec.find("use `Result.ok(value)` as an `ok`-variant compatibility") !=
        std::string::npos);
  CHECK(syntaxSpec.find("`Result.and_then(result, fn)` compatibility helpers") !=
        std::string::npos);
  CHECK(syntaxSpec.find("The source C++ emitter keeps using a compatibility Result\n"
                        "bridge") != std::string::npos);
  CHECK(syntaxSpec.find("use legacy `Result.ok(value)`") == std::string::npos);
  CHECK(syntaxSpec.find("use legacy `Result.map(result, fn)`") ==
        std::string::npos);
  CHECK(syntaxSpec.find("The legacy source C++ emitter") == std::string::npos);
  CHECK(syntaxSpec.find("IR-backed local, direct-call, and dereferenced local\n"
                        "    borrowed/pointer imported status-only "
                        "`Result<Error>` sums preserve") !=
        std::string::npos);
  CHECK(syntaxSpec.find("imported status-only\n"
                        "`Result.error(...)` / `Result.why(...)` lowering "
                        "remain compatibility surfaces") ==
        std::string::npos);
  CHECK(syntaxSpec.find("propagation remains a hybrid compiler/runtime bridge "
                        "until its migration TODO lands") ==
        std::string::npos);
  CHECK(todo.find("TODO-4313") == std::string::npos);
  CHECK(todoFinished.find("TODO-4313: Align status-only Result bridge docs") !=
        std::string::npos);
  CHECK(todoFinished.find("Aligned SyntaxSpec with the status-only Result helper and\n"
                          "    propagation support that already landed for "
                          "IR-backed paths") != std::string::npos);
}

TEST_CASE("Result helper compatibility adapter inventory stays source locked") {
  const std::filesystem::path primeStructPath = resolveRepoPath("docs/PrimeStruct.md");
  REQUIRE(std::filesystem::exists(primeStructPath));

  const std::filesystem::path repoRoot = primeStructPath.parent_path().parent_path();
  const std::string primeStructDoc = readFile(primeStructPath.string());

  CHECK(primeStructDoc.find("Result helper compatibility adapter inventory:") !=
        std::string::npos);
  const std::vector<std::string> inventoriedPaths = {
      "src/emitter/EmitterExprLambda.h",
      "src/emitter/EmitterExprResultCalls.h",
      "src/ir_lowerer/IrLowererLowerEmitExprResultHelpers.h",
      "src/ir_lowerer/IrLowererLowerEmitExprTryHelpers.cpp",
      "src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp",
      "src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp",
      "src/ir_lowerer/IrLowererLowerSumHelpers.cpp",
      "src/ir_lowerer/IrLowererPackedResultHelpers.cpp",
      "src/ir_lowerer/IrLowererResultHelpers.cpp",
      "src/ir_lowerer/IrLowererStatementBindingStatementEmit.cpp",
      "src/ir_lowerer/IrLowererStatementBindingTypeMetadata.cpp",
      "src/ir_lowerer/IrLowererUninitializedStructInference.cpp",
      "src/semantics/SemanticsValidatorBuildParameters.cpp",
      "src/semantics/SemanticsValidatorExprResultFile.cpp",
      "src/semantics/SemanticsValidatorExprSumConstructors.cpp",
      "src/semantics/SemanticsValidatorInferGraph.cpp",
      "src/semantics/SemanticsValidatorInferPreDispatchCalls.cpp",
      "src/semantics/SemanticsValidatorResultHelpers.cpp",
      "src/semantics/SemanticsValidatorStatementReturns.cpp",
      "src/semantics/TemplateMonomorphBindingCallInference.h",
      "src/semantics/TemplateMonomorphExperimentalCollectionValueRewrites.h",
  };
  for (const std::string &path : inventoriedPaths) {
    CHECK(primeStructDoc.find("`" + path + "`") != std::string::npos);
  }
  CHECK(primeStructDoc.find("Retirement decision:") != std::string::npos);
  CHECK(primeStructDoc.find("implemented as ordinary `/std/result/*` helpers") !=
        std::string::npos);
  CHECK(primeStructDoc.find("delete packed bridge emission") !=
        std::string::npos);

  const std::vector<std::string> expectedHelperStringFiles = {
      "src/ir_lowerer/IrLowererLowerEmitExprResultHelpers.h",
      "src/ir_lowerer/IrLowererLowerSumHelpers.cpp",
      "src/ir_lowerer/IrLowererPackedResultHelpers.cpp",
      "src/ir_lowerer/IrLowererStatementBindingStatementEmit.cpp",
      "src/semantics/SemanticsValidatorExprResultFile.cpp",
      "src/semantics/SemanticsValidatorExprSumConstructors.cpp",
      "src/semantics/SemanticsValidatorResultHelpers.cpp",
      "src/semantics/SemanticsValidatorStatementReturns.cpp",
  };
  CHECK(productionFilesContainingAny(repoRoot,
                                     {"Result.ok",
                                      "Result.map",
                                      "Result.and_then",
                                      "Result.map2"}) ==
        expectedHelperStringFiles);
  CHECK(primeStructDoc.find("legacy `Result.ok(value)`") == std::string::npos);
  CHECK(primeStructDoc.find("legacy `Result.map(result, fn)`") ==
        std::string::npos);
  CHECK(primeStructDoc.find("legacy Result helpers") == std::string::npos);
}

TEST_CASE("generic requirement predicate surface stays source locked") {
  const std::filesystem::path primeStructPath = resolveRepoPath("docs/PrimeStruct.md");
  const std::filesystem::path syntaxSpecPath =
      resolveRepoPath("docs/PrimeStruct_SyntaxSpec.md");
  const std::filesystem::path codeExamplesPath =
      resolveRepoPath("docs/CodeExamples.md");
  const std::filesystem::path safeArrayExtentViewsPath =
      resolveRepoPath("docs/SafeArrayExtentViews.md");
  const std::filesystem::path todoFinishedPath =
      resolveRepoPath("docs/todo_finished.md");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(safeArrayExtentViewsPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpec = readFile(syntaxSpecPath.string());
  const std::string codeExamplesDoc = readFile(codeExamplesPath.string());
  const std::string safeArrayExtentViewsDoc =
      readFile(safeArrayExtentViewsPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(primeStructDoc.find(
            "Requirements and contracts are definition transforms, not body statements") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`require<...>` is the forced compile-time requirement form.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("[require<typeof<left> == i32, typeof<right> == i32>]") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`require(...)` is the runtime-capable contract form.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("[require(count(dst) == count(src))]") !=
        std::string::npos);
  CHECK(primeStructDoc.find("A definition may carry at most one `require<...>` transform and at most one\n"
                            "  `require(...)` transform.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Current implementation boundary: the parser and semantic validator still\n"
                            "  accept legacy `[require(...)]`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`typeof<left>` is a compile-time query because") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`typeof(left)` remains an ordinary\n"
                            "  runtime call shape and is never compile-time requirement syntax.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`/std/meta/type_equals<A, B>()`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`/std/meta/has_trait<T>(Trait)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`/std/meta/can_construct<T, Args...>()`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`/std/meta/can_copy<T>()`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`/std/meta/can_move<T>()`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`/std/meta/has_field<T>(name)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`/std/meta/value_greater_equal<A, B>()`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("simple\n"
                            "  comparisons over compile-time values such as `N > 0`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The implemented capability slice evaluates `has_trait` for\n"
                            "  `Additive`/`Multiplicative`/`Comparable`/`Indexable`") !=
        std::string::npos);
  CHECK(syntaxSpec.find("The compile-time host can answer these canonical `/std/meta/*` predicates\n"
                        "  from published semantic requirement facts with deterministic success,\n"
                        "  unsatisfied, and invalid-evaluation results. `field_type_equals` remains") !=
        std::string::npos);
  CHECK(primeStructDoc.find("compile-time integer `value_*` equality and ordering predicates.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Existing transform-style trait constraints such as `[Additive<i32>]` remain\n"
                            "source-compatible compatibility vocabulary") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Failed requirements on direct calls are diagnostics, not C++-style\n"
                            "  substitution failure by accident.") !=
        std::string::npos);

  CHECK(syntaxSpec.find("[require<typeof<left> == typeof<right>, "
                        "meta.has_trait<typeof<left>>(Additive)>]") !=
        std::string::npos);
  CHECK(syntaxSpec.find("They live with the callable signature, run") !=
        std::string::npos);
  CHECK(syntaxSpec.find("publish facts rather\n"
                        "  than ordinary values visible to the function body.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("`require<...>` is forced compile-time requirement syntax.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("`require(...)` is contract syntax over values.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("A definition has at most one `require<...>` transform and at most one\n"
                        "  `require(...)` transform.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("`typeof(left)` remains an ordinary runtime\n"
                        "  call-shaped expression and is not a compile-time requirement primitive.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("comma-separated conjunction through `require<...>`, builtin predicate calls,\n"
                        "  user-defined compile-time predicates, and simple comparisons over\n"
                        "  compile-time values such as `N > 0`.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("legacy `[require(...)]` is still accepted\n"
                        "  as transition syntax for compile-time generic requirements.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("User-defined predicates distinguish `false` from invalid evaluation") !=
        std::string::npos);
  CHECK(codeExamplesDoc.find(
            "Use `require<...>` for caller-visible compile-time obligations") !=
        std::string::npos);
  CHECK(codeExamplesDoc.find(
            "Use `require(...)` only for pure runtime-capable contracts") !=
        std::string::npos);
  CHECK(codeExamplesDoc.find(
            "Preferred phase-split spelling uses `require<...>` for these compile-time-only\n"
            "facts:") != std::string::npos);
  CHECK(codeExamplesDoc.find(
            "Transition-only current compiler spelling still uses `require(...)` for the\n"
            "same compile-time requirement facts") != std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find(
            "require<...>   // compile-time requirement, no runtime fallback") !=
        std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find(
            "require(...)   // contract: prove statically if possible, otherwise check") !=
        std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find(
            "New specification\n"
            "prose should use `require<...>` for the forced compile-time form and reserve\n"
            "`require(...)` for contracts that may be proven or checked at runtime.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The first implemented user-predicate slice evaluates pure zero-runtime-argument\n"
                            "  predicates whose bodies return a literal source `bool`.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The implemented compile-time branch slice adds\n"
                            "  `ct_if(predicate()) { ... } else { ... }`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("branches such as `return(ct_if(...) { value } else { fallback })`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Generic-specialized definitions may also use `ct_if` over type facts\n"
                            "  after template monomorphization selects concrete parameter types.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Local generated structs\n"
                            "  introduced by the selected statement branch receive a deterministic\n"
                            "  branch-scoped identity") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Compile-time flow is pure by default.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`effects<compiletime>(...)` uses the same effect vocabulary as runtime\n"
                            "  `effects(...)`, but it authorizes only the compile-time phase") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Compile-time termination is budgeted in categories that TODO-4358 must\n"
                            "  enforce independently") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Compile-time caches are semantic caches, not backend caches.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Compile-time diagnostic categories are stable: `satisfied`, `unsatisfied`,\n"
                            "  `invalid-evaluation`, `denied-effect`, `budget-exhausted`,") !=
        std::string::npos);
  CHECK(syntaxSpec.find("The initial implemented user-predicate evaluator accepts pure predicates with\n"
                        "  no runtime parameters and a literal source `bool` return body.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("The implemented compile-time branch form is\n"
                        "  `ct_if(predicate()) { ... } else { ... }`.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("Generic\n"
                        "  definitions may defer `ct_if` branch selection until template\n"
                        "  monomorphization gives the predicate concrete specialized facts.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("In expression position, return values, local binding\n"
                        "  initializers, and nested expression operands use exactly one selected branch\n"
                        "  value.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("Local generated structs introduced by the selected statement branch receive\n"
                        "  deterministic branch-scoped identities") !=
        std::string::npos);
  CHECK(syntaxSpec.find("Compile-time execution is pure unless the enclosing definition declares\n"
                        "  phase-qualified effects with `effects<compiletime>(...)`.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("Runtime `effects(...)` and compile-time `effects<compiletime>(...)` share\n"
                        "  effect names but authorize different phases.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("Compile-time termination uses independent budgets for callable preparation,\n"
                        "  call depth and recursion edges, evaluator steps") !=
        std::string::npos);
  CHECK(syntaxSpec.find("Compile-time cache keys include the language/semantic-product version,\n"
                        "  predicate/helper identity, normalized compile-time arguments") !=
        std::string::npos);
  CHECK(syntaxSpec.find("Compile-time diagnostics use stable categories: `satisfied`, `unsatisfied`,\n"
                        "  `invalid-evaluation`, `denied-effect`, `budget-exhausted`,") !=
        std::string::npos);
  CHECK(syntaxSpec.find("Failed requirements on a direct call are diagnostics, not C++-style\n"
                        "  substitution failure by accident.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("rather than silently\n"
                        "  erasing candidates.") !=
        std::string::npos);

  CHECK(todoFinished.find("TODO-4341: Define generic requirement predicate surface") !=
        std::string::npos);
  CHECK(todoFinished.find("Marked transform-style trait constraints such as `[Additive<i32>]` as\n"
                          "      source-compatible compatibility vocabulary") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4558: Add generic parser and source-lock conformance") !=
        std::string::npos);
  CHECK(todoFinished.find("Added parser conformance for canonical `require(...)` predicate\n"
                          "      transforms covering same-type, capability, and compile-time value\n"
                          "      predicates.") !=
        std::string::npos);
}

TEST_CASE("safe pointer optionality docs stay source locked") {
  const std::filesystem::path primeStructPath = resolveRepoPath("docs/PrimeStruct.md");
  const std::filesystem::path memoryCapabilitiesPath =
      resolveRepoPath("docs/MemoryCapabilities.md");
  const std::filesystem::path safeArrayExtentViewsPath =
      resolveRepoPath("docs/SafeArrayExtentViews.md");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(memoryCapabilitiesPath));
  REQUIRE(std::filesystem::exists(safeArrayExtentViewsPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string memoryCapabilitiesDoc =
      readFile(memoryCapabilitiesPath.string());
  const std::string safeArrayExtentViewsDoc =
      readFile(safeArrayExtentViewsPath.string());

  CHECK(primeStructDoc.find("in safe code, `Pointer<T>` is a valid non-null storage identity") !=
        std::string::npos);
  CHECK(primeStructDoc.find("must expose that possibility as `Maybe<Pointer<T>>` or\n"
                            "  `Result<Pointer<T>, ErrorT>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Raw or foreign nullable\n"
                            "  addresses remain unsafe adapter material") !=
        std::string::npos);
  CHECK(primeStructDoc.find("the built-in heap intrinsics still return bare `Pointer<T>` values") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`Maybe<Pointer<T>>` or `Result<Pointer<T>, AllocError>`") !=
        std::string::npos);

  CHECK(memoryCapabilitiesDoc.find("`Pointer<T>` value in safe code names valid non-null storage") !=
        std::string::npos);
  CHECK(memoryCapabilitiesDoc.find("[return<Result<Pointer<T>, AllocError>> needs<write(arena)>]") !=
        std::string::npos);
  CHECK(memoryCapabilitiesDoc.find("[unsafe return<Maybe<Pointer<T>>> needs<addr(foreign)>]") !=
        std::string::npos);
  CHECK(memoryCapabilitiesDoc.find("that\n"
                                   "uncertainty belongs to `RawPointer<T>` and unsafe/FFI adapter code") !=
        std::string::npos);
  CHECK(memoryCapabilitiesDoc.find("`Result<Pointer<T>, FfiError>` instead of letting a nullable address masquerade\n"
                                   "as safe `Pointer<T>`.") !=
        std::string::npos);

  CHECK(safeArrayExtentViewsDoc.find("Maybe<Pointer<T>>\n"
                                     "Result<Pointer<T>, AllocError>") !=
        std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find("[return<Result<Pointer<T>, AllocError>>]") !=
        std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find("`Pointer<T>` is a valid, non-null pointer value in safe code") !=
        std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find("return `Maybe<Pointer<T>>` or `Result<Pointer<T>, E>`") !=
        std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find("later implementation leaves should either change\n"
                                     "those intrinsic signatures or add safe fallible wrappers") !=
        std::string::npos);
}

TEST_CASE("capability parameterized views docs stay source locked") {
  const std::filesystem::path primeStructPath =
      resolveRepoPath("docs/PrimeStruct.md");
  const std::filesystem::path memoryCapabilitiesPath =
      resolveRepoPath("docs/MemoryCapabilities.md");
  const std::filesystem::path safeArrayExtentViewsPath =
      resolveRepoPath("docs/SafeArrayExtentViews.md");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(memoryCapabilitiesPath));
  REQUIRE(std::filesystem::exists(safeArrayExtentViewsPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string memoryCapabilitiesDoc =
      readFile(memoryCapabilitiesPath.string());
  const std::string safeArrayExtentViewsDoc =
      readFile(safeArrayExtentViewsPath.string());

  CHECK(primeStructDoc.find("Capability-parameterized views are a core design direction") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`Reference<T, Capability>` is the count-one view form") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`Slice<T, Capability>` is the contiguous runtime-count view form") !=
        std::string::npos);
  CHECK(primeStructDoc.find("the capability view model around "
                            "`Reference<T, Capability>` / `Slice<T, Capability>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("the normative view model is semantic\n"
                            "  `View<T, Capability>` over valid `Pointer<T>` storage") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`Reference<T, Capability>` is the non-null single-element view with\n"
                            "  `count == 1`; `Slice<T, Capability>` is the contiguous multi-element view\n"
                            "  with a runtime `count`") != std::string::npos);
  CHECK(primeStructDoc.find("`Reference<T, Capability>` is not nullable and does not carry an absence\n"
                            "  state") != std::string::npos);
  CHECK(primeStructDoc.find("`Slice<T, Capability>` carries a runtime element count and borrows\n"
                            "  `values[start, end)`") != std::string::npos);
  CHECK(primeStructDoc.find("Current\n"
                            "  implementation boundary: parser and lowering support the existing\n"
                            "  `Reference<T>`, array, and pointer surfaces only") !=
        std::string::npos);

  CHECK(memoryCapabilitiesDoc.find("## Unified View Values") !=
        std::string::npos);
  CHECK(memoryCapabilitiesDoc.find("View<T, Capability> {\n"
                                   "  pointer: Pointer<T>\n"
                                   "  count: element count\n"
                                   "  provenance: source place or owner identity\n"
                                   "  capability: inherited authority metadata\n"
                                   "}") != std::string::npos);
  CHECK(memoryCapabilitiesDoc.find("Canonical rule: `Reference<T, Capability>` is the non-null single-element view\n"
                                   "with `count == 1`; `Slice<T, Capability>` is the contiguous multi-element view\n"
                                   "with a runtime `count`") != std::string::npos);
  CHECK(memoryCapabilitiesDoc.find("It is not a nullable pointer and does not carry an absence\n"
                                   "state; optional production belongs in `Maybe<Pointer<T>>`,\n"
                                   "`Maybe<Reference<T, Capability>>`, or a `Result` wrapper") !=
        std::string::npos);
  CHECK(memoryCapabilitiesDoc.find("It carries the runtime\n"
                                   "element count needed for indexing, iteration, and bounds checks") !=
        std::string::npos);
  CHECK(memoryCapabilitiesDoc.find("Exact standard\n"
                                   "capability names are still open design vocabulary.") !=
        std::string::npos);

  CHECK(safeArrayExtentViewsDoc.find("phase split and unified capability view model in this note have\n"
                                     "been promoted into the normative language direction") !=
        std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find("Reference<T, Capability> == Slice<T, Capability> where count == 1") !=
        std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find("Canonical rule: `Reference<T, Capability>` is the non-null single-element view\n"
                                     "with `count == 1`; `Slice<T, Capability>` is the contiguous multi-element view\n"
                                     "with a runtime `count`") != std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find("For `Reference<T, Capability>`, `count` is statically one") !=
        std::string::npos);
  CHECK(safeArrayExtentViewsDoc.find("For `Slice<T, Capability>`, `count` is\n"
                                     "normally a runtime value.") != std::string::npos);
}

TEST_CASE("task spawn wait prototype docs stay source locked") {
  const std::filesystem::path primeStructPath =
      resolveRepoPath("docs/PrimeStruct.md");
  const std::filesystem::path multithreadingPath =
      resolveRepoPath("docs/MultithreadingPrototype.md");
  const std::filesystem::path todoFinishedPath =
      resolveRepoPath("docs/todo_finished.md");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(multithreadingPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string multithreadingDoc = readFile(multithreadingPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(primeStructDoc.find("the first structured-concurrency surface uses `[effects(task)]`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The parser accepts `[spawn] f(...)` as an execution transform on call envelopes") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The semantic pass publishes `Task<T>` binding facts for\n"
                            "  `[spawn] f(...)`, infers `wait(Task<T>) -> T` and multi-task `wait(...) -> tuple<...>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("VM/native lower the structured runtime slice by storing spawned call results in task handle bindings") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`spawn` is reserved for the first task surface and must prefix call syntax as\n"
                            "    `[spawn] f(...)`.") !=
        std::string::npos);

  CHECK(multithreadingDoc.find("left{[spawn] computeLeft()}") !=
        std::string::npos);
  CHECK(multithreadingDoc.find("leftResult{wait(left)}") !=
        std::string::npos);
  CHECK(multithreadingDoc.find("The grouped effect name `task` is the initial prototype spelling.") !=
        std::string::npos);
  CHECK(multithreadingDoc.find("TODO-4561 locked the parser surface and\n"
                               "documentation spelling, and TODO-4562 added semantic `Task<T>` facts") !=
        std::string::npos);
  CHECK(multithreadingDoc.find("TODO-4563 added the first VM/native execution behavior") !=
        std::string::npos);
  CHECK(multithreadingDoc.find("TODO-4278 then layered multi-task `wait(left, right, ...)` on\n"
                               "top of that substrate by returning ordinary stdlib tuple values.") !=
        std::string::npos);
  CHECK(multithreadingDoc.find("as a stack-backed task handle whose payload is the result of `computeLeft()`.") !=
        std::string::npos);

  CHECK(todoFinished.find("TODO-4561: Add task spawn/wait parser and effect locks") !=
        std::string::npos);
  CHECK(todoFinished.find("Locked `spawn` as an execution-only transform that must prefix call\n"
                          "      syntax such as `[spawn] f(...)`.") !=
        std::string::npos);
  CHECK(todoFinished.find("Left `Task<T>` semantic facts, lifetime diagnostics, and runtime/native\n"
                          "      execution to TODO-4562 and TODO-4563.") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4562: Add task handle semantic facts and lifetime diagnostics") !=
        std::string::npos);
  CHECK(todoFinished.find("Added semantic task-handle tracking so `[spawn] f(...)` publishes\n"
                          "      `Task<T>` binding/local-auto facts when `f` returns `T`.") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4563: Add single-task spawn/wait runtime execution") !=
        std::string::npos);
  CHECK(todoFinished.find("Lowered a single `[Task<T>] handle{[spawn] f(...)}` binding as a\n"
                          "      stack-backed stored result for the spawned call.") !=
        std::string::npos);
}


TEST_SUITE_END();
