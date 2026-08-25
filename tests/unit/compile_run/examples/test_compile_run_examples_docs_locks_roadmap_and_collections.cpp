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

TEST_SUITE_END();
