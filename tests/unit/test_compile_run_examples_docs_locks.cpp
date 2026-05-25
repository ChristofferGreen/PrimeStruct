#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

#include <algorithm>
#include <sstream>
#include <vector>

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

static std::filesystem::path resolveUnitTestsPath() {
  std::filesystem::path testsPath = std::filesystem::path("..") / "tests" / "unit";
  if (!std::filesystem::exists(testsPath)) {
    testsPath = std::filesystem::current_path() / "tests" / "unit";
  }
  return testsPath;
}

static std::filesystem::path resolveRepoPath(const std::filesystem::path &relativePath) {
  std::filesystem::path path = std::filesystem::path("..") / relativePath;
  if (!std::filesystem::exists(path)) {
    path = std::filesystem::current_path() / relativePath;
  }
  return path;
}

static std::vector<std::filesystem::path> filesWithRetainedDoctestSkips(
    const std::filesystem::path &testsPath) {
  std::vector<std::filesystem::path> paths;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(testsPath)) {
    if (!entry.is_regular_file() ||
        entry.path().filename() == "test_compile_run_examples_docs_locks.cpp") {
      continue;
    }
    const std::string source = readFile(entry.path().string());
    if (source.find("doctest::skip(true)") != std::string::npos) {
      paths.push_back(entry.path());
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

static std::vector<std::string> productionFilesContainingAny(
    const std::filesystem::path &repoRoot,
    const std::vector<std::string> &needles) {
  std::vector<std::string> paths;
  for (const char *dirname : {"src", "include"}) {
    const std::filesystem::path root = repoRoot / dirname;
    if (!std::filesystem::exists(root)) {
      continue;
    }
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      const std::string ext = entry.path().extension().string();
      if (ext != ".cpp" && ext != ".h" && ext != ".hpp") {
        continue;
      }
      const std::string source = readFile(entry.path().string());
      for (const std::string &needle : needles) {
        if (source.find(needle) != std::string::npos) {
          paths.push_back(entry.path().lexically_relative(repoRoot).generic_string());
          break;
        }
      }
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

static std::string currentTestCaseNameFromLine(const std::string &line,
                                               const std::string &currentName) {
  const std::string prefix = "TEST_CASE(\"";
  const std::size_t start = line.find(prefix);
  if (start == std::string::npos) {
    return currentName;
  }
  const std::size_t nameStart = start + prefix.size();
  const std::size_t nameEnd = line.find('"', nameStart);
  if (nameEnd == std::string::npos) {
    return currentName;
  }
  return line.substr(nameStart, nameEnd - nameStart);
}

static bool isDirectOldSoaImportLine(const std::string &line) {
  const std::size_t first = line.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return false;
  }
  const std::string trimmed = line.substr(first);
  return trimmed.rfind("import /std/collections/soa_vector", 0) == 0 ||
         trimmed.rfind("import /std/collections/experimental_soa_vector", 0) == 0;
}

static bool isExplicitSoaRejectionFixtureName(const std::string &testName) {
  for (const char *marker :
       {"reject", "rejection", "legacy", "old-surface", "builtin", "substrate"}) {
    if (testName.find(marker) != std::string::npos) {
      return true;
    }
  }
  return false;
}

static std::vector<std::string> directOldSoaImportFixtureViolations(
    const std::filesystem::path &testsPath) {
  std::vector<std::string> violations;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(testsPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string ext = entry.path().extension().string();
    if (ext != ".cpp" && ext != ".h") {
      continue;
    }
    std::istringstream stream(readFile(entry.path().string()));
    std::string line;
    std::string currentTestName;
    int lineNumber = 0;
    while (std::getline(stream, line)) {
      ++lineNumber;
      currentTestName = currentTestCaseNameFromLine(line, currentTestName);
      if (!isDirectOldSoaImportLine(line) ||
          isExplicitSoaRejectionFixtureName(currentTestName)) {
        continue;
      }
      violations.push_back(entry.path().lexically_relative(testsPath).generic_string() + ":" +
                           std::to_string(lineNumber) + " in " + currentTestName);
    }
  }
  std::sort(violations.begin(), violations.end());
  return violations;
}

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
      std::filesystem::path("..") / "tests" / "unit" / "test_compile_run_vm_collections_vector_limits_a.cpp";
  std::filesystem::path nativeVectorLimitsPath = std::filesystem::path("..") / "tests" / "unit" /
                                                 "test_compile_run_native_backend_collections_mutators_and_limits_a.cpp";
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
    vmVectorLimitsPath =
        std::filesystem::current_path() / "tests" / "unit" / "test_compile_run_vm_collections_vector_limits_a.cpp";
  }
  if (!std::filesystem::exists(nativeVectorLimitsPath)) {
    nativeVectorLimitsPath = std::filesystem::current_path() / "tests" / "unit" /
                             "test_compile_run_native_backend_collections_mutators_and_limits_a.cpp";
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
  CHECK(primeStructDoc.find("Add a concrete TODO before changing that UI runtime seam") !=
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
  CHECK(codingGuidelines.find("incubating `soa_vector<T>`") == std::string::npos);
  CHECK(codingGuidelines.find("planned `soa_vector<T>`") == std::string::npos);
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
  CHECK(codeExamples.find("`stdlib/std/collections/vector.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/map.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/errors.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/soa.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/soa_vector.prime`") ==
        std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/soa_vector_conversions.prime`") ==
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
  CHECK(primeStructDoc.find("`stdlib/std/collections/surfaces.psmeta`") !=
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
  CHECK(primeStructDoc.find("SoA public helper, constructor, import-alias, field-view, and conversion\n"
                            "  metadata now lives in `stdlib/std/collections/surfaces.psmeta`") !=
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
  CHECK(todo.find("- TODO-4570: Retire duplicate map2 candidate surface | track: collection-stdlib-cleanup") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4571: Add compiler-knowledge inventory for map/vector | track: collection-audit") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4572: Remove vector statement-helper compiler path | track: vector-special-case-deletion") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4573: Remove compiler-owned map literal lowering | track: map-special-case-deletion") !=
        std::string::npos);
  CHECK(todo.find("Map/vector compiler-independence: TODO-4570 and TODO-4571 can run in") !=
        std::string::npos);
  CHECK(todo.find("TODO-4574: Remove vector count/access compiler classifiers") !=
        std::string::npos);
  CHECK(todo.find("### Vector/Map Bridge Contract Summary") == std::string::npos);
  CHECK(todo.find("later cutover TODOs retire them") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4042:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4043:") == std::string::npos);
  CHECK(todo.find("TODO-4044") == std::string::npos);
  CHECK(todo.find("TODO-4187") == std::string::npos);
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
            "`/std/collections/internal_vector/*` family owns the internal backing adapter behind that") !=
        std::string::npos);
  CHECK(primeStructDoc.find(
            "`/std/collections/experimental_map/*` is\n"
            "  rejected as a source import and remains only as a legacy forwarding shim") !=
        std::string::npos);
  CHECK(primeStructDoc.find("no `experimental` namespace counts as canonical public API") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/internal_vector/*` | Internal substrate/helper namespace | Internal vector backing adapter used by canonical `/std/collections/vector/*`; it preserves the current compatibility `Vector<T>` type identity until the final vector surface audit. | TODO-4373 |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/experimental_vector/*` | Rejected compatibility namespace | Direct source imports are rejected; the shim remains only as legacy forwarding storage identity behind `/std/collections/internal_vector/*` until the final vector surface audit. | TODO-4373 |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/experimental_map/*` | Rejected compatibility namespace | Direct source imports are rejected; the shim remains only as legacy forwarding storage identity behind `/std/collections/internal_map/*` until the final map surface audit. | TODO-4464 |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/gfx/experimental/*` | Temporary compatibility namespace | Legacy compatibility shim over canonical `/std/gfx/*`; no longer part of the public gfx contract and retained only for targeted compatibility coverage while the residual seam remains importable. | none |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/internal_buffer_checked/*` | Internal substrate/helper namespace | Explicitly internal checked buffer plumbing for container conformance and memory-wrapper flows, not a stable user-facing stdlib API. | none |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Explicit canonical statement calls to\n"
                            "    `/std/collections/vector/push`, `pop`, `reserve`, `clear`, `remove_at`,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("fall through to visible `.prime` helper definitions\n"
                            "    instead of the vector statement-helper emitter") !=
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
  CHECK(todo.find("- TODO-4570: Retire duplicate map2 candidate surface") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4571: Add compiler-knowledge inventory for map/vector") !=
        std::string::npos);
  CHECK(todo.find("Stdlib de-experimentalization: TODO-4059") == std::string::npos);
  CHECK(todo.find("TODO-4103: Rename the remaining experimental SoA storage namespace") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4059:") == std::string::npos);
  CHECK(todo.find("Legacy gfx compatibility seam: `/std/gfx/experimental/*` remains importable") ==
        std::string::npos);
  CHECK(todo.find("Accepted temporary compatibility namespace:") == std::string::npos);
  CHECK(todo.find("`/std/collections/soa_vector*` and `/std/collections/experimental_soa_vector*`") ==
        std::string::npos);
  CHECK(todo.find("SoA compatibility shim: direct") == std::string::npos);
  CHECK(todo.find("imports are rejected; canonical public code uses `/std/collections/soa/*`") ==
        std::string::npos);
  CHECK(todo.find("`/std/collections/internal_soa_vector_conversions/*`,") ==
        std::string::npos);
  CHECK(todo.find("no active\n  TODO currently targets them") ==
        std::string::npos);
  CHECK(todo.find("until their explicit shim, rename, or maturity TODOs land") ==
        std::string::npos);
  CHECK(todo.find("/std/collections/internal_buffer_checked/*`,") == std::string::npos);
  CHECK(todo.find("/std/collections/internal_buffer_unchecked/*`,") == std::string::npos);
  CHECK(todo.find("/std/collections/internal_soa_storage/*` are implementation-facing") ==
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
      std::filesystem::path("..") / "tests" / "unit" / "test_compile_run_checked_pointer_conformance_helpers.h";
  std::filesystem::path vmCompatTestPath =
      std::filesystem::path("..") / "tests" / "unit" / "test_compile_run_vm_collections_wrapper_temporaries_a.cpp";
  std::filesystem::path nativeCompatTestPath = std::filesystem::path("..") / "tests" / "unit" /
                                               "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(checkedPointerHelpersPath)) {
    checkedPointerHelpersPath =
        std::filesystem::current_path() / "tests" / "unit" / "test_compile_run_checked_pointer_conformance_helpers.h";
  }
  if (!std::filesystem::exists(vmCompatTestPath)) {
    vmCompatTestPath =
        std::filesystem::current_path() / "tests" / "unit" / "test_compile_run_vm_collections_wrapper_temporaries_a.cpp";
  }
  if (!std::filesystem::exists(nativeCompatTestPath)) {
    nativeCompatTestPath = std::filesystem::current_path() / "tests" / "unit" /
                           "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(checkedPointerHelpersPath));
  REQUIRE(std::filesystem::exists(vmCompatTestPath));
  REQUIRE(std::filesystem::exists(nativeCompatTestPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string checkedPointerHelpers = readFile(checkedPointerHelpersPath.string());
  const std::string vmCompatTest = readFile(vmCompatTestPath.string());
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
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_soa_vector.prime";
  std::filesystem::path soaExamplePath =
      std::filesystem::path("..") / "examples" / "3.Surface" / "soa_ecs.prime";
  std::filesystem::path cppCompatTestPath =
      std::filesystem::path("..") / "tests" / "unit" / "test_compile_run_imports_operations.cpp";
  std::filesystem::path vmCompatTestPath =
      std::filesystem::path("..") / "tests" / "unit" / "test_compile_run_vm_collections_wrapper_temporaries_a.cpp";
  std::filesystem::path nativeCompatTestPath = std::filesystem::path("..") / "tests" / "unit" /
                                               "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp";
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
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "experimental_soa_vector.prime";
  }
  if (!std::filesystem::exists(soaExamplePath)) {
    soaExamplePath = std::filesystem::current_path() / "examples" / "3.Surface" / "soa_ecs.prime";
  }
  if (!std::filesystem::exists(cppCompatTestPath)) {
    cppCompatTestPath =
        std::filesystem::current_path() / "tests" / "unit" / "test_compile_run_imports_operations.cpp";
  }
  if (!std::filesystem::exists(vmCompatTestPath)) {
    vmCompatTestPath = std::filesystem::current_path() / "tests" / "unit" /
                       "test_compile_run_vm_collections_wrapper_temporaries_a.cpp";
  }
  if (!std::filesystem::exists(nativeCompatTestPath)) {
    nativeCompatTestPath = std::filesystem::current_path() / "tests" / "unit" /
                           "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp";
  }
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));
  REQUIRE(std::filesystem::exists(experimentalSoaVectorPath));
  REQUIRE(std::filesystem::exists(soaExamplePath));
  REQUIRE(std::filesystem::exists(cppCompatTestPath));
  REQUIRE(std::filesystem::exists(vmCompatTestPath));
  REQUIRE(std::filesystem::exists(nativeCompatTestPath));

  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpecDoc = readFile(syntaxSpecPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());
  const std::string experimentalSoaVector = readFile(experimentalSoaVectorPath.string());
  const std::string soaExample = readFile(soaExamplePath.string());
  const std::string cppCompatTest = readFile(cppCompatTestPath.string());
  const std::string vmCompatTest = readFile(vmCompatTestPath.string());
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
  CHECK(primeStructDoc.find("| `/std/collections/experimental_soa_vector/*` | Rejected compatibility namespace |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Direct source imports are rejected; the shim remains only behind internal SoA implementation adapters") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/internal_soa_vector/*` | Internal substrate/helper namespace |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Internal SoA wrapper implementation adapter used by canonical") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/internal_soa_vector_conversions/*` | Internal substrate/helper namespace |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Internal SoA conversion implementation adapter used by canonical") !=
        std::string::npos);
  CHECK(primeStructDoc.find("/std/collections/internal_soa_storage/*") != std::string::npos);
  CHECK(primeStructDoc.find("Direct source imports are rejected; canonical conversions route through") !=
        std::string::npos);
  CHECK(primeStructDoc.find("This section is the scope reference for the promoted `soa<T>` public") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Focused rejection tests keep their diagnostics stable") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Rejected compatibility seams:** `/std/collections/soa_vector*`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Internal substrate namespaces:** `/std/collections/internal_soa_vector/*`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("owns ordinary canonical wrapper implementation forwarding") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`/std/collections/internal_soa_vector_conversions/*` owns ordinary canonical") !=
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
  CHECK(todo.find("Rename direction: `soa_vector<T>` is retired as the public collection") ==
        std::string::npos);
  CHECK(todo.find("/std/collections/soa_vector*`, rooted") == std::string::npos);
  CHECK(todo.find("Retired compatibility spellings are `soa_vector<T>`") ==
        std::string::npos);
  CHECK(todo.find("Rejection seams: C++/VM/native tests lock the direct-import rejection") ==
        std::string::npos);
  CHECK(todo.find("Internal substrate namespaces: `/std/collections/internal_soa_vector/*`") ==
        std::string::npos);
  CHECK(todo.find("owns canonical wrapper implementation forwarding") ==
        std::string::npos);
  CHECK(todo.find("`/std/collections/internal_soa_vector_conversions/*` owns canonical") ==
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
  CHECK(todo.find("code no longer needs `experimental_soa_vector`") ==
        std::string::npos);
  CHECK(todo.find("`soa_vector<T>` remains an\n  incubating canonical experiment") ==
        std::string::npos);
  CHECK(todo.find("The canonical wrapper routes through\n  `/std/collections/internal_soa_vector/*`") ==
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
  CHECK(soaExample.find("import /std/collections/soa_vector/*") ==
        std::string::npos);
  CHECK(soaExample.find("import /std/collections/soa_vector_conversions/*") ==
        std::string::npos);
  CHECK(soaExample.find("import /std/collections/experimental_soa_vector/*") ==
        std::string::npos);
  CHECK(soaExample.find("import /std/collections/experimental_soa_vector_conversions/*") ==
        std::string::npos);
  CHECK(experimentalSoaVector.find(
            "// Rejected direct-import shim behind canonical /std/collections/soa/*.") !=
        std::string::npos);
  CHECK(experimentalSoaVector.find(
            "// Internal adapters may import this file while public source imports reject.") !=
        std::string::npos);
  CHECK(experimentalSoaVector.find(
            "// Canonical wrappers route through /std/collections/internal_soa_vector/*.") !=
        std::string::npos);
  CHECK(experimentalSoaVector.find(
            "// Ordinary public examples should import /std/collections/soa/* instead.") !=
        std::string::npos);
  CHECK(cppCompatTest.find("TEST_CASE(\"rejects experimental soa_vector stdlib helpers in C++ emitter\")") !=
        std::string::npos);
  CHECK(cppCompatTest.find("import /std/collections/experimental_soa_vector/*") !=
        std::string::npos);
  CHECK(vmCompatTest.find("TEST_CASE(\"rejects vm experimental soa_vector stdlib helpers\")") !=
        std::string::npos);
  CHECK(vmCompatTest.find("import /std/collections/experimental_soa_vector/*") !=
        std::string::npos);
  CHECK(nativeCompatTest.find("TEST_CASE(\"rejects native experimental soa_vector stdlib helpers\")") !=
        std::string::npos);
  CHECK(nativeCompatTest.find("import /std/collections/experimental_soa_vector/*") !=
        std::string::npos);
}

TEST_CASE("legacy soa_vector compatibility rejection matrix stays source locked") {
  const std::filesystem::path cppParityPath =
      resolveRepoPath(std::filesystem::path("tests") / "unit" / "test_compile_run_imports_operations.cpp");
  const std::filesystem::path vmParityPath = resolveRepoPath(
      std::filesystem::path("tests") / "unit" / "test_compile_run_vm_collections_wrapper_temporaries_a.cpp");
  const std::filesystem::path nativeParityPath = resolveRepoPath(
      std::filesystem::path("tests") / "unit" /
      "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp");
  REQUIRE(std::filesystem::exists(cppParityPath));
  REQUIRE(std::filesystem::exists(vmParityPath));
  REQUIRE(std::filesystem::exists(nativeParityPath));

  const std::string cppParity = readFile(cppParityPath.string());
  const std::string vmParity = readFile(vmParityPath.string());
  const std::string nativeParity = readFile(nativeParityPath.string());
  const std::string parityProgram = R"(import /std/collections/*
import /std/collections/soa_vector/*
import /std/collections/soa_vector_conversions/*

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

  CHECK(parityProgram.find("experimental_soa_vector") == std::string::npos);
  CHECK(parityProgram.find("soaVectorNew<Particle>()") != std::string::npos);
  CHECK(parityProgram.find("reserve(values, 2i32)") != std::string::npos);
  CHECK(parityProgram.find("push(values, Particle(4i32, 6i32))") != std::string::npos);
  CHECK(parityProgram.find("get(values, 0i32)") != std::string::npos);
  CHECK(parityProgram.find("ref(values, 1i32)") != std::string::npos);
  CHECK(parityProgram.find("to_aos(values)") != std::string::npos);

  CHECK(cppParity.find(
            "TEST_CASE(\"legacy soa_vector compatibility helpers reject in C++ emitter\")") !=
        std::string::npos);
  const std::size_t cppParityProgramOffset = cppParity.find(parityProgram);
  CHECK(cppParityProgramOffset != std::string::npos);
  CHECK(cppParity.find("CHECK(runCommand(compileCmd) == 2);", cppParityProgramOffset) !=
        std::string::npos);
  CHECK(cppParity.find("meta.field_count requires struct type argument: type:Particle", cppParityProgramOffset) !=
        std::string::npos);

  CHECK(vmParity.find("TEST_CASE(\"vm legacy soa_vector compatibility helpers reject\")") !=
        std::string::npos);
  const std::size_t vmParityProgramOffset = vmParity.find(parityProgram);
  CHECK(vmParityProgramOffset != std::string::npos);
  CHECK(vmParity.find("CHECK(runCommand(runCmd) == 2);", vmParityProgramOffset) !=
        std::string::npos);
  CHECK(vmParity.find("direct import of retired soa_vector compatibility modules", vmParityProgramOffset) !=
        std::string::npos);

  CHECK(nativeParity.find("TEST_CASE(\"native legacy soa_vector compatibility helpers reject\")") !=
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
  const std::filesystem::path cppParityPath =
      resolveRepoPath(std::filesystem::path("tests") / "unit" / "test_compile_run_imports_operations.cpp");
  const std::filesystem::path vmParityPath = resolveRepoPath(
      std::filesystem::path("tests") / "unit" / "test_compile_run_vm_collections_wrapper_temporaries_a.cpp");
  const std::filesystem::path nativeParityPath = resolveRepoPath(
      std::filesystem::path("tests") / "unit" /
      "test_compile_run_native_backend_collections_experimental_maps_and_helpers.cpp");
  REQUIRE(std::filesystem::exists(testsPath));
  REQUIRE(std::filesystem::exists(examplesPath));
  REQUIRE(std::filesystem::exists(cppParityPath));
  REQUIRE(std::filesystem::exists(vmParityPath));
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
    INFO("Public examples should use /std/collections/soa/*: "
         << entry.path().lexically_relative(examplesPath).generic_string());
    CHECK(source.find("import /std/collections/soa_vector") == std::string::npos);
    CHECK(source.find("import /std/collections/experimental_soa_vector") ==
          std::string::npos);
    CHECK(source.find("soa_vector<") == std::string::npos);
  }

  const std::string cppParity = readFile(cppParityPath.string());
  const std::string vmParity = readFile(vmParityPath.string());
  const std::string nativeParity = readFile(nativeParityPath.string());
  CHECK(cppParity.find("TEST_CASE(\"compiles and runs public soa count helper") !=
        std::string::npos);
  CHECK(cppParity.find("TEST_CASE(\"compiles and runs canonical soa_vector count helper") ==
        std::string::npos);
  CHECK(cppParity.find("TEST_CASE(\"public soa to_aos explicit helper is a vector target") !=
        std::string::npos);
  CHECK(vmParity.find("TEST_CASE(\"runs vm public soa count helper") !=
        std::string::npos);
  CHECK(vmParity.find("TEST_CASE(\"runs vm canonical soa_vector count helper") ==
        std::string::npos);
  CHECK(nativeParity.find("TEST_CASE(\"compiles and runs native public soa count helper") !=
        std::string::npos);
  CHECK(nativeParity.find(
            "TEST_CASE(\"compiles and runs native canonical soa_vector count helper") ==
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
  const std::filesystem::path internalStoragePath =
      resolveRepoPath(std::filesystem::path("stdlib") / "std" / "collections" /
                      "internal_soa_storage.prime");
  const std::filesystem::path reflectionRuntimePath =
      resolveRepoPath(std::filesystem::path("tests") / "unit" /
                      "test_compile_run_reflection_codegen_runtime.cpp");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));
  REQUIRE(std::filesystem::exists(internalStoragePath));
  REQUIRE(std::filesystem::exists(reflectionRuntimePath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpecDoc = readFile(syntaxSpecPath.string());
  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());
  const std::string internalStorage = readFile(internalStoragePath.string());
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

  CHECK(internalStorage.find("[public struct]\n  SoaColumn<T>()") !=
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
  CHECK(internalStorage.find("/std/collections/soa_vector/") == std::string::npos);
  CHECK(internalStorage.find("soaVectorNew") == std::string::npos);
  CHECK(internalStorage.find("SoaVector<T>") == std::string::npos);

  CHECK(reflectionRuntime.find("reflection SoaSchema helper runtime stays aligned across backends") !=
        std::string::npos);
  CHECK(reflectionRuntime.find("reflection SoaSchema storage helper runtime stays aligned across backends") !=
        std::string::npos);
  CHECK(reflectionRuntime.find("import /std/collections/internal_soa_storage/*") !=
        std::string::npos);
  CHECK(reflectionRuntime.find("SoaSchemaStorageReserve(storage, 5i32)") !=
        std::string::npos);
  CHECK(reflectionRuntime.find("import /std/collections/soa_vector/*") ==
        std::string::npos);
}

TEST_CASE("canonical soa example stays source locked") {
  std::filesystem::path examplePath =
      std::filesystem::path("..") / "examples" / "3.Surface" / "soa_ecs.prime";
  std::filesystem::path oldExamplePath =
      std::filesystem::path("..") / "examples" / "3.Surface" / "soa_vector_ecs.prime";
  std::filesystem::path examplesReadmePath = std::filesystem::path("..") / "examples" / "README.md";
  std::filesystem::path exampleSweepPath =
      std::filesystem::path("..") / "tests" / "unit" / "test_compile_run_bindings_and_examples.cpp";
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
    exampleSweepPath = std::filesystem::current_path() / "tests" / "unit" /
                       "test_compile_run_bindings_and_examples.cpp";
  }
  REQUIRE(std::filesystem::exists(examplePath));
  CHECK(!std::filesystem::exists(oldExamplePath));
  REQUIRE(std::filesystem::exists(examplesReadmePath));
  REQUIRE(std::filesystem::exists(exampleSweepPath));

  const std::string example = readFile(examplePath.string());
  const std::string examplesReadme = readFile(examplesReadmePath.string());
  const std::string exampleSweep = readFile(exampleSweepPath.string());

  CHECK(example.find("import /std/collections/soa/*") != std::string::npos);
  CHECK(example.find("import /std/collections/soa_vector/*") ==
        std::string::npos);
  CHECK(example.find("import /std/collections/soa_vector_conversions/*") ==
        std::string::npos);
  CHECK(example.find("[struct reflect]") != std::string::npos);
  CHECK(example.find("[auto mut] particles{soa</Particle>()}") !=
        std::string::npos);
  CHECK(example.find("particles.reserve(plus(particles.count(), spawnQueue.count()))") !=
        std::string::npos);
  CHECK(example.find("soaVectorNew<Particle>()") == std::string::npos);
  CHECK(example.find("soaVectorToAos<Particle>(particles)") == std::string::npos);
  CHECK(example.find("to_aos(particles)") != std::string::npos);
  CHECK(example.find("experimental_soa_vector") == std::string::npos);
  CHECK(example.find("soa_vector_ecs") == std::string::npos);
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
      "src/ir_lowerer/IrLowererLowerEmitExprTryHelpers.h",
      "src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp",
      "src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp",
      "src/ir_lowerer/IrLowererLowerSumHelpers.h",
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
      "src/ir_lowerer/IrLowererLowerSumHelpers.h",
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
  const std::filesystem::path todoFinishedPath =
      resolveRepoPath("docs/todo_finished.md");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpec = readFile(syntaxSpecPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());

  CHECK(primeStructDoc.find("Requirements are definition transforms, not body statements") !=
        std::string::npos);
  CHECK(primeStructDoc.find("[require(typeof<left> == i32, typeof<right> == i32)]") !=
        std::string::npos);
  CHECK(primeStructDoc.find("duplicate require transform; combine predicates into one require(...)") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`typeof<left>` is a compile-time\n"
                            "  query because it uses the compile-time argument channel; `typeof(left)`\n"
                            "  remains an ordinary runtime call shape") !=
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

  CHECK(syntaxSpec.find("[require(typeof<left> == typeof<right>, "
                        "meta.has_trait<typeof<left>>(Additive))]") !=
        std::string::npos);
  CHECK(syntaxSpec.find("They live with the callable signature, run during\n"
                        "  semantic validation before final IR lowering, and do not create runtime\n"
                        "  values visible to the function body.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("A repeated transform is rejected with\n"
                        "  `duplicate require transform; combine predicates into one require(...)`.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("`typeof(left)` remains an ordinary runtime\n"
                        "  call-shaped expression and is not a requirement primitive.") !=
        std::string::npos);
  CHECK(syntaxSpec.find("simple comparisons over\n"
                        "  compile-time values such as `N > 0`") !=
        std::string::npos);
  CHECK(syntaxSpec.find("User-defined predicates distinguish `false` from invalid evaluation") !=
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

TEST_CASE("todo queue and skipped doctest debt stay source locked") {
  std::filesystem::path todoPath = std::filesystem::path("..") / "docs" / "todo.md";
  std::filesystem::path todoFinishedPath = std::filesystem::path("..") / "docs" / "todo_finished.md";
  std::filesystem::path vmMathPath = std::filesystem::path("..") / "tests" / "unit" / "test_compile_run_vm_math.cpp";
  std::filesystem::path vmMapsPath = std::filesystem::path("..") / "tests" / "unit" / "test_compile_run_vm_maps.cpp";
  std::filesystem::path examplesDocsPath =
      std::filesystem::path("..") / "tests" / "unit" / "test_compile_run_examples_docs.cpp";
  if (!std::filesystem::exists(todoPath)) {
    todoPath = std::filesystem::current_path() / "docs" / "todo.md";
  }
  if (!std::filesystem::exists(todoFinishedPath)) {
    todoFinishedPath = std::filesystem::current_path() / "docs" / "todo_finished.md";
  }
  if (!std::filesystem::exists(vmMathPath)) {
    vmMathPath = std::filesystem::current_path() / "tests" / "unit" / "test_compile_run_vm_math.cpp";
  }
  if (!std::filesystem::exists(vmMapsPath)) {
    vmMapsPath = std::filesystem::current_path() / "tests" / "unit" / "test_compile_run_vm_maps.cpp";
  }
  if (!std::filesystem::exists(examplesDocsPath)) {
    examplesDocsPath = std::filesystem::current_path() / "tests" / "unit" / "test_compile_run_examples_docs.cpp";
  }
  REQUIRE(std::filesystem::exists(todoPath));
  REQUIRE(std::filesystem::exists(todoFinishedPath));
  REQUIRE(std::filesystem::exists(vmMathPath));
  REQUIRE(std::filesystem::exists(vmMapsPath));
  REQUIRE(std::filesystem::exists(examplesDocsPath));

  const std::string todo = readFile(todoPath.string());
  const std::string todoFinished = readFile(todoFinishedPath.string());
  const std::string vmMath = readFile(vmMathPath.string());
  const std::string vmMaps = readFile(vmMapsPath.string());
  const std::string examplesDocs = readFile(examplesDocsPath.string());

  CHECK(todo.find("## Purpose") != std::string::npos);
  CHECK(todo.find("Do not keep completed-task summaries, historical rollout notes, or closed\n"
                  "  coverage snapshots in this file.") !=
        std::string::npos);
  CHECK(todo.find("### Ready Now\n\n"
                  "- TODO-4592: Map parser and semantic diagnostics through source units | track: "
                  "source-unit-provenance") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4565: Add minimal scene graph and camera data model | track: scene-renderer") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4570: Retire duplicate map2 candidate surface | track: collection-stdlib-cleanup") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4571: Add compiler-knowledge inventory for map/vector | track: collection-audit") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4572: Remove vector statement-helper compiler path | track: vector-special-case-deletion") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4573: Remove compiler-owned map literal lowering | track: map-special-case-deletion") !=
        std::string::npos);
  CHECK(todo.find("### Immediate Next 10\n\n"
                  "- TODO-4593: Carry source-unit provenance into IR and VM debug maps") !=
        std::string::npos);
  CHECK(todo.find("### Priority Lanes") != std::string::npos);
  CHECK(todo.find("Source-unit provenance ledger: TODO-4592 -> TODO-4593; TODO-4593 waits on") !=
        std::string::npos);
  CHECK(todo.find("TODO-4591\n"
                  "  completed the inspectable expanded-source ledger that this lane builds on.") !=
        std::string::npos);
  CHECK(todo.find("Scene graph renderer and UI presentation: TODO-4565 -> TODO-4566") !=
        std::string::npos);
  CHECK(todo.find("Map/vector compiler-independence: TODO-4570 and TODO-4571 can run in") !=
        std::string::npos);
  CHECK(todo.find("### Execution Queue\n\n"
                  "- TODO-4592: Map parser and semantic diagnostics through source units") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4591: Add expanded-source provenance ledger") !=
        std::string::npos);
  CHECK(todoFinished.find("Added public `ExpandedSource`, `SourceUnit`, and `SourceSegment` types") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4564: Lock scene renderer defaults and UI producer contract | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4564: Lock scene renderer defaults and UI producer contract") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4564: Lock scene renderer defaults and UI producer contract") !=
        std::string::npos);
  CHECK(todo.find("- TODO-4562: Add task handle semantic facts and lifetime diagnostics | track:") ==
        std::string::npos);
  CHECK(todo.find("### Ready Now (Parallel-Candidate Leaves; No Unmet TODO Dependencies)") ==
        std::string::npos);
  CHECK(todo.find("### Immediate Next 10 (Track Successors; Not Ready Until Dependencies Land)") ==
        std::string::npos);
  CHECK(todo.find("- `soa-zero-audit`: TODO-4524/TODO-4529 completed the strict\n"
                  "  zero-production-trace audit; no SoA zero-audit leaf is ready.") ==
        std::string::npos);
  CHECK(todo.find("- `map-zero-audit`: TODO-4537 split the broad lowerer substrate item into\n"
                  "  bounded leaves; TODO-4539 removed generated MapValue path synthesis, and\n"
                  "  TODO-4540 removed the lowerer key/value local kind plus ref/pointer flags.") ==
        std::string::npos);
  CHECK(todo.find("no map zero-audit leaf is ready.") == std::string::npos);
  CHECK(todo.find("- `tuple-type-packs`: TODO-4276 completed helper/lifecycle pack\n"
                  "  expansion, TODO-4271 added compile-time pack indexing, TODO-4272 added\n"
                  "  the initial stdlib tuple surface, and TODO-4274 added tuple bracket\n"
                  "  indexing, TODO-4273 added heterogeneous `make_tuple` inference, and\n"
                  "  TODO-4277 added tuple destructuring. TODO-4278 added task multi-wait over\n"
                  "  ordinary stdlib tuples; no tuple-type-packs leaf is ready.") ==
        std::string::npos);
  CHECK(todo.find("- `multithreading-substrate`: TODO-4545 was split into TODO-4561,\n"
                  "  TODO-4562, and TODO-4563 so single-task spawn/wait can land as parser/effect,\n"
                  "  semantic/lifetime, and runtime execution slices before TODO-4278. TODO-4561\n"
                  "  locked the parser/effect spelling, TODO-4562 added semantic/lifetime facts\n"
                  "  and diagnostics, and TODO-4563 added single-task VM/native runtime\n"
                  "  execution; no multithreading-substrate leaf is ready.") ==
        std::string::npos);
  CHECK(todo.find("- `procedural-genericity`: TODO-4336 allowed type locals in local binding and\n"
                  "  struct-field envelopes, TODO-4337 added non-escaping local generated\n"
                  "  structs, TODO-4338 stabilized deterministic generated identity and\n"
                  "  provenance, TODO-4339 lowered procedural facts through semantic-product\n"
                  "  direct-call and layout metadata, and TODO-4340 added docs and positive\n"
                  "  examples, and TODO-4546 added negative conformance; no procedural-genericity\n"
                  "  leaf is ready.") ==
        std::string::npos);
  CHECK(todo.find("- `generic-requirements`: TODO-4331, TODO-4334, TODO-4341, TODO-4342,\n"
                  "  TODO-4343, TODO-4344, TODO-4352, TODO-4353, and TODO-4354 are complete;\n"
                  "  TODO-4355 wired the compile-time host to published `/std/meta/*` predicate\n"
                  "  facts; TODO-4356 prepared restricted compile-time callables; TODO-4357\n"
                  "  evaluates pure user predicates; TODO-4345 added statement-level concrete\n"
                  "  `ct_if`; TODO-4547 added generic-specialized branch selection, and\n"
                  "  TODO-4548 added expression-position `ct_if` values; TODO-4549 scoped\n"
                  "  selected-branch generated type facts; TODO-4346 documented compile-time flow\n"
                  "  policy; TODO-4550 enforced active compile-time budget limits; TODO-4358\n"
                  "  enforced phase-qualified compile-time effects; TODO-4551 added\n"
                  "  deterministic cache keys and invalidation; TODO-4347 routed requirement\n"
                  "  facts into overload selection; and TODO-4351 added integer value\n"
                  "  requirement facts. TODO-4348 was split into bounded diagnostic leaves,\n"
                  "  TODO-4552 published provenance-rich direct requirement failures,\n"
                  "  TODO-4553 published requirement-overload diagnostics, TODO-4554 published\n"
                  "  compile-time-flow diagnostics, and TODO-4555 added predicate/value\n"
                  "  conformance coverage. TODO-4556 added focused conformance for compile-time\n"
                  "  effect opt-ins, cache invalidation material, and active budget enforcement.\n"
                  "  TODO-4557 locked compiler-hosted CT VM boundary coverage. TODO-4558 added\n"
                  "  parser and source-lock conformance for public generic requirement and\n"
                  "  `ct_if` syntax. TODO-4560 added compile-run and user-facing diagnostic\n"
                  "  conformance coverage. TODO-4559 added semantic-product and IR-preparation\n"
                  "  handoff conformance. TODO-4359 was split into TODO-4555, TODO-4556, and\n"
                  "  TODO-4557 so compile-time VM conformance could be covered in parallel by\n"
                  "  predicate/value, effects/cache/budget, and compiler-host boundary leaves.\n"
                  "  TODO-4349 was split into TODO-4558, TODO-4559, and TODO-4560 so the broader\n"
                  "  conformance matrix could proceed in parser/source-lock, semantic/IR, and\n"
                  "  compile-run/diagnostic tracks; all three split leaves are complete.") ==
        std::string::npos);
  CHECK(todo.find("### Ready Now (Parallel-Candidate Leaves; No Unmet TODO Dependencies)\n\n"
                  "- none") == std::string::npos);
  CHECK(todo.find("### Immediate Next 10 (Track Successors; Not Ready Until Dependencies Land)\n\n"
                  "- none") == std::string::npos);
  CHECK(todo.find("- TODO-4273: Add heterogeneous value-pack inference") ==
        std::string::npos);
  CHECK(todo.find("- [~] TODO-4305: Rename and style canonical `.prime` SoA surface") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4305: Rename and style canonical `.prime` SoA surface") !=
        std::string::npos);
  CHECK(todo.find("- Semantic phase contract hardening:") == std::string::npos);
  CHECK(todo.find("- Deferred graph and inference hardening: TODO-4239") ==
        std::string::npos);
  CHECK(todo.find("- Deferred semantic-product/backend/tooling follow-ups: TODO-4245") ==
        std::string::npos);
  CHECK(todo.find("Generic contiguous-storage coverage needed before vector ordinary `.prime`") ==
        std::string::npos);
  CHECK(todo.find("- Deferred SoA finish: TODO-4252") ==
        std::string::npos);
  CHECK(todo.find("### Execution Queue (Recommended Track Order)\n\n"
                  "- none") == std::string::npos);
  CHECK(todo.find("- TODO-4563: Add single-task spawn/wait runtime execution | track:") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4545: Implement first structured task spawn/wait substrate") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4545: Implement first structured task spawn/wait substrate") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4558: Add generic parser and source-lock conformance") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4559: Add generic semantic and IR conformance") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4560: Add generic compile-run diagnostics conformance") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4558: Add generic parser and source-lock conformance") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4559: Add generic semantic and IR conformance") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4560: Add generic compile-run diagnostics conformance") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4548: Add expression-position `ct_if` values") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4548: Add expression-position `ct_if` values") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4549: Scope branch-local generated type facts") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4549: Scope branch-local generated type facts") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4346: Add compile-time flow effect and termination policy") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4346: Add compile-time flow effect and termination policy") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4547: Add specialization-aware `ct_if` over type facts") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4547: Add specialization-aware `ct_if` over type facts") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4345: Add compile-time `if` over type facts") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4345: Add compile-time `if` over type facts") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4357: Evaluate pure user predicates at compile time") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4357: Evaluate pure user predicates at compile time") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4356: Add restricted compile-time callable lowering") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4356: Add restricted compile-time callable lowering") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4355: Add compile-time host and meta intrinsics") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4355: Add compile-time host and meta intrinsics") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4354: Factor reusable VM interpreter kernel") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4354: Factor reusable VM interpreter kernel") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4353: Add typed compile-time value model") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4353: Add typed compile-time value model") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4352: Add compile-time VM facade and host") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4352: Add compile-time VM facade and host") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4344: Add capability and trait support predicates") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4344: Add capability and trait support predicates") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4343: Add builtin type relation predicates") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4343: Add builtin type relation predicates") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4342: Represent requirement predicates as semantic facts") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4342: Represent requirement predicates as semantic facts") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4331: Implement compile-time argument channel model") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4331: Implement compile-time argument channel model") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4276: Expand type packs in helpers and lifecycle hooks") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4276: Expand type packs in helpers and lifecycle hooks") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4277: Add tuple destructuring sugar") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4273: Add heterogeneous value-pack inference") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4272: Add stdlib `tuple<Ts...>`") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4271: Add compile-time pack indexing") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4275: Expand type packs into struct storage") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4270: Add compile-time integer template arguments") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4269: Bind and monomorphize type-pack arguments") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4268: Add heterogeneous type-pack syntax and metadata") !=
        std::string::npos);
  CHECK(todo.find("TODO-4518: Migrate SoA compatibility fixtures") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4518: Migrate SoA compatibility fixtures") !=
        std::string::npos);
  CHECK(todo.find("TODO-4519: Delete `soa_vector` compatibility seams") ==
        std::string::npos);
  CHECK(todo.find("TODO-4521: Delete semantic SoA public-surface traces") ==
        std::string::npos);
  CHECK(todo.find("TODO-4522: Delete lowerer and emitter SoA public-surface traces") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4521: Delete semantic SoA public-surface traces") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4522: Delete lowerer and emitter SoA public-surface traces") !=
        std::string::npos);
  CHECK(todo.find("- [~] TODO-4524: Tighten SoA surface audit to zero") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4524: Tighten SoA surface audit to zero") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4525: Delete text-filter and IR-printer SoA zero-audit residue") !=
        std::string::npos);
  CHECK(todo.find("TODO-4527: Delete template-monomorph SoA zero-audit residue") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4527: Delete template-monomorph SoA zero-audit residue") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4533: Remove lowerer call-resolution SoA bridge traces") !=
        std::string::npos);
  CHECK(todo.find("TODO-4528: Reduce lowerer count-access SoA trace residue") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4528: Reduce lowerer count-access SoA trace residue") !=
        std::string::npos);
  CHECK(todo.find("TODO-4529: Replace SoA inventory with strict zero audit") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4529: Replace SoA inventory with strict zero audit") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4519: Delete `soa_vector` compatibility seams") !=
        std::string::npos);
  const std::vector<std::string> completedSemanticPhaseQueue = {
      "TODO-4351: Add value-level compile-time requirement facts",
      "TODO-4347: Integrate requirements with overload selection",
      "TODO-4358: Enforce phase-qualified compile-time effects",
      "TODO-4550: Enforce compile-time evaluation budgets",
      "TODO-4551: Add compile-time evaluation cache keys",
  };
  for (const std::string &entry : completedSemanticPhaseQueue) {
    CHECK(todo.find("- " + entry) == std::string::npos);
    CHECK(todo.find("- [ ] " + entry) == std::string::npos);
    CHECK(todoFinished.find(entry) != std::string::npos);
  }
  CHECK(todo.find("- [ ] TODO-4560: Add generic compile-run diagnostics conformance") ==
        std::string::npos);
  CHECK(todoFinished.find(
            "TODO-4560: Add generic compile-run diagnostics conformance") !=
        std::string::npos);
  CHECK(todo.find("| track: procedural-genericity-docs |") ==
        std::string::npos);
  CHECK(todoFinished.find("  - parallel_track: procedural-genericity-docs") !=
        std::string::npos);
  CHECK(todo.find("| track: procedural-genericity-negatives |") ==
        std::string::npos);
  CHECK(todo.find("TODO-4340: Add procedural generic docs and examples") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4340: Add procedural generic docs and examples") !=
        std::string::npos);
  CHECK(todo.find("TODO-4546: Add procedural generic negative conformance") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4546: Add procedural generic negative conformance") !=
        std::string::npos);
  CHECK(todo.find("TODO-4337: Add local generated nominal structs") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4337: Add local generated nominal structs") !=
        std::string::npos);
  CHECK(todo.find("TODO-4338: Stabilize generated type identity and mangling") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4338: Stabilize generated type identity and mangling") !=
        std::string::npos);
  CHECK(todo.find("TODO-4339: Lower procedural generic facts through semantics") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4339: Lower procedural generic facts through semantics") !=
        std::string::npos);
  CHECK(todo.find("TODO-4546: Add procedural generic negative conformance") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4546: Add procedural generic negative conformance") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4332: Add bare zero-arg execution syntax") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4333: Reject ambiguous value/execution names") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4334: Add compile-time `[type]` local bindings") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4335: Add `typeof<symbol>` compile-time primitive") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4336: Allow type locals in envelope positions") !=
        std::string::npos);
  CHECK(todo.find("TODO-4341: Define generic requirement predicate surface") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4341: Define generic requirement predicate surface") !=
        std::string::npos);
  CHECK(todo.find("TODO-4561: Add task spawn/wait parser and effect locks") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4561: Add task spawn/wait parser and effect locks") !=
        std::string::npos);
  CHECK(todo.find("TODO-4562: Add task handle semantic facts and lifetime diagnostics") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4562: Add task handle semantic facts and lifetime diagnostics") !=
        std::string::npos);
  CHECK(todo.find("TODO-4563: Add single-task spawn/wait runtime execution") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4563: Add single-task spawn/wait runtime execution") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4277, TODO-4563") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4278: Integrate multi-wait with stdlib tuple") !=
        std::string::npos);
  CHECK(todoFinished.find("  - depends_on: TODO-4277, TODO-4563") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4545: Implement first structured task spawn/wait substrate") !=
        std::string::npos);
  CHECK(todoFinished.find("  - depends_on: TODO-4342, TODO-4343") !=
        std::string::npos);
  CHECK(todoFinished.find("  - depends_on: TODO-4342, TODO-4335") !=
        std::string::npos);
  CHECK(todoFinished.find("  - parallel_track: soa-zero-audit") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4226") == std::string::npos);
  CHECK(todo.find("TODO-4227") == std::string::npos);
  CHECK(todo.find("TODO-4215") == std::string::npos);
  CHECK(todo.find("TODO-4214") == std::string::npos);
  CHECK(todoFinished.find("TODO-4215: Make semantic-product publication consume merged fact bundles") !=
        std::string::npos);
  CHECK(todo.find("TODO-4263: Design generic and unit sum variants") == std::string::npos);
  CHECK(todo.find("TODO-4288:") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4288") == std::string::npos);
  CHECK(todoFinished.find("TODO-4288: Add executable unit sum variants") !=
        std::string::npos);
  CHECK(todo.find("TODO-4289: Add generic sum declarations and monomorphization") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4289: Add generic sum declarations and monomorphization") !=
        std::string::npos);
  CHECK(todo.find("TODO-4264: Add stdlib-owned `Maybe<T>` sum") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4264: Add stdlib-owned `Maybe<T>` sum") !=
        std::string::npos);
  CHECK(todo.find("TODO-4265: Add stdlib-owned `Result<T, E>` sum") ==
        std::string::npos);
  CHECK(todo.find("TODO-4267: Retire legacy Maybe/Result representations") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4267: Retire legacy Maybe/Result representations") !=
        std::string::npos);
  CHECK(todo.find("TODO-4363: Retire Result helper legacy wording") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4363: Retire Result helper legacy wording") !=
        std::string::npos);
  CHECK(todo.find("TODO-4364: Fence Result compatibility helper adapters") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4364: Fence Result compatibility helper adapters") !=
        std::string::npos);
  CHECK(todo.find("TODO-4291: Decide sum-backed mutable `Maybe<T>` helpers") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4291: Decide sum-backed mutable `Maybe<T>` helpers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4292: Promote and style canonical `.prime` vector implementation") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4292: Split canonical vector promotion umbrella") !=
        std::string::npos);
  CHECK(todo.find("TODO-4365: Style canonical vector constructor wrappers") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4365: Style canonical vector constructor wrappers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4366: Route vector wrappers through canonical helpers") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4366: Route vector wrappers through canonical helpers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4294: Lower vector helpers through ordinary `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4294: Split vector ordinary `.prime` lowering") !=
        std::string::npos);
  CHECK(todo.find("TODO-4368: Route canonical vector mutator statements through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4368: Route canonical vector mutator statements through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4369: Route canonical vector read helpers through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4369: Route canonical vector read helpers through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4370: Route vector compatibility mutators through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4370: Route vector compatibility mutators through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4371: Split hard-coded vector layout lowering") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4371: Split hard-coded vector layout lowering") !=
        std::string::npos);
  CHECK(todo.find("TODO-4372: Route vector setter statements through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4372: Route vector setter statements through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4373: Route vector metadata inline helpers through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4373: Route vector metadata inline helpers through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4374: Route vector metadata expression fallbacks through `.prime`") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4374: Route vector metadata expression fallbacks through `.prime`") !=
        std::string::npos);
  CHECK(todo.find("TODO-4375: Replace vector constructor header materialization") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4375: Replace vector constructor header materialization") !=
        std::string::npos);
  CHECK(todo.find("TODO-4295: Move collection surface metadata out of C++") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4295: Move collection surface metadata out of C++") !=
        std::string::npos);
  CHECK(todo.find("TODO-4296: Stop publishing vector compatibility metadata") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4296: Stop publishing vector compatibility metadata") !=
        std::string::npos);
  CHECK(todo.find("TODO-4376: Delete vector wrapper helper aliases") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4376: Delete vector wrapper helper aliases") !=
        std::string::npos);
  CHECK(todo.find("TODO-4377: Reject rooted vector helper aliases") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4377: Reject rooted vector helper aliases") !=
        std::string::npos);
  CHECK(todo.find("TODO-4293: Bridge legacy `Result` helpers to the result sum") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4293: Bridge legacy `Result` helpers to the result sum") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4299: Bridge `Result.map2` to the result sum") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4298: Bridge `Result.and_then` to the result sum") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4297: Bridge `Result.map` to the result sum") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4296: Bridge `Result.ok` to the result sum variant") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4295: Bridge `Result.why` to the result sum payload") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4294: Bridge `Result.error` to the result sum tag") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4292: Add importable stdlib `Result<T, E>` sum") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4287: Add unit sum declaration metadata") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4227: Move semantic-product fact families into worker bundles") !=
        std::string::npos);
  CHECK(todoFinished.find("TODO-4214: Introduce deterministic worker result bundles") !=
        std::string::npos);
  CHECK(todo.find("TODO-4282: Reject call-shaped struct field construction") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4282: Reject call-shaped struct field construction") !=
        std::string::npos);
  CHECK(todo.find("TODO-4254: Migrate generated construction surfaces") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4254: Migrate generated construction surfaces") !=
        std::string::npos);
  CHECK(todo.find("TODO-4255: Migrate collection construction surfaces") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4255: Migrate collection construction surfaces") !=
        std::string::npos);
  CHECK(todo.find("TODO-4256: Classify constructor-shaped helper compatibility") ==
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4256") == std::string::npos);
  CHECK(todoFinished.find("TODO-4256: Classify constructor-shaped helper compatibility") !=
        std::string::npos);
  CHECK(todo.find("TODO-4257: Add sum declaration metadata and layout") ==
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4257") == std::string::npos);
  CHECK(todoFinished.find("TODO-4257: Add sum declaration metadata and layout") !=
        std::string::npos);
  CHECK(todo.find("TODO-4258: Add explicit sum construction") ==
        std::string::npos);
  CHECK(todoFinished.find("TODO-4258: Add explicit sum construction") !=
        std::string::npos);
  CHECK(todo.find("TODO-4259: Add inferred sum variant construction") ==
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4259") == std::string::npos);
  CHECK(todoFinished.find("TODO-4259: Add inferred sum variant construction") !=
        std::string::npos);
  CHECK(todo.find("TODO-4260: Add `pick` semantic validation") ==
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4260") == std::string::npos);
  CHECK(todoFinished.find("TODO-4260: Add `pick` semantic validation") !=
        std::string::npos);
  CHECK(todo.find("TODO-4262:") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4262") == std::string::npos);
  CHECK(todoFinished.find("TODO-4262: Add public sum-type examples") !=
        std::string::npos);
  CHECK(todo.find("TODO-4284:") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4284") == std::string::npos);
  CHECK(todoFinished.find("TODO-4284: Stabilize aggregate sum pick result copies") !=
        std::string::npos);
  CHECK(todo.find("TODO-4285:") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4285") == std::string::npos);
  CHECK(todoFinished.find("TODO-4285: Route sum moves through active payload helpers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4286:") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4286") == std::string::npos);
  CHECK(todoFinished.find("TODO-4286: Route sum drop through active payload destroy helpers") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4227") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4215") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4228") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4216") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4217") == std::string::npos);
  CHECK(todo.find("TODO-4217") == std::string::npos);
  CHECK(todoFinished.find("TODO-4217: Move stdlib compatibility rewrites behind surface adapters") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4224") == std::string::npos);
  CHECK(todo.find("TODO-4224") == std::string::npos);
  CHECK(todoFinished.find("TODO-4224: Cut over vector/map compatibility decisions to surface adapters") !=
        std::string::npos);
  CHECK(todo.find("TODO-4229") == std::string::npos);
  CHECK(todoFinished.find("TODO-4229: Cut over SoA compatibility decisions to surface adapters") !=
        std::string::npos);
  CHECK(todo.find("TODO-4230") == std::string::npos);
  CHECK(todoFinished.find("TODO-4230: Cut over gfx compatibility decisions to surface adapters") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4218") == std::string::npos);
  CHECK(todo.find("TODO-4218") == std::string::npos);
  CHECK(todoFinished.find("TODO-4218: Make local-auto graph facts the exclusive inference authority") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4231") == std::string::npos);
  CHECK(todo.find("TODO-4231") == std::string::npos);
  CHECK(todoFinished.find("TODO-4231: Make query/try/on_error graph facts the exclusive authority") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4219") == std::string::npos);
  CHECK(todo.find("TODO-4219") == std::string::npos);
  CHECK(todoFinished.find("TODO-4219: Fail closed on residual lowerer AST semantic fallbacks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4225") == std::string::npos);
  CHECK(todo.find("TODO-4225") == std::string::npos);
  CHECK(todoFinished.find("TODO-4225: Close call-target and helper-routing lowerer fallbacks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4232") == std::string::npos);
  CHECK(todo.find("TODO-4232") == std::string::npos);
  CHECK(todoFinished.find("TODO-4232: Close binding/type/effect/layout lowerer fallbacks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4233") == std::string::npos);
  CHECK(todo.find("TODO-4233") == std::string::npos);
  CHECK(todoFinished.find("TODO-4233: Close backend-adapter and source-composition fallbacks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4220") == std::string::npos);
  CHECK(todo.find("TODO-4220") == std::string::npos);
  CHECK(todoFinished.find("TODO-4220: Add semantic phase handoff conformance gates") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4234") == std::string::npos);
  CHECK(todo.find("TODO-4234") == std::string::npos);
  CHECK(todoFinished.find("TODO-4234: Add semantic budget and worker-parity release gates") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4221") == std::string::npos);
  CHECK(todo.find("TODO-4221") == std::string::npos);
  CHECK(todoFinished.find("TODO-4221: Retire stale semantic validator source locks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4235") == std::string::npos);
  CHECK(todo.find("TODO-4235") == std::string::npos);
  CHECK(todoFinished.find("TODO-4235: Retire remaining semantic/lowerer architecture source locks") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4236") == std::string::npos);
  CHECK(todo.find("TODO-4236") == std::string::npos);
  CHECK(todoFinished.find("TODO-4236: Define graph invalidation contracts by edit family") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4237") == std::string::npos);
  CHECK(todo.find("TODO-4237") == std::string::npos);
  CHECK(todoFinished.find("TODO-4237: Add graph invalidation fan-out regression tests") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4238") == std::string::npos);
  CHECK(todo.find("TODO-4238") == std::string::npos);
  CHECK(todoFinished.find("TODO-4238: Pin the CT-eval graph and semantic-product boundary") !=
        std::string::npos);
  CHECK(todo.find("TODO-4239") == std::string::npos);
  CHECK(todoFinished.find("TODO-4239: Migrate helper-routing template inference onto graph facts") !=
        std::string::npos);
  CHECK(todo.find("TODO-4216") == std::string::npos);
  CHECK(todoFinished.find("TODO-4216: Split semantic rewrites into an explicit pass manifest") !=
        std::string::npos);
  CHECK(todo.find("TODO-4228") == std::string::npos);
  CHECK(todoFinished.find("TODO-4228: Factor and version the semantic-product boundary API") !=
        std::string::npos);
  CHECK(todo.find("TODO-4240") == std::string::npos);
  CHECK(todoFinished.find("TODO-4240: Add backend semantic-product conformance coverage") !=
        std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4240") == std::string::npos);
  CHECK(todo.find("TODO-4241") == std::string::npos);
  CHECK(todoFinished.find("TODO-4241: Retire semantic-product output compatibility callers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4279") == std::string::npos);
  CHECK(todoFinished.find("TODO-4279: Retire compile-pipeline helper compatibility callers") !=
        std::string::npos);
  CHECK(todo.find("TODO-4242") == std::string::npos);
  CHECK(todoFinished.find("TODO-4242: Inventory repo-wide source-lock replacement candidates") !=
        std::string::npos);
  CHECK(todo.find("TODO-4243") == std::string::npos);
  CHECK(todoFinished.find("TODO-4243: Improve focused backend rerun ergonomics") !=
        std::string::npos);
  CHECK(todo.find("TODO-4244") == std::string::npos);
  CHECK(todoFinished.find("TODO-4244: Decide the `soa_vector` maturity exit") !=
        std::string::npos);
  CHECK(todo.find("TODO-4245") == std::string::npos);
  CHECK(todoFinished.find("TODO-4245: Plan dynamic vector growth and runtime storage support") !=
        std::string::npos);
  CHECK(todo.find("TODO-4253") == std::string::npos);
  CHECK(todoFinished.find("TODO-4253: Add brace constructor argument-list path") !=
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
  CHECK(todo.find("  - depends_on: TODO-4244") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4246") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4247") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4248") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4249") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4250") == std::string::npos);
  CHECK(todo.find("  - depends_on: TODO-4251") == std::string::npos);
  CHECK(todo.find("- TODO-4162") == std::string::npos);
  CHECK(todo.find("- TODO-4165") == std::string::npos);
  CHECK(todo.find("- TODO-4159") == std::string::npos);
  CHECK(todo.find("- TODO-4160") == std::string::npos);
  CHECK(todo.find("- TODO-4161") == std::string::npos);
  CHECK(todo.find("- TODO-4163") == std::string::npos);
  CHECK(todo.find("- Skipped doctest debt: TODO-4107") ==
        std::string::npos);
  CHECK(todo.find("- Release-gate stability and test-suite audit follow-up:") ==
        std::string::npos);
  CHECK(todo.find("- TODO-4158") == std::string::npos);
  CHECK(todo.find("- TODO-4169") == std::string::npos);
  CHECK(todo.find("- TODO-4167") == std::string::npos);
  CHECK(todo.find("- TODO-4152") == std::string::npos);
  CHECK(todo.find("- TODO-4151") == std::string::npos);
  CHECK(todo.find("- TODO-4147") == std::string::npos);
  CHECK(todo.find("Architecture hardening backlog: TODO-4580, TODO-4581 -> TODO-4582,") !=
        std::string::npos);
  CHECK(todo.find("TODO-4580: Replace private source-lock tests with public contracts") !=
        std::string::npos);
  CHECK(todo.find("TODO-4588: Add pass/phase invalidation manifest beyond semantics") !=
        std::string::npos);
  CHECK(todo.find("| Semantic ownership boundary and graph/local-auto authority | none |") ==
        std::string::npos);
  CHECK(todo.find("| Compile-pipeline stage and publication-boundary contracts | none |") ==
        std::string::npos);
  CHECK(todo.find("| Compile-time macro hooks and AST transform ownership | none |") ==
        std::string::npos);
  CHECK(todo.find("| Stdlib bridge consolidation and collection/file/gfx surface authority | none |") ==
        std::string::npos);
  CHECK(todo.find("| Vector/map stdlib ownership cutover and collection surface authority | none |") ==
        std::string::npos);
  CHECK(todo.find("| Release benchmark/example suite stability and doctest governance | none |") ==
        std::string::npos);
  CHECK(todo.find("| Focused backend rerun ergonomics and suite partitioning | none |") ==
        std::string::npos);
  CHECK(todo.find("| Test-suite audit follow-up and release-gate stability | none |") ==
        std::string::npos);
  CHECK(todo.find("| Stdlib de-experimentalization and public/internal namespace cleanup | none |") ==
        std::string::npos);
  CHECK(todo.find("| SoA maturity and `soa` public-surface rename | none |") ==
        std::string::npos);
  CHECK(todo.find("| De-experimentalization surface and namespace parity | none |") ==
        std::string::npos);
  CHECK(todo.find("| `soa` maturity and canonical surface parity | none |") ==
        std::string::npos);
  CHECK(todo.find("| Validator entrypoint and benchmark-plumbing split | none |") ==
        std::string::npos);
  CHECK(todo.find("| Semantic-product publication by module and fact family | none |") ==
        std::string::npos);
  CHECK(todo.find("| Semantic-product public API factoring and versioning | none |") ==
        std::string::npos);
  CHECK(todo.find("| IR lowerer compile-unit breakup | none |") ==
        std::string::npos);
  CHECK(todo.find("| Backend validation/build ergonomics | none |") ==
        std::string::npos);
  CHECK(todo.find("| Semantic-product-authority conformance | none |") ==
        std::string::npos);
  CHECK(todo.find("| AST transform hook conformance | none |") ==
        std::string::npos);
  CHECK(todo.find("| Compile-pipeline stage handoff conformance | none |") ==
        std::string::npos);
  CHECK(todo.find("| Architecture contract probe migration | none |") ==
        std::string::npos);
  CHECK(todo.find("| Semantic-product publication parity and deterministic ordering | none |") ==
        std::string::npos);
  CHECK(todo.find("| Lowerer/source-composition contract coverage | none |") ==
        std::string::npos);
  CHECK(todo.find("| Vector/map bridge parity for imports, rewrites, and lowering | none |") ==
        std::string::npos);
  CHECK(todo.find("### Skipped Doctest Debt Summary") == std::string::npos);
  CHECK(todo.find("Retained `doctest::skip(true)` coverage is currently absent from the active") ==
        std::string::npos);
  CHECK(todo.find("queue because no skipped doctest cases remain under `tests/unit`.") ==
        std::string::npos);
  CHECK(todo.find("New skipped doctest coverage must create a new explicit TODO before it lands.") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4146: Split or optimize slow serialization shards") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4145: Reconcile stale wrapper-helper audit expectations") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4117:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4118:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4110:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4106: Re-enable or prune skipped collection compatibility suites") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4123: Split validator core from publication and benchmark shadows") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4122: Publish indexed graph-backed facts for lowering") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4107: Re-enable or prune residual skipped IR, docs, and smoke suites") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4132: Add `==` support for reflected `Equal` helpers") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4133: Let `generate(...)` imply reflection enablement") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4134: Accept type-qualified dot-call syntax for static helpers") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4105:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4109:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4112:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4114:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4116:") == std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4142: Retire brittle architecture source locks after contract") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4123: Split validator core from publication and benchmark shadows.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4118: Re-enable or prune remaining VM non-i32 map key runtime skips.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4107: Re-enable or prune residual skipped IR, docs, and smoke suites.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4132: Add `==` support for reflected `Equal` helpers.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4122: Publish indexed graph-backed facts for lowering.") !=
        std::string::npos);
  CHECK(todoFinished.find("- [x] TODO-4158: Introduce a narrow lowerer syntax and provenance view") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4133: Let `generate(...)` imply reflection enablement.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4134: Accept type-qualified dot-call syntax for static helpers.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4117: Re-enable or prune remaining VM numeric map indexing sugar skip.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4110: Re-enable or prune remaining VM support-matrix math skips.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4116: Narrow remaining VM numeric-key map skip debt into explicit blocker-owned leaves.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4115: Prune stale skipped VM argv-key map indexing duplicate.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4113: Prune stale skipped VM string-key map duplicates.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4111: Prune stale skipped legacy VM map helper duplicates.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4108: Prune stale skipped VM scalar math helper coverage.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4104: Restore skipped doctest debt tracking in the active queue.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4106: Re-enable or prune skipped collection compatibility suites.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4197: Lock skipped doctest debt policy.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4212: Introduce semantic validation plan prepass.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4213: Route definition workers through the shared validation plan.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4222: Route execution validation through the shared plan.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4226: Add a structured semantic diagnostic/result sink.") !=
        std::string::npos);

  CHECK(vmMath.find("TEST_CASE(\"runs vm with qualified math names\")") != std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"rejects vm support-matrix math nominal helpers\")") !=
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm quaternion multiply and rotation helpers\")") !=
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm matrix composition order helpers\")") !=
        std::string::npos);
  CHECK(vmMath.find("doctest::skip(true)") == std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math abs/sign/min/max\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math saturate/lerp\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math clamp\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math pow\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math trig helpers\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm with math exp/log\" * doctest::skip(true))") ==
        std::string::npos);

  CHECK(vmMaps.find("TEST_CASE(\"rejects vm map indexing sugar without canonical helper\")") !=
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"rejects vm bool map access helpers without canonical helper\")") !=
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"rejects vm u64 map access helpers without canonical helper\")") !=
        std::string::npos);
  CHECK(vmMaps.find("doctest::skip(true)") == std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"rejects vm map indexing with argv key\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"runs vm with string-valued map literals\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"runs vm with string-keyed map literals\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"runs vm with string-keyed map binding lookup\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"runs vm with map at helper\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"vm map at rejects missing key\" * doctest::skip(true))") ==
        std::string::npos);
  CHECK(vmMaps.find("TEST_CASE(\"rejects vm map lookup with argv string key\" * doctest::skip(true))") ==
        std::string::npos);

  CHECK(examplesDocs.find("TEST_CASE(\"collection docs snippets stay code-examples style and executable\")") !=
        std::string::npos);
  CHECK(examplesDocs.find(
            "TEST_CASE(\"collection docs snippets stay code-examples style and executable\" * doctest::skip(true))") ==
        std::string::npos);
}

TEST_CASE("constructor-shaped compatibility inventory stays source locked") {
  const std::filesystem::path primeStructPath = resolveRepoPath("docs/PrimeStruct.md");
  const std::filesystem::path syntaxSpecPath =
      resolveRepoPath("docs/PrimeStruct_SyntaxSpec.md");
  const std::filesystem::path fileLowererPath = resolveRepoPath(
      "tests/unit/"
      "test_ir_pipeline_validation_ir_lowerer_inference_get_return_info_step_reports_missing_definitions.cpp");
  const std::filesystem::path gfxSemanticsPath =
      resolveRepoPath("tests/unit/test_semantics_imports_gfx.h");
  const std::filesystem::path maybeSemanticsPath =
      resolveRepoPath("tests/unit/test_semantics_maybe.cpp");
  const std::filesystem::path collectionSnapshotPath =
      resolveRepoPath("tests/unit/test_semantics_type_resolution_graph_snapshots.cpp");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(fileLowererPath));
  REQUIRE(std::filesystem::exists(gfxSemanticsPath));
  REQUIRE(std::filesystem::exists(maybeSemanticsPath));
  REQUIRE(std::filesystem::exists(collectionSnapshotPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpecDoc = readFile(syntaxSpecPath.string());
  const std::string fileLowerer = readFile(fileLowererPath.string());
  const std::string gfxSemantics = readFile(gfxSemanticsPath.string());
  const std::string maybeSemantics = readFile(maybeSemanticsPath.string());
  const std::string collectionSnapshot = readFile(collectionSnapshotPath.string());

  CHECK(primeStructDoc.find("## Constructor-Shaped Compatibility Inventory") !=
        std::string::npos);
  CHECK(primeStructDoc.find(
            "value construction uses braces (`Type{...}` or a context-typed `{...}`)") !=
        std::string::npos);
  CHECK(primeStructDoc.find(
            "imported `File<Mode>(path)` is retained as compatibility helper syntax") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`Window(...)`, `Device()`, and `Buffer<T>(count)` "
                            "are retained compatibility entry points") !=
        std::string::npos);
  CHECK(primeStructDoc.find(
            "Legacy call-shaped `array<T>(...)`, `vector<T>(...)`, `map<K, V>(...)`,") !=
        std::string::npos);
  CHECK(primeStructDoc.find(
            "`Maybe<T>` is now a stdlib-owned sum with `none` and `some` variants") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`Maybe<T>{value}` is accepted when the `some` payload is the only matching variant") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("Current call-shaped vector helpers remain "
                           "compatibility helper surfaces;") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("constructor-shaped compatibility helper surface "
                           "rewrites through stdlib") !=
        std::string::npos);
  CHECK(syntaxSpecDoc.find("It is not value construction syntax.") !=
        std::string::npos);

  CHECK(fileLowerer.find("TEST_CASE(\"ir lowerer inference expr-kind dispatch "
                         "infers try from namespaced File constructors\")") !=
        std::string::npos);
  CHECK(gfxSemantics.find("TEST_CASE(\"canonical gfx window constructor entry "
                          "point validates through stdlib helper\")") !=
        std::string::npos);
  CHECK(gfxSemantics.find("TEST_CASE(\"canonical gfx device constructor entry "
                          "point validates through stdlib helper\")") !=
        std::string::npos);
  CHECK(gfxSemantics.find("TEST_CASE(\"canonical gfx Buffer allocation helper "
                          "validates through builtin rewrite\")") !=
        std::string::npos);
  CHECK(maybeSemantics.find("TEST_CASE(\"maybe some constructs present sum value\")") !=
        std::string::npos);
  CHECK(maybeSemantics.find("TEST_CASE(\"maybe target-typed initializer accepts "
                            "unique inferred payload\")") !=
        std::string::npos);
  CHECK(maybeSemantics.find("TEST_CASE(\"maybe mutable struct helpers are "
                            "retired on sum values\")") !=
        std::string::npos);
  CHECK(collectionSnapshot.find(
            "TEST_CASE(\"semantic product publishes graph-backed collection "
            "constructor local-auto surface ids\")") !=
        std::string::npos);
  CHECK(collectionSnapshot.find("StdlibSurfaceId::CollectionsVectorConstructors") !=
        std::string::npos);
  CHECK(collectionSnapshot.find("findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") !=
        std::string::npos);
  CHECK(collectionSnapshot.find("StdlibSurfaceId::CollectionsColumnarConstructors") !=
        std::string::npos);
}

TEST_CASE("scene renderer ui producer contract stays source locked") {
  std::filesystem::path graphicsDocPath = std::filesystem::path("..") / "docs" / "Graphics_API_Design.md";
  std::filesystem::path specDocPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path bridgePath =
      std::filesystem::path("..") / "examples" / "shared" / "software_surface_bridge.h";
  if (!std::filesystem::exists(graphicsDocPath)) {
    graphicsDocPath = std::filesystem::current_path() / "docs" / "Graphics_API_Design.md";
  }
  if (!std::filesystem::exists(specDocPath)) {
    specDocPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(bridgePath)) {
    bridgePath = std::filesystem::current_path() / "examples" / "shared" / "software_surface_bridge.h";
  }
  REQUIRE(std::filesystem::exists(graphicsDocPath));
  REQUIRE(std::filesystem::exists(specDocPath));
  REQUIRE(std::filesystem::exists(bridgePath));

  const std::string graphicsDoc = readFile(graphicsDocPath.string());
  const std::string specDoc = readFile(specDocPath.string());
  const std::string bridge = readFile(bridgePath.string());

  CHECK(graphicsDoc.find("## Locked Scene Renderer and UI Producer Contract (vNext)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("UI is a scene producer, not a special-purpose renderer") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`/std/ui/CommandList.serialize()` stream remains a stable adapter path") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`Scene`: insertion-ordered node storage") != std::string::npos);
  CHECK(graphicsDoc.find("`Node`: stable node id, parent id, local `Transform`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`Transform`: local translation/scale/rotation metadata") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`Camera`: a projection mode plus projection config") !=
        std::string::npos);
  CHECK(graphicsDoc.find("Orthographic projection\n  is the first supported mode") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`Material`: owns base color and opacity for a primitive") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`Light`: renderer-owned lighting data") != std::string::npos);
  CHECK(graphicsDoc.find("Primitive descriptors: flat rects, rounded rects, text overlays") !=
        std::string::npos);
  CHECK(graphicsDoc.find("one scene unit to\none logical pixel") != std::string::npos);
  CHECK(graphicsDoc.find("top-left origin") != std::string::npos);
  CHECK(graphicsDoc.find("`+x` points\nright") != std::string::npos);
  CHECK(graphicsDoc.find("`+y` points down") != std::string::npos);
  CHECK(graphicsDoc.find("base plane is `z=0`") != std::string::npos);
  CHECK(graphicsDoc.find("positive local `z`\nraises visual geometry toward the viewer") !=
        std::string::npos);
  CHECK(graphicsDoc.find("UI hit testing remains rect/layout\nbased") != std::string::npos);
  CHECK(graphicsDoc.find("Materials own color") != std::string::npos);
  CHECK(graphicsDoc.find("opacity `1.0`, shade strength `1.0`, no texture slots") !=
        std::string::npos);
  CHECK(graphicsDoc.find("2D SDFs produce coverage masks painted source-over") !=
        std::string::npos);
  CHECK(graphicsDoc.find("3D SDFs may blend geometry, depth, and normals only when each SDF has\n"
                         "  an explicit material assignment") != std::string::npos);
  CHECK(graphicsDoc.find("Text remains a 2D overlay/primitive") != std::string::npos);
  CHECK(graphicsDoc.find("international\n  shaping, bidi ordering, fallback fonts") !=
        std::string::npos);
  CHECK(graphicsDoc.find("deterministic glyph atlas/raster\n  output") != std::string::npos);
  CHECK(graphicsDoc.find("HarfBuzz-class shaping") != std::string::npos);
  CHECK(graphicsDoc.find("FreeType-class glyph loading/rasterization") != std::string::npos);
  CHECK(graphicsDoc.find("ICU/FriBidi-class Unicode bidi/boundary service") !=
        std::string::npos);
  CHECK(graphicsDoc.find("ambient weight `0.55`, key weight\n  `0.45`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("key direction from upper-left/front") != std::string::npos);
  CHECK(graphicsDoc.find("no shadows, no stochastic\n  sampling, and no author lights") !=
        std::string::npos);
  CHECK(graphicsDoc.find("painter order emitted by the UI producer is primary") !=
        std::string::npos);
  CHECK(graphicsDoc.find("equal-depth ties resolve by stable node id, then\nprimitive sub-order") !=
        std::string::npos);
  CHECK(graphicsDoc.find("Scene producer and command-list adapter layer") !=
        std::string::npos);
  CHECK(graphicsDoc.find("UI rect/state/event logic emits scene nodes for presentation") !=
        std::string::npos);
  CHECK(graphicsDoc.find("Rounded rectangles are expressed through SDF-style scene primitive\n"
                         "     descriptors") != std::string::npos);
  CHECK(graphicsDoc.find("only a UI-specific software renderer") == std::string::npos);
  CHECK(graphicsDoc.find("Base renderer layer:\n   - Consumes a flat draw-command list") ==
        std::string::npos);

  CHECK(specDoc.find("The scene renderer boundary is now locked as a UI producer contract") !=
        std::string::npos);
  CHECK(specDoc.find("rather\n  than a UI-specific software renderer") != std::string::npos);
  CHECK(specDoc.find("renderer-facing `Scene`, `Node`, `Transform`,\n"
                     "  `Camera`, `Material`, `Light`, and primitive descriptor concepts") !=
        std::string::npos);
  CHECK(specDoc.find("orthographic `Camera` projection config") != std::string::npos);
  CHECK(specDoc.find("one scene unit\n  to one logical pixel") != std::string::npos);
  CHECK(specDoc.find("painter order is primary, then local `z`, then stable node id") !=
        std::string::npos);
  CHECK(specDoc.find("HarfBuzz-class, FreeType-class, and ICU/FriBidi-class wrappers") !=
        std::string::npos);

  CHECK(bridge.find("Presentation transport for scene-renderer or legacy command-list adapter output") !=
        std::string::npos);
}

TEST_CASE("ui command list adapter docs stay source locked") {
  std::filesystem::path graphicsDocPath = std::filesystem::path("..") / "docs" / "Graphics_API_Design.md";
  std::filesystem::path specDocPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  if (!std::filesystem::exists(graphicsDocPath)) {
    graphicsDocPath = std::filesystem::current_path() / "docs" / "Graphics_API_Design.md";
  }
  if (!std::filesystem::exists(specDocPath)) {
    specDocPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  REQUIRE(std::filesystem::exists(graphicsDocPath));
  REQUIRE(std::filesystem::exists(specDocPath));

  const std::string graphicsDoc = readFile(graphicsDocPath.string());
  const std::string specDoc = readFile(specDocPath.string());

  CHECK(graphicsDoc.find("The initial stdlib prototype lives under `/std/ui/*`") != std::string::npos);
  CHECK(graphicsDoc.find("`Rgba8`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_text(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_rounded_rect(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.push_clip(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.pop_clip()`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.clip_depth()`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree`") != std::string::npos);
  CHECK(graphicsDoc.find("`LoginFormNodes`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_root_column(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_column(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_leaf(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_label(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_button(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_input(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_panel(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_login_form(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.measure()`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.arrange(x, y, width, height)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.serialize() -> vector<i32>`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_label(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_button(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_input(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.begin_panel(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.end_panel()`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_login_form(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_panel(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_label(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_button(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_input(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.bind_event(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_login_form(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.serialize() -> vector<i32>`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_pointer_move(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_pointer_down(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_pointer_up(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_key_down(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_key_up(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_ime_preedit(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_ime_commit(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_resize(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_focus_gained(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_focus_lost(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.event_count()`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.serialize() -> vector<i32>`") != std::string::npos);
  CHECK(graphicsDoc.find("First word: format version (`1`)") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `draw_text`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `draw_rounded_rect`") != std::string::npos);
  CHECK(graphicsDoc.find("`3` = `push_clip`") != std::string::npos);
  CHECK(graphicsDoc.find("`4` = `pop_clip`") != std::string::npos);
  CHECK(graphicsDoc.find("Current prototype serialization format for `/std/ui/HtmlCommandList.serialize()`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`1` = `emit_panel`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `emit_label`") != std::string::npos);
  CHECK(graphicsDoc.find("`3` = `emit_button`") != std::string::npos);
  CHECK(graphicsDoc.find("`4` = `emit_input`") != std::string::npos);
  CHECK(graphicsDoc.find("`5` = `bind_event`") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `click`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `input`") != std::string::npos);
  CHECK(graphicsDoc.find("Current prototype serialization format for `/std/ui/UiEventStream.serialize()`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`1` = `pointer_move`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `pointer_down`") != std::string::npos);
  CHECK(graphicsDoc.find("`3` = `pointer_up`") != std::string::npos);
  CHECK(graphicsDoc.find("`4` = `key_down`") != std::string::npos);
  CHECK(graphicsDoc.find("`5` = `key_up`") != std::string::npos);
  CHECK(graphicsDoc.find("`6` = `ime_preedit`") != std::string::npos);
  CHECK(graphicsDoc.find("`7` = `ime_commit`") != std::string::npos);
  CHECK(graphicsDoc.find("`8` = `resize`") != std::string::npos);
  CHECK(graphicsDoc.find("`9` = `focus_gained`") != std::string::npos);
  CHECK(graphicsDoc.find("`10` = `focus_lost`") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `shift`, `2` = `control`, `4` = `alt`, `8` = `meta`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`pop_clip()` at depth `0` is a deterministic no-op") != std::string::npos);
  CHECK(graphicsDoc.find("Single-root flat tree; nodes are appended in parent-before-child") != std::string::npos);
  CHECK(graphicsDoc.find("`measure()` walks reverse insertion order") != std::string::npos);
  CHECK(graphicsDoc.find("`arrange(x, y, width, height)` assigns the root rectangle") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `leaf`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `column`") != std::string::npos);
  CHECK(graphicsDoc.find("`append_label(...)` creates a leaf whose measured width is") != std::string::npos);
  CHECK(graphicsDoc.find("`append_button(...)` creates a leaf whose measured size is the label") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`append_input(...)` creates a leaf whose measured height matches the") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_button(...)` emits one rounded rect followed by one text command") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_input(...)` emits one rounded rect followed by one text command") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`append_panel(...)` creates a column-backed container node") != std::string::npos);
  CHECK(graphicsDoc.find("`begin_panel(...)` emits one rounded rect for the panel background") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`end_panel()` emits one balancing `pop_clip()`") != std::string::npos);
  CHECK(graphicsDoc.find("`append_login_form(...) -> LoginFormNodes` is the first composite widget") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_login_form(...)` emits only through `begin_panel`, `draw_label`,") !=
        std::string::npos);
  CHECK(graphicsDoc.find("Composite widget helpers in this prototype must not call raw") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_text`, `draw_rounded_rect`, `push_clip`, `pop_clip`, `append_leaf`,") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`emit_login_form(...)` emits only through `emit_panel`, `emit_label`,") !=
        std::string::npos);
  CHECK(graphicsDoc.find("Composite HTML adapter helpers in this prototype must not call raw") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`append_word`, `append_color`, or `append_string` directly.") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_pointer_move(...)`, `push_pointer_down(...)`, and `push_pointer_up(...)` normalize through one pointer") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_key_down(...)` and `push_key_up(...)` normalize through one key event record shape") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_ime_preedit(...)` and `push_ime_commit(...)` normalize through one IME event record shape") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_resize(...)`, `push_focus_gained(...)`, and `push_focus_lost(...)` normalize through one view event record") !=
        std::string::npos);
  CHECK(graphicsDoc.find("can upload a deterministic BGRA8 software surface into a shared Metal") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`--software-surface-demo`") != std::string::npos);
  CHECK(graphicsDoc.find("[CommandList mut] commands{CommandList{}}") != std::string::npos);
  CHECK(graphicsDoc.find("tree.append_root_column(2i32, 3i32, 10i32, 4i32)") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_label(root, 10i32, \"Hi\"utf8)") != std::string::npos);
  CHECK(graphicsDoc.find("commands.draw_button(") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_panel(root, 2i32, 1i32, 20i32, 12i32)") != std::string::npos);
  CHECK(graphicsDoc.find("commands.begin_panel(layout, panel, 4i32") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("commands.draw_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("[HtmlCommandList mut] html{HtmlCommandList{}}") != std::string::npos);
  CHECK(graphicsDoc.find("html.emit_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("[UiEventStream mut] events{UiEventStream{}}") != std::string::npos);
  CHECK(graphicsDoc.find("events.push_pointer_down(login.submitButton, 7i32, 1i32, 20i32, 30i32)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_key_down(login.usernameInput, 13i32, 3i32, 1i32)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_ime_preedit(login.usernameInput, 1i32, 4i32, \"al|\"utf8)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_ime_commit(login.usernameInput, \"alice\"utf8)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_resize(login.panel, 40i32, 57i32)") != std::string::npos);
  CHECK(graphicsDoc.find("events.push_focus_gained(login.usernameInput)") != std::string::npos);
  CHECK(graphicsDoc.find("events.push_focus_lost(login.usernameInput)") != std::string::npos);
  CHECK(specDoc.find("deterministic HTML/backend adapter records and deterministic platform input records") !=
        std::string::npos);
  CHECK(specDoc.find("`emit_input`, `bind_event`, `emit_login_form`, `UiEventStream`, `push_pointer_move`, `push_pointer_down`,") !=
        std::string::npos);
  CHECK(specDoc.find("host bridge can blit a deterministic BGRA8 software") !=
        std::string::npos);
  CHECK(specDoc.find("emit deterministic HTML/backend adapter records and normalize pointer, keyboard, IME, resize, and focus input into") !=
        std::string::npos);
  CHECK(specDoc.find("deterministic UI event-stream records") !=
        std::string::npos);
}

TEST_CASE("image api docs and stdlib stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path imageStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "image" / "image.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(imageStdlibPath)) {
    imageStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "image" / "image.prime";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(imageStdlibPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string imageStdlib = readFile(imageStdlibPath.string());

  CHECK(primeStructDoc.find("the shared image file-I/O API currently lives under `/std/image/*`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.read(width, height, pixels, path) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.write(path, width, height, pixels) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.read(width, height, pixels, path) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write(path, width, height, pixels) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`pixels` is a flat `vector<i32>` in RGB byte") != std::string::npos);
  CHECK(primeStructDoc.find("image file I/O follows `File<...>` behavior: `ppm.read(...)` and `png.read(...)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("require `effects(file_read, heap_alloc)` because they reset/materialize the pixel buffer") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write(...)` require `effects(file_write)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`file_write` also implies `file_read` for compatibility") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.read(...)` currently parses ASCII `P3` and binary `P6` PPM files in VM/native/Wasm") !=
        std::string::npos);
  CHECK(primeStructDoc.find("read contract now compiles through target validation") !=
        std::string::npos);
  CHECK(primeStructDoc.find("overflowed read-side size arithmetic") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.write(...)` now emits ASCII `P3` PPM files in") !=
        std::string::npos);
  CHECK(primeStructDoc.find("invalid dimensions, payload mismatches, overflowed write-side size") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.read(...)` now validates PNG signatures/chunks,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("current PNG read subset for both non-interlaced and Adam7-interlaced images:") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The shared decoder accepts a single") !=
        std::string::npos);
  CHECK(primeStructDoc.find("dynamic-Huffman reads") !=
        std::string::npos);
  CHECK(primeStructDoc.find("reads materialize the public flat RGB buffer") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Malformed or missing PNGs,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write(...)` now emits non-interlaced 8-bit RGB PNG files in VM/native/Wasm") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ImageError.why()` currently returns") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ContainerError.missingKey()`, `ContainerError.indexOutOfBounds()`,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The image stdlib layer also defines `/ImageError/why([ImageError] err)` as the public wrapper over the") !=
        std::string::npos);
  CHECK(primeStructDoc.find("print_line(Result.why(ppm.read(width, height, pixels, \"input.ppm\"utf8)))") !=
        std::string::npos);
  CHECK(primeStructDoc.find("print_line(Result.why(png.write(\"output.png\"utf8, width, height, pixels)))") !=
        std::string::npos);

  const size_t resetReadOutputsStart = imageStdlib.find("resetReadOutputs(");
  const size_t imageRgbWritePixelCountStart =
      imageStdlib.find("\n  [return<bool>]\n  imageRgbWritePixelCount(", resetReadOutputsStart);
  REQUIRE(resetReadOutputsStart != std::string::npos);
  REQUIRE(imageRgbWritePixelCountStart != std::string::npos);
  REQUIRE(imageRgbWritePixelCountStart > resetReadOutputsStart);
  const std::string resetReadOutputsBody =
      imageStdlib.substr(resetReadOutputsStart, imageRgbWritePixelCountStart - resetReadOutputsStart);

  CHECK(imageStdlib.find("[public struct]\n  ImageError()") != std::string::npos);
  CHECK(imageStdlib.find("ImageError{1i32}") != std::string::npos);
  CHECK(imageStdlib.find("ImageError{2i32}") != std::string::npos);
  CHECK(imageStdlib.find("ImageError{3i32}") != std::string::npos);
  CHECK(imageStdlib.find("\"image_read_unsupported\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("\"image_write_unsupported\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("\"image_invalid_operation\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("namespace ppm") != std::string::npos);
  CHECK(imageStdlib.find("namespace png") != std::string::npos);
  CHECK(imageStdlib.find("ppmReadAsciiInt") != std::string::npos);
  CHECK(imageStdlib.find("ppmReadBinaryRasterLead") != std::string::npos);
  CHECK(imageStdlib.find("imageRgbWritePixelCount") != std::string::npos);
  CHECK(imageStdlib.find("ppmWriteInputValid") != std::string::npos);
  CHECK(imageStdlib.find("ppmWriteHeader") != std::string::npos);
  CHECK(imageStdlib.find("ppmWriteComponent") != std::string::npos);
  CHECK(imageStdlib.find("pngInflateFixedHuffmanBlock") != std::string::npos);
  CHECK(imageStdlib.find("pngBuildFixedDistanceLengths") != std::string::npos);
  CHECK(imageStdlib.find("pngInflateCopyFromOutput") != std::string::npos);
  CHECK(imageStdlib.find("pngPackedSampleAt") != std::string::npos);
  CHECK(imageStdlib.find("pngScanlineBytesValid") != std::string::npos);
  CHECK(imageStdlib.find("pngScalePackedSampleToByte") != std::string::npos);
  CHECK(imageStdlib.find("pngScaleU16SampleToByte") != std::string::npos);
  CHECK(imageStdlib.find("The codec and deflate helpers below intentionally keep explicit") != std::string::npos);
  CHECK(imageStdlib.find("public-facing image wrapper layer\n  // above, which should prefer the readable surface syntax when possible.") !=
        std::string::npos);
  CHECK(resetReadOutputsBody.find("/std/collections/vector/clear(pixels)") != std::string::npos);
  CHECK(resetReadOutputsBody.find("pixels.clear()") == std::string::npos);
  CHECK(imageStdlib.find("pngScanlineChannelByte") != std::string::npos);
  CHECK(imageStdlib.find("pngAdam7PassStartX") != std::string::npos);
  CHECK(imageStdlib.find("pngDecodeRows") != std::string::npos);
  CHECK(imageStdlib.find("pngChunkIsPlte(") != std::string::npos);
  CHECK(imageStdlib.find("pngChunkCrcMatches(") != std::string::npos);
  CHECK(imageStdlib.find("[public return<Result<ImageError>> effects(file_read, heap_alloc)]\n    read([i32 mut] width, [i32 mut] height, [vector<i32> mut] pixels, [string] path)") !=
        std::string::npos);
  CHECK(imageStdlib.find("[public return<Result<ImageError>> effects(file_write)]\n    write([string] path, [i32] width, [i32] height, [vector<i32>] pixels)") !=
        std::string::npos);

  const size_t ppmStart = imageStdlib.find("namespace ppm");
  const size_t pngStart = imageStdlib.find("namespace png");
  REQUIRE(ppmStart != std::string::npos);
  REQUIRE(pngStart != std::string::npos);
  REQUIRE(pngStart > ppmStart);
  const std::string ppmBody = imageStdlib.substr(ppmStart, pngStart - ppmStart);
  CHECK(ppmBody.find("readImpl(") != std::string::npos);
  CHECK(ppmBody.find("writeImpl(") != std::string::npos);
  CHECK(ppmBody.find("File<Read>(path)?") != std::string::npos);
  CHECK(ppmBody.find("File<Write>(path)?") != std::string::npos);
  CHECK(imageStdlib.find("if(err.code == 1i32)") != std::string::npos);
  CHECK(imageStdlib.find("if(err.code == 2i32)") != std::string::npos);
  CHECK(imageStdlib.find("return(\"image_invalid_operation\"utf8)") != std::string::npos);
  CHECK(imageStdlib.find("file.readByte(value)?") != std::string::npos);
  CHECK(imageStdlib.find("pixelCount{pixels.count()}") != std::string::npos);
  CHECK(imageStdlib.find("if(width <= 0i32 || height <= 0i32)") != std::string::npos);
  CHECK(imageStdlib.find("pixelCountWide{convert<i64>(width) * convert<i64>(height) * 3i64}") !=
        std::string::npos);
  CHECK(imageStdlib.find("if(pixelCountWide <= 0i64 || pixelCountWide > 2147483647i64)") !=
        std::string::npos);
  CHECK(imageStdlib.find("if(!imageRgbWritePixelCount(width, height, expectedPixelCount))") !=
        std::string::npos);
  CHECK(imageStdlib.find("if(pixelCount != expectedPixelCount)") != std::string::npos);
  CHECK(imageStdlib.find("while(index < pixelCount, do()") != std::string::npos);
  CHECK(imageStdlib.find("if(component < 0i32 || component > 255i32)") != std::string::npos);
  CHECK(imageStdlib.find("++index") != std::string::npos);
  CHECK(ppmBody.find("return(invalidOperation())") != std::string::npos);
  CHECK(ppmBody.find("return(Result.ok())") != std::string::npos);
  CHECK(imageStdlib.find("return(value - 48i32)") != std::string::npos);
  CHECK(imageStdlib.find("value = pendingByte") != std::string::npos);
  CHECK(imageStdlib.find("value = multiply(value, 10i32) + ppmDigitValue(byte)") != std::string::npos);
  CHECK(imageStdlib.find("pixelCount = convert<i32>(pixelCountWide)") != std::string::npos);
  CHECK(ppmBody.find("status = ppmReadAsciiInt(file, hasPending, pendingByte, parsedWidth)") != std::string::npos);
  CHECK(ppmBody.find("status = ppmWriteComponent(file, pixels[index])") != std::string::npos);
  CHECK(ppmBody.find("if(status != 0i32)") != std::string::npos);
  CHECK(ppmBody.find("pixelCount{pixels.count()}") != std::string::npos);
  CHECK(ppmBody.find("assign(") == std::string::npos);
  CHECK(ppmBody.find("plus(") == std::string::npos);
  CHECK(ppmBody.find("minus(") == std::string::npos);
  CHECK(ppmBody.find("return(unsupported_write())") == std::string::npos);
  CHECK(ppmBody.find("file.read_byte(value)?") == std::string::npos);

  const std::string pngBody = imageStdlib.substr(pngStart);
  CHECK(pngBody.find("return(unsupported_read())") != std::string::npos);
  CHECK(imageStdlib.find("pngWriteByte([File<Write>] file, [i32] value)") != std::string::npos);
  CHECK(imageStdlib.find("if(value < 0i32 || value > 255i32)") != std::string::npos);
  CHECK(pngBody.find("if(status == 1i32)") != std::string::npos);
  CHECK(pngBody.find("if(status == 2i32)") != std::string::npos);
  CHECK(pngBody.find("if(status != 0i32)") != std::string::npos);
  CHECK(pngBody.find("file.write_byte(value)?") == std::string::npos);
  CHECK(pngBody.find("readImpl(") != std::string::npos);
  CHECK(pngBody.find("writeImpl(") != std::string::npos);
  CHECK(pngBody.find("pngValidateSignature") != std::string::npos);
  CHECK(pngBody.find("pngReadU32Be") != std::string::npos);
  CHECK(pngBody.find("pngReadChunkType") != std::string::npos);
  CHECK(pngBody.find("pngReadIhdr") != std::string::npos);
  CHECK(pngBody.find("pngInflateDeflateBlocks") != std::string::npos);
  CHECK(pngBody.find("pngDecodeScanlines") != std::string::npos);
  CHECK(pngBody.find("pngWriteSignature") != std::string::npos);
  CHECK(pngBody.find("pngWriteIdatChunk") != std::string::npos);
  CHECK(pngBody.find("pngWriteSizingValid") != std::string::npos);
  CHECK(pngBody.find("File<Read>(path)?") != std::string::npos);
  CHECK(pngBody.find("File<Write>(path)?") != std::string::npos);
  CHECK(pngBody.find("pngAppendBytes") != std::string::npos);
  const size_t pngPreludeStart = imageStdlib.find("pngReadU32Be(");
  const size_t pngDecodeStart = imageStdlib.find("pngPaethPredictor");
  REQUIRE(pngPreludeStart != std::string::npos);
  REQUIRE(pngDecodeStart != std::string::npos);
  REQUIRE(pngDecodeStart > pngPreludeStart);
  const std::string pngPreludeBody = imageStdlib.substr(pngPreludeStart, pngDecodeStart - pngPreludeStart);
  CHECK(pngPreludeBody.find("pngReadU32Be") != std::string::npos);
  CHECK(pngPreludeBody.find("pngReadIhdr") != std::string::npos);
  CHECK(pngPreludeBody.find("pngChunkCrc") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteChunkOpen") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteSizingValid") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteIdatChunk") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteIendChunk") != std::string::npos);
  CHECK(pngPreludeBody.find("value = multiply(value, 256i32) + byte") != std::string::npos);
  CHECK(pngPreludeBody.find("remaining = remaining - 1i32") != std::string::npos);
  CHECK(pngPreludeBody.find("width = parsedWidth") != std::string::npos);
  CHECK(pngPreludeBody.find("return(value - multiply(divide(value, divisor), divisor))") != std::string::npos);
  CHECK(pngPreludeBody.find("crc = pngCrc32UpdateByte(crc, typeA)") != std::string::npos);
  CHECK(pngPreludeBody.find("return(divide(rawByteCount + 65534i32, 65535i32))") != std::string::npos);
  CHECK(pngPreludeBody.find("rawByteCount = convert<i32>(rawByteCountWide)") != std::string::npos);
  CHECK(pngPreludeBody.find("a = pngMod(a + byte, 65521i32)") != std::string::npos);
  CHECK(pngPreludeBody.find("pixelOffset = pixelOffset + 3i32") != std::string::npos);
  CHECK(pngPreludeBody.find("assign(") == std::string::npos);
  CHECK(pngPreludeBody.find("plus(") == std::string::npos);
  CHECK(pngPreludeBody.find("minus(") == std::string::npos);
  const size_t pngInflateStart = imageStdlib.find("pngZlibHeaderValid");
  REQUIRE(pngInflateStart != std::string::npos);
  REQUIRE(pngInflateStart > pngDecodeStart);
  const std::string pngScanlineBody = imageStdlib.substr(pngDecodeStart, pngInflateStart - pngDecodeStart);
  CHECK(pngScanlineBody.find("pngPaethPredictor") != std::string::npos);
  CHECK(pngScanlineBody.find("pngDecodeRows") != std::string::npos);
  CHECK(pngScanlineBody.find("predictor{left + up - upLeft}") != std::string::npos);
  CHECK(pngScanlineBody.find("return(divide(multiply(pngColorTypeSamplesPerPixel(colorType), bitDepth) + 7i32, 8i32))") != std::string::npos);
  CHECK(pngScanlineBody.find("scanlineBytes = 0i32") != std::string::npos);
  CHECK(pngScanlineBody.find("reconstructed = pngMod(reconstructed + leftByte, 256i32)") != std::string::npos);
  CHECK(pngScanlineBody.find("paletteBytes[paletteOffset + 1i32]") != std::string::npos);
  CHECK(pngScanlineBody.find("pixelByteIndex = pixelByteIndex + bytesPerPixel") != std::string::npos);
  CHECK(pngScanlineBody.find("offset = offset + scanlineBytes") != std::string::npos);
  CHECK(pngScanlineBody.find("assign(") == std::string::npos);
  CHECK(pngScanlineBody.find("plus(") == std::string::npos);
  CHECK(pngScanlineBody.find("minus(") == std::string::npos);
  const size_t pngInflateExecStart = imageStdlib.find("pngInflateCopyFromOutput");
  REQUIRE(pngInflateExecStart != std::string::npos);
  REQUIRE(pngInflateExecStart > pngInflateStart);
  const std::string pngBitstreamBody = imageStdlib.substr(pngInflateStart, pngInflateExecStart - pngInflateStart);
  CHECK(pngBitstreamBody.find("pngReadBits") != std::string::npos);
  CHECK(pngBitstreamBody.find("pngReadDynamicHuffmanLengths") != std::string::npos);
  CHECK(pngBitstreamBody.find("pngLengthInfo") != std::string::npos);
  CHECK(pngBitstreamBody.find("return(equal(pngMod(multiply(cmf, 256i32) + flg, 31i32), 0i32))") != std::string::npos);
  CHECK(pngBitstreamBody.find("value = value + multiply(pngMod(shifted, 2i32), factor)") != std::string::npos);
  CHECK(pngBitstreamBody.find("codeLengthLengths[codeLengthSymbol] = lengthValue") != std::string::npos);
  CHECK(pngBitstreamBody.find("totalCodeCount{literalCount + distanceCount}") != std::string::npos);
  CHECK(pngBitstreamBody.find("repeatCount{11i32 + repeatExtra}") != std::string::npos);
  CHECK(pngBitstreamBody.find("baseOut = 3i32 + (symbol - 257i32)") != std::string::npos);
  CHECK(pngBitstreamBody.find("if(equal(symbol - currentSymbol, 1i32),") != std::string::npos);
  CHECK(pngBitstreamBody.find("assign(") == std::string::npos);
  CHECK(pngBitstreamBody.find("plus(") == std::string::npos);
  CHECK(pngBitstreamBody.find("minus(") == std::string::npos);
  const size_t pngReadStart = imageStdlib.find("pngDecodeScanlines");
  REQUIRE(pngReadStart != std::string::npos);
  REQUIRE(pngReadStart > pngInflateExecStart);
  const std::string pngInflateExecBody = imageStdlib.substr(pngInflateExecStart, pngReadStart - pngInflateExecStart);
  CHECK(pngInflateExecBody.find("pngInflateStoredBlock") != std::string::npos);
  CHECK(pngInflateExecBody.find("pngInflateDynamicHuffmanBlock") != std::string::npos);
  CHECK(pngInflateExecBody.find("pngInflateDeflateBlocks") != std::string::npos);
  CHECK(pngInflateExecBody.find("output[count(output) - distance]") != std::string::npos);
  CHECK(pngInflateExecBody.find("trailerStart{count(compressed) - 4i32}") != std::string::npos);
  CHECK(pngInflateExecBody.find("byteIndex = byteIndex + 4i32") != std::string::npos);
  CHECK(pngInflateExecBody.find("matchLength{lengthBase + lengthExtraValue}") != std::string::npos);
  CHECK(pngInflateExecBody.find("code = code + multiply(bit, factor)") != std::string::npos);
  CHECK(pngInflateExecBody.find("symbol = 280i32") != std::string::npos);
  CHECK(pngInflateExecBody.find("hclenBits + 4i32") != std::string::npos);
  CHECK(pngInflateExecBody.find("inflateStatus = pngInflateStoredBlock(compressed, byteIndex, bitIndex, output)") != std::string::npos);
  CHECK(pngInflateExecBody.find("if(not_equal(compressedCount - byteIndex, 4i32),") != std::string::npos);
  CHECK(pngInflateExecBody.find("assign(") == std::string::npos);
  CHECK(pngInflateExecBody.find("plus(") == std::string::npos);
  CHECK(pngInflateExecBody.find("minus(") == std::string::npos);
  const size_t pngWriteStart = imageStdlib.rfind(
      "[effects(file_write), return<int> on_error<FileError, /std/image/ignoreFileError>]\n    writeImpl");
  REQUIRE(pngWriteStart != std::string::npos);
  REQUIRE(pngWriteStart > pngReadStart);
  const std::string pngReadBody = imageStdlib.substr(pngReadStart, pngWriteStart - pngReadStart);
  CHECK(pngReadBody.find("pixels[targetOffset] = passPixels[sourceOffset]") != std::string::npos);
  CHECK(pngReadBody.find("pixels[targetOffset + 1i32] = passPixels[sourceOffset + 1i32]") != std::string::npos);
  CHECK(pngReadBody.find("sawIhdr = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("sawPlte = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("sawPostIdatChunk = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("sawIdat = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("width = parsedWidth") != std::string::npos);
  CHECK(pngReadBody.find("height = parsedHeight") != std::string::npos);
  CHECK(pngReadBody.find("assign(") == std::string::npos);
  CHECK(pngReadBody.find("plus(") == std::string::npos);
  CHECK(pngReadBody.find("minus(") == std::string::npos);
  CHECK(pngBody.find("return(invalidOperation())") != std::string::npos);
  CHECK(pngBody.find("return(unsupported_write())") == std::string::npos);
}

TEST_CASE("file readByte docs and helpers stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path preludePath = std::filesystem::path("..") / "src" / "emitter" / "EmitterEmitPrelude.h";
  std::filesystem::path resultCallsPath =
      std::filesystem::path("..") / "src" / "emitter" / "EmitterExprResultCalls.h";
  std::filesystem::path fileAccessCallsPath =
      std::filesystem::path("..") / "src" / "emitter" / "EmitterExprFileAccessCalls.h";
  std::filesystem::path lowererPath = std::filesystem::path("..") / "src" / "ir_lowerer" / "IrLowererFileWriteHelpers.cpp";
  std::filesystem::path fileStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "file" / "file.prime";
  std::filesystem::path fileErrorsPath = std::filesystem::path("..") / "stdlib" / "std" / "file" / "errors.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(preludePath)) {
    preludePath = std::filesystem::current_path() / "src" / "emitter" / "EmitterEmitPrelude.h";
  }
  if (!std::filesystem::exists(resultCallsPath)) {
    resultCallsPath = std::filesystem::current_path() / "src" / "emitter" / "EmitterExprResultCalls.h";
  }
  if (!std::filesystem::exists(fileAccessCallsPath)) {
    fileAccessCallsPath =
        std::filesystem::current_path() / "src" / "emitter" / "EmitterExprFileAccessCalls.h";
  }
  if (!std::filesystem::exists(lowererPath)) {
    lowererPath = std::filesystem::current_path() / "src" / "ir_lowerer" / "IrLowererFileWriteHelpers.cpp";
  }
  if (!std::filesystem::exists(fileStdlibPath)) {
    fileStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "file" / "file.prime";
  }
  if (!std::filesystem::exists(fileErrorsPath)) {
    fileErrorsPath = std::filesystem::current_path() / "stdlib" / "std" / "file" / "errors.prime";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(preludePath));
  REQUIRE(std::filesystem::exists(resultCallsPath));
  REQUIRE(std::filesystem::exists(fileAccessCallsPath));
  REQUIRE(std::filesystem::exists(lowererPath));
  REQUIRE(std::filesystem::exists(fileStdlibPath));
  REQUIRE(std::filesystem::exists(fileErrorsPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string prelude = readFile(preludePath.string());
  const std::string resultCalls = readFile(resultCallsPath.string());
  const std::string fileAccessCalls = readFile(fileAccessCallsPath.string());
  const std::string lowerer = readFile(lowererPath.string());
  const std::string fileStdlib = readFile(fileStdlibPath.string());
  const std::string fileErrors = readFile(fileErrorsPath.string());

  CHECK(primeStructDoc.find("`readByte([i32 mut] value)`") != std::string::npos);
  CHECK(primeStructDoc.find("`readByte(...)` reports deterministic end-of-file as `EOF`") != std::string::npos);
  CHECK(primeStructDoc.find("`FileError.isEof(err)`") != std::string::npos);
  CHECK(primeStructDoc.find("`/File/openRead(...)`, `/File/openWrite(...)`, `/File/openAppend(...)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Compatibility wrappers keep the older snake_case spellings") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`read_byte([i32 mut] value)`") == std::string::npos);
  CHECK(primeStructDoc.find("`FileError.is_eof(err)`") == std::string::npos);
  CHECK(primeStructDoc.find("read-only file operations require `effects(file_read)`") != std::string::npos);

  CHECK(fileStdlib.find("/File/openRead([string] path)") != std::string::npos);
  CHECK(fileStdlib.find("/File/open_read([string] path)") != std::string::npos);
  CHECK(fileStdlib.find("return(/File/openRead(path))") != std::string::npos);
  CHECK(fileStdlib.find("/File/readByte<Mode, T>([File<Mode>] self, [T mut] value)") != std::string::npos);
  CHECK(fileStdlib.find("/File/read_byte<Mode, T>([File<Mode>] self, [T mut] value)") != std::string::npos);
  CHECK(fileStdlib.find("return(/File/readByte(self, value))") != std::string::npos);
  CHECK(fileStdlib.find("/File/writeLine<Mode>([File<Mode>] self)") != std::string::npos);
  CHECK(fileStdlib.find("/File/write_line<Mode>([File<Mode>] self)") != std::string::npos);
  CHECK(fileStdlib.find("return(/File/writeLine(self))") != std::string::npos);
  CHECK(fileStdlib.find("/File/writeByte<Mode, T>([File<Mode>] self, [T] value)") != std::string::npos);
  CHECK(fileStdlib.find("return(/File/writeByte(self, value))") != std::string::npos);
  CHECK(fileStdlib.find("/File/writeBytes<Mode, T>([File<Mode>] self, [array<T>] bytes)") != std::string::npos);
  CHECK(fileStdlib.find("return(/File/writeBytes(self, bytes))") != std::string::npos);

  CHECK(fileErrors.find("isEof([FileError] err)") != std::string::npos);
  CHECK(fileErrors.find("return(/std/file/FileError/isEof(err))") != std::string::npos);

  CHECK(prelude.find("static inline uint32_t ps_file_read_byte") != std::string::npos);
  CHECK(prelude.find("return \" << FileReadEofCode << \"u;") != std::string::npos);
  CHECK(prelude.find("std::string_view(\\\"EOF\\\")") != std::string::npos);
  CHECK(prelude.find("struct ps_result_status") != std::string::npos);
  CHECK(prelude.find("static constexpr uint32_t ps_result_ok_tag = 0u;") !=
        std::string::npos);
  CHECK(prelude.find("static constexpr uint32_t ps_result_error_tag = 1u;") !=
        std::string::npos);
  CHECK(prelude.find("static inline ps_result_status ps_result_status_ok()") !=
        std::string::npos);
  CHECK(prelude.find("static inline ps_result_status ps_result_status_error(uint32_t err)") !=
        std::string::npos);
  CHECK(prelude.find("static inline ps_result_status ps_result_status_from_error(uint32_t err)") !=
        std::string::npos);
  CHECK(prelude.find("static inline bool ps_result_status_is_error(ps_result_status result)") !=
        std::string::npos);
  CHECK(prelude.find("static inline uint32_t ps_result_status_error_payload(ps_result_status result)") !=
        std::string::npos);
  CHECK(prelude.find("static inline ps_result_status ps_try_status(ps_result_status result") !=
        std::string::npos);
  CHECK(prelude.find("static inline uint32_t ps_try_status") == std::string::npos);
  CHECK(prelude.find("struct ps_result_value") != std::string::npos);
  CHECK(prelude.find("uint32_t tag = 0;") != std::string::npos);
  CHECK(prelude.find("uint32_t error = 0;") != std::string::npos);
  CHECK(prelude.find("uint32_t ok = 0;") != std::string::npos);
  CHECK(prelude.find("uint32_t payload = 0;") == std::string::npos);
  CHECK(prelude.find("result.tag == ps_result_error_tag") !=
        std::string::npos);
  CHECK(prelude.find("result.tag != 0u") == std::string::npos);
  CHECK(prelude.find("return result.ok;") != std::string::npos);
  CHECK(prelude.find("return result.payload;") == std::string::npos);
  CHECK(prelude.find("operator uint64_t()") == std::string::npos);
  CHECK(prelude.find("ps_legacy_result_value(uint64_t raw)") == std::string::npos);
  CHECK(prelude.find("using ps_legacy_result_value = uint64_t;") == std::string::npos);
  CHECK(prelude.find("ps_result_value_ok") != std::string::npos);
  CHECK(prelude.find("ps_result_value_error") != std::string::npos);
  CHECK(prelude.find("ps_result_value_is_error") != std::string::npos);
  CHECK(prelude.find("ps_result_value_error_payload") != std::string::npos);
  CHECK(prelude.find("ps_result_value_ok_payload") != std::string::npos);
  CHECK(prelude.find("ps_result_is_error") == std::string::npos);
  CHECK(prelude.find("ps_result_error_payload") == std::string::npos);
  CHECK(prelude.find("ps_result_payload") == std::string::npos);
  CHECK(prelude.find("ps_result_pack") == std::string::npos);
  CHECK(prelude.find("static inline uint64_t ps_legacy_result_pack") == std::string::npos);
  CHECK(prelude.find("static inline uint64_t ps_file_open_read") == std::string::npos);
  CHECK(prelude.find("ps_legacy_result_pack") == std::string::npos);
  CHECK(prelude.find("ps_legacy_result_error") == std::string::npos);
  CHECK(prelude.find("ps_legacy_result_payload") == std::string::npos);
  CHECK(resultCalls.find("static_cast<\" << sourceResultValueCppType << \">(ps_next)") ==
        std::string::npos);
  CHECK(resultCalls.find("static_cast<\" + sourceResultValueCppType + \">(0)") ==
        std::string::npos);
  CHECK(resultCalls.find("sourceResultValueOkExpr(") != std::string::npos);
  CHECK(resultCalls.find("sourceResultValueErrorExpr(") != std::string::npos);
  CHECK(resultCalls.find("sourceResultValueIsErrorExpr(") != std::string::npos);
  CHECK(resultCalls.find("sourceResultValueErrorPayloadExpr(") != std::string::npos);
  CHECK(resultCalls.find("sourceResultValueOkPayloadExpr(") != std::string::npos);
  CHECK(resultCalls.find("sourceResultIsErrorExpr(") == std::string::npos);
  CHECK(resultCalls.find("sourceResultErrorPayloadExpr(") == std::string::npos);
  CHECK(resultCalls.find("sourceResultValuePayloadExpr(") == std::string::npos);
  CHECK(resultCalls.find("sourceResultPackExpr") == std::string::npos);
  CHECK(resultCalls.find("sourceResultStatusOkExpr()") != std::string::npos);
  CHECK(resultCalls.find("sourceResultStatusIsErrorExpr(argText)") != std::string::npos);
  CHECK(resultCalls.find("sourceResultStatusErrorPayloadExpr(guardedResultName)") !=
        std::string::npos);
  CHECK(resultCalls.find("sourceResultStatusCppType") == std::string::npos);
  CHECK(fileAccessCalls.find("ps_result_status_from_error(ps_file_read_byte") !=
        std::string::npos);

  CHECK(lowerer.find("read_byte requires exactly one argument") != std::string::npos);
  CHECK(lowerer.find("read_byte requires mutable integer binding") != std::string::npos);
  CHECK(lowerer.find("emitInstruction(IrOpcode::FileReadByte") != std::string::npos);
}

TEST_CASE("container error docs and helpers stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path collectionErrorsPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "errors.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(collectionErrorsPath)) {
    collectionErrorsPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "errors.prime";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(collectionErrorsPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string collectionErrors = readFile(collectionErrorsPath.string());

  CHECK(primeStructDoc.find("`ContainerError.missingKey()`, `ContainerError.indexOutOfBounds()`,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ContainerError.empty()`, and `ContainerError.capacityExceeded()`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("the public camelCase root wrappers `/ContainerError/missingKey()`,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Compatibility wrappers keep the older snake_case root spellings") !=
        std::string::npos);
  CHECK(primeStructDoc.find("wrapper for container error strings, `/ContainerError/missing_key()`,") ==
        std::string::npos);

  CHECK(collectionErrors.find("/ContainerError/missingKey()") != std::string::npos);
  CHECK(collectionErrors.find("/ContainerError/indexOutOfBounds()") != std::string::npos);
  CHECK(collectionErrors.find("/ContainerError/capacityExceeded()") != std::string::npos);
  CHECK(collectionErrors.find("/ContainerError/missing_key() {\n  return(/ContainerError/missingKey())\n}") !=
        std::string::npos);
  CHECK(collectionErrors.find(
            "/ContainerError/index_out_of_bounds() {\n  return(/ContainerError/indexOutOfBounds())\n}") !=
        std::string::npos);
  CHECK(collectionErrors.find(
            "/ContainerError/capacity_exceeded() {\n  return(/ContainerError/capacityExceeded())\n}") !=
        std::string::npos);
  CHECK(collectionErrors.find("missingKey() {\n      return(ContainerError{1i32})\n    }") !=
        std::string::npos);
  CHECK(collectionErrors.find("missing_key() {\n      return(missingKey())\n    }") !=
        std::string::npos);
  CHECK(collectionErrors.find("indexOutOfBounds() {\n      return(ContainerError{2i32})\n    }") !=
        std::string::npos);
  CHECK(collectionErrors.find("index_out_of_bounds() {\n      return(indexOutOfBounds())\n    }") !=
        std::string::npos);
  CHECK(collectionErrors.find("capacityExceeded() {\n      return(ContainerError{4i32})\n    }") !=
        std::string::npos);
  CHECK(collectionErrors.find("capacity_exceeded() {\n      return(capacityExceeded())\n    }") !=
        std::string::npos);
  CHECK(collectionErrors.find("containerMissingKey() {\n    return(/std/collections/ContainerError/missingKey())\n  }") !=
        std::string::npos);
  CHECK(collectionErrors.find(
            "containerIndexOutOfBounds() {\n    return(/std/collections/ContainerError/indexOutOfBounds())\n  }") !=
        std::string::npos);
  CHECK(collectionErrors.find(
            "containerCapacityExceeded() {\n    return(/std/collections/ContainerError/capacityExceeded())\n  }") !=
        std::string::npos);
}

TEST_CASE("maybe stdlib control flow stays source locked to surface if syntax") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path maybeStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "maybe" / "maybe.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(maybeStdlibPath)) {
    maybeStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "maybe" / "maybe.prime";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(maybeStdlibPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string maybeStdlib = readFile(maybeStdlibPath.string());

  CHECK(primeStructDoc.find("`isEmpty()` / `isSome()`") != std::string::npos);
  CHECK(primeStructDoc.find("compatibility wrappers `is_empty()` / `is_some()`") != std::string::npos);
  CHECK(primeStructDoc.find("**Helper surface (stdlib):** `is_empty()` / `is_some()`") == std::string::npos);
  CHECK(primeStructDoc.find("`Maybe<T>` is a stdlib-owned generic sum type") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The old mutable struct helpers `set(value)`, `clear()`, and `take()`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("value.set(1i32) // error: sum-backed Maybe<T> has no mutable helper set") !=
        std::string::npos);
  CHECK(primeStructDoc.find("value.clear() // error: sum-backed Maybe<T> has no mutable helper clear") !=
        std::string::npos);
  CHECK(primeStructDoc.find("[i32] out{value.take()} // error: sum-backed Maybe<T> has no mutable helper take") !=
        std::string::npos);
  CHECK(maybeStdlib.find("[public sum]\n  Maybe<T> {\n    none\n    [T] some\n  }") !=
        std::string::npos);
  CHECK(maybeStdlib.find("[Maybe<T>] result{[some] value}") != std::string::npos);
  CHECK(maybeStdlib.find("[Maybe<T>] result{}") != std::string::npos);
  CHECK(maybeStdlib.find("return(result)") != std::string::npos);
  CHECK(maybeStdlib.find("/Maybe/isEmpty<T>([Maybe<T>] self)") != std::string::npos);
  CHECK(maybeStdlib.find("/Maybe/isSome<T>([Maybe<T>] self)") != std::string::npos);
  CHECK(maybeStdlib.find("/Maybe/is_empty<T>([Maybe<T>] self)") != std::string::npos);
  CHECK(maybeStdlib.find("/Maybe/is_some<T>([Maybe<T>] self)") != std::string::npos);
  CHECK(maybeStdlib.find("pick(self) {\n      none {\n        return(true)\n      }") !=
        std::string::npos);
  CHECK(maybeStdlib.find("pick(self) {\n      none {\n        return(false)\n      }") !=
        std::string::npos);
  CHECK(maybeStdlib.find("[public struct]") == std::string::npos);
  CHECK(maybeStdlib.find("uninitialized<T>") == std::string::npos);
  CHECK(maybeStdlib.find("drop(this.value)") == std::string::npos);
  CHECK(maybeStdlib.find("assign(this.empty, true)") == std::string::npos);
  CHECK(maybeStdlib.find("assign(this.empty, false)") == std::string::npos);
  CHECK(maybeStdlib.find("assign(ref.empty, false)") == std::string::npos);
  CHECK(maybeStdlib.find("this.empty = true") == std::string::npos);
  CHECK(maybeStdlib.find("this.empty = false") == std::string::npos);
  CHECK(maybeStdlib.find("ref.empty = false") == std::string::npos);
}

TEST_CASE("small stdlib wrappers stay source locked to inferred locals") {
  std::filesystem::path codeExamplesPath = std::filesystem::path("..") / "docs" / "CodeExamples.md";
  std::filesystem::path maybeStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "maybe" / "maybe.prime";
  std::filesystem::path vectorStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "vector.prime";
  std::filesystem::path collectionsStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "collections.prime";
  std::filesystem::path mapStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "map.prime";
  std::filesystem::path internalMapStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "internal_map.prime";
  std::filesystem::path experimentalVectorStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_vector.prime";
  std::filesystem::path internalVectorStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "internal_vector.prime";
  std::filesystem::path experimentalMapStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_map.prime";
  std::filesystem::path soaWrapperPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "soa_vector.prime";
  std::filesystem::path soaPublicPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "soa.prime";
  std::filesystem::path soaConversionsPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "soa_vector_conversions.prime";
  std::filesystem::path internalSoaVectorPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "internal_soa_vector.prime";
  std::filesystem::path internalSoaConversionsPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "internal_soa_vector_conversions.prime";
  std::filesystem::path experimentalSoaVectorPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_soa_vector.prime";
  std::filesystem::path experimentalSoaConversionsPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_soa_vector_conversions.prime";
  if (!std::filesystem::exists(codeExamplesPath)) {
    codeExamplesPath = std::filesystem::current_path() / "docs" / "CodeExamples.md";
  }
  if (!std::filesystem::exists(maybeStdlibPath)) {
    maybeStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "maybe" / "maybe.prime";
  }
  if (!std::filesystem::exists(vectorStdlibPath)) {
    vectorStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "collections" / "vector.prime";
  }
  if (!std::filesystem::exists(collectionsStdlibPath)) {
    collectionsStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "collections.prime";
  }
  if (!std::filesystem::exists(mapStdlibPath)) {
    mapStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "collections" / "map.prime";
  }
  if (!std::filesystem::exists(internalMapStdlibPath)) {
    internalMapStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "internal_map.prime";
  }
  if (!std::filesystem::exists(experimentalVectorStdlibPath)) {
    experimentalVectorStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "experimental_vector.prime";
  }
  if (!std::filesystem::exists(internalVectorStdlibPath)) {
    internalVectorStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "internal_vector.prime";
  }
  if (!std::filesystem::exists(experimentalMapStdlibPath)) {
    experimentalMapStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "experimental_map.prime";
  }
  if (!std::filesystem::exists(soaWrapperPath)) {
    soaWrapperPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "soa_vector.prime";
  }
  if (!std::filesystem::exists(soaPublicPath)) {
    soaPublicPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "soa.prime";
  }
  if (!std::filesystem::exists(soaConversionsPath)) {
    soaConversionsPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "soa_vector_conversions.prime";
  }
  if (!std::filesystem::exists(internalSoaVectorPath)) {
    internalSoaVectorPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "internal_soa_vector.prime";
  }
  if (!std::filesystem::exists(internalSoaConversionsPath)) {
    internalSoaConversionsPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" /
        "internal_soa_vector_conversions.prime";
  }
  if (!std::filesystem::exists(experimentalSoaVectorPath)) {
    experimentalSoaVectorPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "experimental_soa_vector.prime";
  }
  if (!std::filesystem::exists(experimentalSoaConversionsPath)) {
    experimentalSoaConversionsPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" /
        "experimental_soa_vector_conversions.prime";
  }
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(maybeStdlibPath));
  REQUIRE(std::filesystem::exists(vectorStdlibPath));
  REQUIRE(std::filesystem::exists(collectionsStdlibPath));
  REQUIRE(std::filesystem::exists(mapStdlibPath));
  REQUIRE(std::filesystem::exists(internalMapStdlibPath));
  REQUIRE(std::filesystem::exists(experimentalVectorStdlibPath));
  REQUIRE(std::filesystem::exists(internalVectorStdlibPath));
  REQUIRE(std::filesystem::exists(experimentalMapStdlibPath));
  REQUIRE(std::filesystem::exists(soaWrapperPath));
  REQUIRE(std::filesystem::exists(soaPublicPath));
  REQUIRE(std::filesystem::exists(soaConversionsPath));
  REQUIRE(std::filesystem::exists(internalSoaVectorPath));
  REQUIRE(std::filesystem::exists(internalSoaConversionsPath));
  REQUIRE(std::filesystem::exists(experimentalSoaVectorPath));
  REQUIRE(std::filesystem::exists(experimentalSoaConversionsPath));

  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::string maybeStdlib = readFile(maybeStdlibPath.string());
  const std::string vectorStdlib = readFile(vectorStdlibPath.string());
  const std::string collectionsStdlib = readFile(collectionsStdlibPath.string());
  const std::string mapStdlib = readFile(mapStdlibPath.string());
  const std::string internalMapStdlib = readFile(internalMapStdlibPath.string());
  const std::string experimentalVectorStdlib = readFile(experimentalVectorStdlibPath.string());
  const std::string internalVectorStdlib = readFile(internalVectorStdlibPath.string());
  const std::string experimentalMapStdlib = readFile(experimentalMapStdlibPath.string());
  const std::string soaWrapper = readFile(soaWrapperPath.string());
  const std::string soaPublic = readFile(soaPublicPath.string());
  const std::string soaConversions = readFile(soaConversionsPath.string());
  const std::string internalSoaVector = readFile(internalSoaVectorPath.string());
  const std::string internalSoaConversions = readFile(internalSoaConversionsPath.string());
  const std::string experimentalSoaVector = readFile(experimentalSoaVectorPath.string());
  const std::string experimentalSoaConversions = readFile(experimentalSoaConversionsPath.string());

  CHECK(codeExamples.find("Internal implementation, bridge, or substrate-oriented code:") !=
        std::string::npos);
  CHECK(codeExamples.find("Intentionally canonical or substrate-oriented code:") == std::string::npos);
  CHECK(codeExamples.find("[mut] current{start}") != std::string::npos);
  CHECK(codeExamples.find("limit{5}") != std::string::npos);
  CHECK(codeExamples.find("return(counter.doubled() + Counter.defaultStep())") !=
        std::string::npos);
  CHECK(codeExamples.find("return(counter.doubled() + /Counter/defaultStep())") ==
        std::string::npos);
  CHECK(codeExamples.find("preferred type-qualified dot-call form for static\nhelpers together.") !=
        std::string::npos);
  CHECK(codeExamples.find("prefer `left == right` at the call site and\nkeep `/Type/Equal(left, right)` as the helper contract underneath.") !=
        std::string::npos);
  CHECK(codeExamples.find("return(left == right)") != std::string::npos);

  CHECK(maybeStdlib.find("[Maybe<T>] result{[some] value}") != std::string::npos);
  CHECK(maybeStdlib.find("[Maybe<T>] result{}") != std::string::npos);
  CHECK(maybeStdlib.find("return(result)") != std::string::npos);
  CHECK(maybeStdlib.find("some(value) {\n        return(true)\n      }") != std::string::npos);
  CHECK(maybeStdlib.find("out{take(this.value)}") == std::string::npos);
  CHECK(maybeStdlib.find("[mut] out{Maybe<T>{}}") == std::string::npos);
  CHECK(maybeStdlib.find("[mut] ref{location(out)}") == std::string::npos);
  CHECK(maybeStdlib.find("[T] out{take(this.value)}") == std::string::npos);
  CHECK(maybeStdlib.find("[Maybe<T> mut] out{Maybe<T>{}}") == std::string::npos);
  CHECK(maybeStdlib.find("[Reference<Maybe<T>> mut] ref{location(out)}") == std::string::npos);

  CHECK(vectorStdlib.find(
            "// Canonical public wrapper layer over the internal_vector implementation module.") !=
        std::string::npos);
  CHECK(vectorStdlib.find("import /std/collections/internal_vector/*") != std::string::npos);
  CHECK(vectorStdlib.find("[mut] result{/std/collections/internal_vector/vector<T>()}") !=
        std::string::npos);
  CHECK(vectorStdlib.find("valueCount{values.count()}") != std::string::npos);
  CHECK(vectorStdlib.find("[mut] index{0i32}") != std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/vector/push<T>(result, /at(values, index))") !=
        std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/vector/push<T>(result, first)") !=
        std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/vector/push<T>(result, second)") !=
        std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/internal_vector/vectorCount<T>(values)") !=
        std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/internal_vector/vectorPush<T>(values, value)") !=
        std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/internal_vector/vectorAt<T>(values, index)") !=
        std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/experimental_vector/") == std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/experimental_vector/vectorPair<T>(first, second)") ==
        std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/experimental_vector/vectorPush<T>(out, values[index])") ==
        std::string::npos);
  CHECK(vectorStdlib.find("[mut] out{/std/collections/internal_vector/vector<T>()}") ==
        std::string::npos);
  CHECK(vectorStdlib.find("[Vector<T> mut] out{/std/collections/internal_vector/vector<T>()}") ==
        std::string::npos);
  CHECK(vectorStdlib.find("[i32] valueCount{count(values)}") == std::string::npos);
  CHECK(vectorStdlib.find("[i32 mut] index{0i32}") == std::string::npos);

  CHECK(collectionsStdlib.find("Retired compatibility umbrella.") != std::string::npos);
  CHECK(collectionsStdlib.find("Public map helpers now live under /std/collections/map/*.") !=
        std::string::npos);
  CHECK(collectionsStdlib.find("vectorCount<T>") == std::string::npos);
  CHECK(collectionsStdlib.find("vectorCapacity<T>") == std::string::npos);
  CHECK(collectionsStdlib.find("vectorPush<T>") == std::string::npos);
  CHECK(collectionsStdlib.find("vectorAt<T>") == std::string::npos);
  CHECK(collectionsStdlib.find("vectorSingle<T>") == std::string::npos);
  CHECK(collectionsStdlib.find("vectorPair<T>") == std::string::npos);
  CHECK(collectionsStdlib.find("mapCount") == std::string::npos);
  CHECK(collectionsStdlib.find("mapContains") == std::string::npos);
  CHECK(collectionsStdlib.find("mapTryAt") == std::string::npos);
  CHECK(collectionsStdlib.find("mapAt") == std::string::npos);
  CHECK(collectionsStdlib.find("mapInsert") == std::string::npos);
  CHECK(collectionsStdlib.find("[public") == std::string::npos);

  CHECK(mapStdlib.find(
            "// Standalone canonical stdlib-owned map implementation.") !=
        std::string::npos);
  CHECK(mapStdlib.find("import /std/collections/internal_vector/*") !=
        std::string::npos);
  CHECK(mapStdlib.find("import /std/collections/internal_map") == std::string::npos);
  CHECK(mapStdlib.find("import /std/collections/experimental_map") == std::string::npos);
  CHECK(mapStdlib.find("[MapValue<K, V> mut] out{mapNew<K, V>()}") !=
        std::string::npos);
  CHECK(mapStdlib.find("[i32] entryCount{count(entries)}") != std::string::npos);
  CHECK(mapStdlib.find("[i32 mut] index{0i32}") != std::string::npos);
  CHECK(mapStdlib.find("[Entry<K, V>] current{entries[index]}") !=
        std::string::npos);
  CHECK(mapStdlib.find("/std/collections/mapSingle") == std::string::npos);
  CHECK(mapStdlib.find("/std/collections/mapPair") == std::string::npos);
  CHECK(mapStdlib.find("/std/collections/map/mapCount<K, V>") !=
        std::string::npos);
  CHECK(mapStdlib.find("[MapValue<K, V> mut] values") == std::string::npos);
  CHECK(mapStdlib.find("[map<K, V> mut] out{/std/collections/internal_map/mapNew<K, V>()}") ==
        std::string::npos);
  CHECK(mapStdlib.find("/std/collections/internal_map/mapNew<K, V>()") ==
        std::string::npos);

  CHECK(experimentalVectorStdlib.find("// Rejected direct-import shim for the legacy experimental vector namespace.") !=
        std::string::npos);
  CHECK(experimentalVectorStdlib.find(
            "// Canonical wrappers route through /std/collections/internal_vector/*.") !=
        std::string::npos);
  CHECK(experimentalVectorStdlib.find("import /std/collections/internal_vector/*") !=
        std::string::npos);
  CHECK(internalVectorStdlib.find("// Internal vector backing implementation behind canonical /std/collections/vector/*.") !=
        std::string::npos);
  CHECK(internalVectorStdlib.find("namespace internal_vector") != std::string::npos);
  CHECK(internalVectorStdlib.find("[public struct]\n  Vector<T>()") != std::string::npos);
  CHECK(internalVectorStdlib.find("/std/collections/internal_vector/vectorPush<T>(values, value)") ==
        std::string::npos);
  CHECK(experimentalMapStdlib.find("// Rejected direct-import shim for the legacy experimental map namespace.") !=
        std::string::npos);
  CHECK(experimentalMapStdlib.find(
            "// Canonical wrappers route through /std/collections/internal_map/*.") !=
        std::string::npos);
  CHECK(experimentalMapStdlib.find("import /std/collections/internal_map/*") !=
        std::string::npos);
  CHECK(experimentalMapStdlib.find("[Vector<K>] keys{this.keys}") == std::string::npos);
  CHECK(internalMapStdlib.find("[Vector<K>] keys{this.keys}") != std::string::npos);
  CHECK(internalMapStdlib.find(
            "// Internal map backing implementation behind canonical /std/collections/map/*.") !=
        std::string::npos);
  CHECK(internalMapStdlib.find(
            "// Vector storage comes from the internal vector backing module.") !=
        std::string::npos);
  CHECK(internalMapStdlib.find("return(/std/collections/internal_vector/vectorCount<K>(keys))") !=
        std::string::npos);
  CHECK(experimentalMapStdlib.find("import /std/collections/experimental_vector/*") ==
        std::string::npos);
  CHECK(experimentalMapStdlib.find("return(/std/collections/map/count<K, V>(this))") == std::string::npos);

  CHECK(soaWrapper.find("// Retired compatibility module.") !=
        std::string::npos);
  CHECK(soaWrapper.find("Public SoA helpers now live under /std/collections/soa/*.") !=
        std::string::npos);
  CHECK(soaWrapper.find("import /std/collections/internal_soa_vector/*") ==
        std::string::npos);
  CHECK(soaWrapper.find("import /std/collections/experimental_soa_vector/*") ==
        std::string::npos);
  CHECK(soaWrapper.find("valueCount{/count(values)}") == std::string::npos);
  CHECK(soaWrapper.find("/std/collections/internal_soa_vector/soaVectorNew<T>()") ==
        std::string::npos);
  CHECK(soaWrapper.find("/std/collections/internal_soa_vector/soaVectorPush<T>(out, /at(values, index))") ==
        std::string::npos);
  CHECK(soaWrapper.find("/std/collections/experimental_soa_vector/soaVectorNew<T>()") ==
        std::string::npos);
  CHECK(soaWrapper.find("/std/collections/experimental_soa_vector/soaVectorPush<T>(out, values[index])") ==
        std::string::npos);
  CHECK(soaWrapper.find("/std/collections/soa_vector/soaVectorNew<T>()") == std::string::npos);
  CHECK(soaWrapper.find("/std/collections/soa_vector/soaVectorSingle<T>([T] value)") ==
        std::string::npos);
  CHECK(soaWrapper.find("/std/collections/soa_vector/soaVectorFromAos<T>([vector<T>] values)") ==
        std::string::npos);
  CHECK(soaWrapper.find("/std/collections/soa_vector/soaVectorCount<T>([SoaVector<T>] values)") ==
        std::string::npos);
  CHECK(soaWrapper.find("/std/collections/soa_vector/soaVectorFieldView<Struct, Field>(") ==
        std::string::npos);
  CHECK(soaWrapper.find("/std/collections/soa_vector/soaVectorPush<T>([SoaVector<T> mut] values, [T] value)") ==
        std::string::npos);

  CHECK(soaPublic.find(
            "// Canonical public SoA helper namespace over the current SoaVector backing.") !=
        std::string::npos);
  CHECK(soaPublic.find("import /std/collections/internal_soa_vector") !=
        std::string::npos);
  CHECK(soaPublic.find("import /std/collections/experimental_soa_vector/*") ==
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/soa<T>([args<T>] values)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/internal_soa_vector/soaVectorPush<T>(out, /at(values, index))") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/single<T>([T] value)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/from_aos<T>([vector<T>] values)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/count<T>([SoaVector<T>] values)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/get<T>([SoaVector<T>] values, [i32] index)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/ref<T>([SoaVector<T>] values, [i32] index)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/reserve<T>([SoaVector<T> mut] values, [i32] capacity)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/push<T>([SoaVector<T> mut] values, [T] value)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/to_aos<T>([SoaVector<T>] values)") !=
        std::string::npos);
  CHECK(soaPublic.find("return(/std/collections/internal_soa_vector_conversions/soaVectorToAos<T>(values))") !=
        std::string::npos);

  CHECK(internalSoaVector.find(
            "// Internal implementation adapter behind canonical /std/collections/soa/*.") !=
        std::string::npos);
  CHECK(internalSoaVector.find(
            "// targeted compatibility surface.") !=
        std::string::npos);
  CHECK(internalSoaVector.find("namespace internal_soa_vector") !=
        std::string::npos);
  CHECK(internalSoaVector.find("import /std/collections/experimental_soa_vector/*") !=
        std::string::npos);
  CHECK(internalSoaVector.find("[SoaColumn<T>] storage{values.storage}\n"
                               "    return(/std/collections/internal_soa_storage/soaColumnCount<T>(storage))") !=
        std::string::npos);
  CHECK(internalSoaVector.find("[SoaColumn<T> mut] storage{values.storage}\n"
                               "    /std/collections/internal_soa_storage/soaColumnPush<T>(storage, value)") !=
        std::string::npos);

  CHECK(soaConversions.find("// Retired compatibility conversion module.") !=
        std::string::npos);
  CHECK(soaConversions.find("Public SoA conversions now live under /std/collections/soa/*.") !=
        std::string::npos);
  CHECK(soaConversions.find("import /std/collections/soa_vector/*") ==
        std::string::npos);
  CHECK(soaConversions.find("import /std/collections/internal_soa_vector_conversions/*") ==
        std::string::npos);
  CHECK(soaConversions.find("import /std/collections/experimental_soa_vector/*") ==
        std::string::npos);
  CHECK(soaConversions.find("/std/collections/soa_vector_conversions/soaVectorToAos<T>(") ==
        std::string::npos);
  CHECK(soaConversions.find("/std/collections/soa_vector_conversions/soaVectorToAosRef<T>(") ==
        std::string::npos);
  CHECK(soaConversions.find("    [SoaVector<T>] values) {") ==
        std::string::npos);
  CHECK(soaConversions.find("    [Reference<SoaVector<T>>] values) {") ==
        std::string::npos);
  CHECK(soaConversions.find(
            "return(/std/collections/internal_soa_vector_conversions/soaVectorToAos<T>(values))") ==
        std::string::npos);
  CHECK(soaConversions.find(
            "return(/std/collections/internal_soa_vector_conversions/soaVectorToAosRef<T>(values))") ==
        std::string::npos);
  CHECK(soaConversions.find("valueCount{/std/collections/soa_vector/count<T>(values)}") ==
        std::string::npos);
  CHECK(soaConversions.find("valueCount{/std/collections/soa_vector/count_ref<T>(values)}") ==
        std::string::npos);
  CHECK(soaConversions.find("[vector<T> mut] out{vector<T>()}") == std::string::npos);
  CHECK(soaConversions.find("[mut] index{0i32}") == std::string::npos);
  CHECK(soaConversions.find("[i32] valueCount{/std/collections/soa_vector/count<T>(values)}") ==
        std::string::npos);
  CHECK(soaConversions.find("[i32] valueCount{/std/collections/soa_vector/count_ref<T>(values)}") ==
        std::string::npos);
  CHECK(soaConversions.find("[mut] out{vector<T>()}") == std::string::npos);
  CHECK(soaConversions.find("[i32 mut] index{0i32}") == std::string::npos);
  CHECK(soaConversions.find("/std/collections/experimental_soa_vector/SoaVector<T>") ==
        std::string::npos);
  CHECK(internalSoaConversions.find(
            "// Internal conversion adapter behind canonical /std/collections/soa/*.") !=
        std::string::npos);
  CHECK(internalSoaConversions.find("namespace internal_soa_vector_conversions") !=
        std::string::npos);
  CHECK(internalSoaConversions.find("import /std/collections/internal_soa_vector/*") !=
        std::string::npos);
  CHECK(internalSoaConversions.find("import /std/collections/soa/*") !=
        std::string::npos);
  CHECK(internalSoaConversions.find("import /std/collections/soa_vector/*") ==
        std::string::npos);
  CHECK(internalSoaConversions.find("import /std/collections/experimental_soa_vector/*") ==
        std::string::npos);
  CHECK(internalSoaConversions.find("soaVectorToAos<T>([SoaVector<T>] values)") !=
        std::string::npos);
  CHECK(internalSoaConversions.find("soaVectorToAosRef<T>([Reference<SoaVector<T>>] values)") !=
        std::string::npos);
  CHECK(internalSoaConversions.find("valueCount{/std/collections/soa/count<T>(values)}") !=
        std::string::npos);
  CHECK(internalSoaConversions.find("/std/collections/soa/get<T>(values, index)") !=
        std::string::npos);
  CHECK(internalSoaConversions.find("valueCount{/std/collections/soa/count_ref<T>(values)}") !=
        std::string::npos);
  CHECK(internalSoaConversions.find("/std/collections/soa/get_ref<T>(values, index)") !=
        std::string::npos);
  CHECK(internalSoaConversions.find("/std/collections/soa_vector/count<T>(values)") ==
        std::string::npos);
  CHECK(internalSoaConversions.find("/std/collections/soa_vector/get<T>(values, index)") ==
        std::string::npos);
  CHECK(internalSoaConversions.find("/std/collections/soa_vector/count_ref<T>(values)") ==
        std::string::npos);
  CHECK(internalSoaConversions.find("/std/collections/soa_vector/get_ref<T>(values, index)") ==
        std::string::npos);
  CHECK(internalSoaConversions.find("[vector<T> mut] out{vector<T>()}") != std::string::npos);
  CHECK(internalSoaConversions.find("[mut] index{0i32}") != std::string::npos);
  CHECK(experimentalSoaVector.find(
            "// Rejected direct-import shim behind canonical /std/collections/soa/*.") !=
        std::string::npos);
  CHECK(experimentalSoaVector.find(
            "// Internal adapters may import this file while public source imports reject.") !=
        std::string::npos);
  CHECK(experimentalSoaVector.find(
            "// Canonical wrappers route through /std/collections/internal_soa_vector/*.") !=
        std::string::npos);
  CHECK(experimentalSoaVector.find(
            "// Ordinary public examples should import /std/collections/soa/* instead.") !=
        std::string::npos);
  CHECK(experimentalSoaConversions.find(
            "// Rejected direct-import conversion shim for legacy experimental SoA paths.") !=
        std::string::npos);
  CHECK(experimentalSoaConversions.find(
            "// Internal adapters may import this file while public source imports reject.") !=
        std::string::npos);
  CHECK(experimentalSoaConversions.find(
            "// Canonical conversions route through /std/collections/internal_soa_vector_conversions/*.") !=
        std::string::npos);
}

TEST_CASE("ppm image workflows keep explicit read locals") {
  std::filesystem::path imageStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "image" / "image.prime";
  if (!std::filesystem::exists(imageStdlibPath)) {
    imageStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "image" / "image.prime";
  }
  REQUIRE(std::filesystem::exists(imageStdlibPath));

  const std::string imageStdlib = readFile(imageStdlibPath.string());
  const size_t helperStart = imageStdlib.find("ppmSkipComment(");
  const size_t pngStart = imageStdlib.find("namespace png");
  REQUIRE(helperStart != std::string::npos);
  REQUIRE(pngStart != std::string::npos);
  REQUIRE(pngStart > helperStart);
  const std::string ppmHelpersBody = imageStdlib.substr(helperStart, pngStart - helperStart);

  CHECK(ppmHelpersBody.find("[mut] byte{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("status{ppmReadByteStatus(file, byte)}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] started{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("commentStatus{ppmSkipComment(file)}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] sawWhitespace{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("pixelCountWide{convert<i64>(width) * convert<i64>(height) * 3i64}") !=
        std::string::npos);
  CHECK(ppmHelpersBody.find("pixelCount{pixels.count()}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] expectedPixelCount{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("component{pixels[index]}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[File<Read>] file{File<Read>(path)?}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] formatByte{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] status{ppmNextByte(file, hasPending, pendingByte, byte)}") !=
        std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] parsedWidth{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] pixelCount{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("status{readImpl(width, height, pixels, path)}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[File<Write>] file{File<Write>(path)?}") != std::string::npos);

  CHECK(ppmHelpersBody.find("[i32] status{ppmReadByteStatus(file, byte)}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] started{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32] commentStatus{ppmSkipComment(file)}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] sawWhitespace{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i64] pixelCountWide{multiply(multiply(convert<i64>(width), convert<i64>(height)), 3i64)}") ==
        std::string::npos);
  CHECK(ppmHelpersBody.find("[i32] pixelCount{count(pixels)}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] expectedPixelCount{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32] component{pixels[index]}") == std::string::npos);
  CHECK(ppmHelpersBody.find("      file{File<Read>(path)?}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] formatByte{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] status{ppmNextByte(file, hasPending, pendingByte, byte)}") ==
        std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] parsedWidth{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] pixelCount{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32] status{readImpl(width, height, pixels, path)}") == std::string::npos);
  CHECK(ppmHelpersBody.find("      file{File<Write>(path)?}") == std::string::npos);
}

TEST_CASE("png prelude image workflows keep explicit read locals") {
  std::filesystem::path imageStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "image" / "image.prime";
  if (!std::filesystem::exists(imageStdlibPath)) {
    imageStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "image" / "image.prime";
  }
  REQUIRE(std::filesystem::exists(imageStdlibPath));

  const std::string imageStdlib = readFile(imageStdlibPath.string());
  const size_t pngStart = imageStdlib.find("namespace png");
  const size_t pngPreludeStart = imageStdlib.find("pngReadU32Be(");
  const size_t pngDecodeStart = imageStdlib.find("pngPaethPredictor");
  REQUIRE(pngStart != std::string::npos);
  REQUIRE(pngPreludeStart != std::string::npos);
  REQUIRE(pngDecodeStart != std::string::npos);
  REQUIRE(pngDecodeStart > pngPreludeStart);
  const std::string pngPreludeBody = imageStdlib.substr(pngPreludeStart, pngDecodeStart - pngPreludeStart);

  CHECK(pngPreludeBody.find("[mut] index{0i32}") != std::string::npos);
  CHECK(pngPreludeBody.find("status{ppmReadByteStatus(file, byte)}") != std::string::npos);
  CHECK(pngPreludeBody.find("statusA{ppmReadByteStatus(file, a)}") != std::string::npos);
  CHECK(pngPreludeBody.find("[mut] byte{0i32}") != std::string::npos);
  CHECK(pngPreludeBody.find("[mut] parsedWidth{0i32}") != std::string::npos);
  CHECK(pngPreludeBody.find("widthStatus{pngReadU32Be(file, parsedWidth)}") != std::string::npos);
  CHECK(pngPreludeBody.find("[mut] currentLeft{left}") != std::string::npos);
  CHECK(pngPreludeBody.find("leftBit{pngModU64(currentLeft, 2u64)}") != std::string::npos);
  CHECK(pngPreludeBody.find("[mut] crc{4294967295u64}") != std::string::npos);
  CHECK(pngPreludeBody.find("byteA{divide(value, 16777216i32)}") != std::string::npos);
  CHECK(pngPreludeBody.find("lengthStatus{pngWriteU32Be(file, length)}") != std::string::npos);
  CHECK(pngPreludeBody.find("[mut] pixelCount{0i32}") != std::string::npos);
  CHECK(pngPreludeBody.find("rawByteCountWide{convert<i64>(pixelCount) + convert<i64>(height)}") != std::string::npos);
  CHECK(pngPreludeBody.find("status0{pngWriteByte(file, 137i32)}") != std::string::npos);
  CHECK(pngPreludeBody.find("openStatus{pngWriteChunkOpen(file, 13i32, 73i32, 72i32, 68i32, 82i32, crc)}") !=
        std::string::npos);
  CHECK(pngPreludeBody.find("lenLow{pngMod(blockByteCount, 256i32)}") != std::string::npos);
  CHECK(pngPreludeBody.find("[mut] nextBlockCount{65535i32}") != std::string::npos);
  CHECK(pngPreludeBody.find("isFinalBlock{if(equal(nextBlockCount, rawRemaining), then() { 1i32 }, else() { 0i32 })}") !=
        std::string::npos);
  CHECK(pngPreludeBody.find(
            "filterStatus{pngWriteStoredDataByte(file, 0i32, rawRemaining, blockRemaining, crc, adlerA, adlerB)}") !=
        std::string::npos);
  CHECK(pngPreludeBody.find(
            "redStatus{pngWriteStoredDataByte(file, pixels[pixelOffset], rawRemaining, blockRemaining, crc, adlerA, "
            "adlerB)}") != std::string::npos);
  CHECK(pngPreludeBody.find("adlerStatus0{pngWriteChunkPayloadByte(file, divide(adlerB, 256i32), crc)}") !=
        std::string::npos);
  CHECK(pngPreludeBody.find("openStatus{pngWriteChunkOpen(file, 0i32, 73i32, 69i32, 78i32, 68i32, crc)}") !=
        std::string::npos);

  CHECK(pngPreludeBody.find("[i32 mut] index{0i32}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32] status{ppmReadByteStatus(file, byte)}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32] statusA{ppmReadByteStatus(file, a)}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32 mut] byte{0i32}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32 mut] parsedWidth{0i32}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32] widthStatus{pngReadU32Be(file, parsedWidth)}") == std::string::npos);
  CHECK(pngPreludeBody.find("[u64 mut] currentLeft{left}") == std::string::npos);
  CHECK(pngPreludeBody.find("[u64] leftBit{pngModU64(currentLeft, 2u64)}") == std::string::npos);
  CHECK(pngPreludeBody.find("[u64 mut] crc{4294967295u64}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32] byteA{divide(value, 16777216i32)}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32] lengthStatus{pngWriteU32Be(file, length)}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32 mut] pixelCount{0i32}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i64] rawByteCountWide{convert<i64>(pixelCount) + convert<i64>(height)}") ==
        std::string::npos);
  CHECK(pngPreludeBody.find("[i32] status0{pngWriteByte(file, 137i32)}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32] openStatus{pngWriteChunkOpen(file, 13i32, 73i32, 72i32, 68i32, 82i32, crc)}") ==
        std::string::npos);
  CHECK(pngPreludeBody.find("[i32] lenLow{pngMod(blockByteCount, 256i32)}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32 mut] nextBlockCount{65535i32}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32] isFinalBlock{if(equal(nextBlockCount, rawRemaining), then() { 1i32 }, else() { "
                            "0i32 })}") == std::string::npos);
  CHECK(pngPreludeBody.find(
            "[i32] filterStatus{pngWriteStoredDataByte(file, 0i32, rawRemaining, blockRemaining, crc, adlerA, "
            "adlerB)}") == std::string::npos);
  CHECK(pngPreludeBody.find(
            "[i32] redStatus{pngWriteStoredDataByte(file, pixels[pixelOffset], rawRemaining, blockRemaining, crc, "
            "adlerA, adlerB)}") == std::string::npos);
  CHECK(pngPreludeBody.find("[i32] adlerStatus0{pngWriteChunkPayloadByte(file, divide(adlerB, 256i32), crc)}") ==
        std::string::npos);
  CHECK(pngPreludeBody.find("[i32] openStatus{pngWriteChunkOpen(file, 0i32, 73i32, 69i32, 78i32, 68i32, crc)}") ==
        std::string::npos);
}

TEST_CASE("png scanline bitstream and inflate helpers stay source locked to inferred locals") {
  std::filesystem::path imageStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "image" / "image.prime";
  if (!std::filesystem::exists(imageStdlibPath)) {
    imageStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "image" / "image.prime";
  }
  REQUIRE(std::filesystem::exists(imageStdlibPath));

  const std::string imageStdlib = readFile(imageStdlibPath.string());
  const size_t pngDecodeStart = imageStdlib.find("pngPaethPredictor");
  const size_t pngInflateStart = imageStdlib.find("pngZlibHeaderValid");
  const size_t pngInflateExecStart = imageStdlib.find("pngInflateCopyFromOutput");
  const size_t pngReadStart = imageStdlib.find("pngDecodeScanlines");
  REQUIRE(pngDecodeStart != std::string::npos);
  REQUIRE(pngInflateStart != std::string::npos);
  REQUIRE(pngInflateExecStart != std::string::npos);
  REQUIRE(pngReadStart != std::string::npos);
  REQUIRE(pngInflateStart > pngDecodeStart);
  REQUIRE(pngInflateExecStart > pngInflateStart);
  REQUIRE(pngReadStart > pngInflateExecStart);

  const std::string pngScanlineBody = imageStdlib.substr(pngDecodeStart, pngInflateStart - pngDecodeStart);
  const std::string pngBitstreamBody = imageStdlib.substr(pngInflateStart, pngInflateExecStart - pngInflateStart);
  const std::string pngInflateExecBody = imageStdlib.substr(pngInflateExecStart, pngReadStart - pngInflateExecStart);

  CHECK(pngScanlineBody.find("predictor{left + up - upLeft}") != std::string::npos);
  CHECK(pngScanlineBody.find("[mut] valueCount{1i32}") != std::string::npos);
  CHECK(pngScanlineBody.find("[mut] rowBytes{vector<i32>()}") != std::string::npos);
  CHECK(pngScanlineBody.find("sample{pngPackedSampleAt(rowBytes, pixelIndex, bitDepth)}") != std::string::npos);
  CHECK(pngScanlineBody.find("paletteOffset{multiply(sample, 3i32)}") != std::string::npos);
  CHECK(pngScanlineBody.find("[mut] pixelByteIndex{0i32}") != std::string::npos);

  CHECK(pngScanlineBody.find("[i32] predictor{left + up - upLeft}") == std::string::npos);
  CHECK(pngScanlineBody.find("[i32 mut] valueCount{1i32}") == std::string::npos);
  CHECK(pngScanlineBody.find("[vector<i32> mut] rowBytes{vector<i32>()}") == std::string::npos);
  CHECK(pngScanlineBody.find("[i32] sample{pngPackedSampleAt(rowBytes, pixelIndex, bitDepth)}") ==
        std::string::npos);
  CHECK(pngScanlineBody.find("[i32] paletteOffset{multiply(sample, 3i32)}") == std::string::npos);
  CHECK(pngScanlineBody.find("[i32 mut] pixelByteIndex{0i32}") == std::string::npos);

  CHECK(pngBitstreamBody.find("[mut] remaining{count}") != std::string::npos);
  CHECK(pngBitstreamBody.find("status{ppmReadByteStatus(file, byte)}") != std::string::npos);
  CHECK(pngBitstreamBody.find("len{count(bytes)}") != std::string::npos);
  CHECK(pngBitstreamBody.find("[mut] factor{1i32}") != std::string::npos);
  CHECK(pngBitstreamBody.find("[mut] codeLengthLengths{vector<i32>()}") != std::string::npos);
  CHECK(pngBitstreamBody.find("codeLengthSymbol{pngCodeLengthCodeOrder(orderIndex)}") != std::string::npos);
  CHECK(pngBitstreamBody.find("lengthStatus{pngReadBits(compressed, byteIndex, bitIndex, 3i32, lengthValue)}") !=
        std::string::npos);
  CHECK(pngBitstreamBody.find("totalCodeCount{literalCount + distanceCount}") != std::string::npos);
  CHECK(pngBitstreamBody.find("[mut] symbol{0i32}") != std::string::npos);
  CHECK(pngBitstreamBody.find("repeatCount{11i32 + repeatExtra}") != std::string::npos);
  CHECK(pngBitstreamBody.find("highIndex{symbol - count(literalLengthsLow)}") != std::string::npos);
  CHECK(pngBitstreamBody.find("lowMax{pngHuffmanMaxBits(literalLengthsLow)}") != std::string::npos);

  CHECK(pngBitstreamBody.find("[i32 mut] remaining{count}") == std::string::npos);
  CHECK(pngBitstreamBody.find("[i32] status{ppmReadByteStatus(file, byte)}") == std::string::npos);
  CHECK(pngBitstreamBody.find("[i32] len{count(bytes)}") == std::string::npos);
  CHECK(pngBitstreamBody.find("[i32 mut] factor{1i32}") == std::string::npos);
  CHECK(pngBitstreamBody.find("[vector<i32> mut] codeLengthLengths{vector<i32>()}") == std::string::npos);
  CHECK(pngBitstreamBody.find("[i32] codeLengthSymbol{pngCodeLengthCodeOrder(orderIndex)}") == std::string::npos);
  CHECK(pngBitstreamBody.find("[i32] lengthStatus{pngReadBits(compressed, byteIndex, bitIndex, 3i32, lengthValue)}") ==
        std::string::npos);
  CHECK(pngBitstreamBody.find("[i32] totalCodeCount{literalCount + distanceCount}") == std::string::npos);
  CHECK(pngBitstreamBody.find("[i32 mut] symbol{0i32}") == std::string::npos);
  CHECK(pngBitstreamBody.find("[i32] repeatCount{11i32 + repeatExtra}") == std::string::npos);
  CHECK(pngBitstreamBody.find("[i32] highIndex{symbol - count(literalLengthsLow)}") == std::string::npos);
  CHECK(pngBitstreamBody.find("[i32] lowMax{pngHuffmanMaxBits(literalLengthsLow)}") == std::string::npos);

  CHECK(pngInflateExecBody.find("[mut] copyIndex{0i32}") != std::string::npos);
  CHECK(pngInflateExecBody.find("trailerStart{count(compressed) - 4i32}") != std::string::npos);
  CHECK(pngInflateExecBody.find("blockLen{compressed[byteIndex] + multiply(compressed[byteIndex + 1i32], 256i32)}") !=
        std::string::npos);
  CHECK(pngInflateExecBody.find("[mut] symbol{0i32}") != std::string::npos);
  CHECK(pngInflateExecBody.find("status{pngHuffmanDecodeSymbol(compressed, byteIndex, bitIndex, literalLengths, literalMaxBits, symbol)}") !=
        std::string::npos);
  CHECK(pngInflateExecBody.find("matchLength{lengthBase + lengthExtraValue}") != std::string::npos);
  CHECK(pngInflateExecBody.find("copyStatus{pngInflateCopyFromOutput(output, distanceBase + distanceExtraValue, matchLength)}") !=
        std::string::npos);
  CHECK(pngInflateExecBody.find("[mut] hlitBits{0i32}") != std::string::npos);
  CHECK(pngInflateExecBody.find("[mut] literalLengthsLow{vector<i32>()}") != std::string::npos);
  CHECK(pngInflateExecBody.find("lengthStatus{pngReadDynamicHuffmanLengths(compressed, byteIndex, bitIndex, hclenBits + 4i32,") !=
        std::string::npos);
  CHECK(pngInflateExecBody.find("compressedCount{count(compressed)}") != std::string::npos);
  CHECK(pngInflateExecBody.find("finalStatus{pngReadBits(compressed, byteIndex, bitIndex, 1i32, isFinal)}") !=
        std::string::npos);
  CHECK(pngInflateExecBody.find("adlerBHigh{compressed[byteIndex]}") != std::string::npos);

  CHECK(pngInflateExecBody.find("[i32 mut] copyIndex{0i32}") == std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] trailerStart{count(compressed) - 4i32}") == std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] blockLen{compressed[byteIndex] + multiply(compressed[byteIndex + 1i32], 256i32)}") ==
        std::string::npos);
  CHECK(pngInflateExecBody.find("[i32 mut] symbol{0i32}") == std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] status{pngHuffmanDecodeSymbol(compressed, byteIndex, bitIndex, literalLengths, literalMaxBits, symbol)}") ==
        std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] matchLength{lengthBase + lengthExtraValue}") == std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] copyStatus{pngInflateCopyFromOutput(output, distanceBase + distanceExtraValue, matchLength)}") ==
        std::string::npos);
  CHECK(pngInflateExecBody.find("[i32 mut] hlitBits{0i32}") == std::string::npos);
  CHECK(pngInflateExecBody.find("[vector<i32> mut] literalLengthsLow{vector<i32>()}") == std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] lengthStatus{pngReadDynamicHuffmanLengths(compressed, byteIndex, bitIndex, hclenBits + 4i32,") ==
        std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] compressedCount{count(compressed)}") == std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] finalStatus{pngReadBits(compressed, byteIndex, bitIndex, 1i32, isFinal)}") ==
        std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] adlerBHigh{compressed[byteIndex]}") == std::string::npos);
}

TEST_CASE("png top-level read write workflows stay source locked to inferred locals") {
  std::filesystem::path imageStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "image" / "image.prime";
  if (!std::filesystem::exists(imageStdlibPath)) {
    imageStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "image" / "image.prime";
  }
  REQUIRE(std::filesystem::exists(imageStdlibPath));

  const std::string imageStdlib = readFile(imageStdlibPath.string());
  const size_t pngReadStart = imageStdlib.find("pngDecodeScanlines");
  const size_t pngWriteStart = imageStdlib.rfind(
      "[effects(file_write), return<int> on_error<FileError, /std/image/ignoreFileError>]\n    writeImpl");
  REQUIRE(pngReadStart != std::string::npos);
  REQUIRE(pngWriteStart != std::string::npos);
  REQUIRE(pngWriteStart > pngReadStart);

  const std::string pngReadBody = imageStdlib.substr(pngReadStart, pngWriteStart - pngReadStart);
  const std::string pngWriteBody = imageStdlib.substr(pngWriteStart);

  CHECK(pngReadBody.find("[mut] offset{0i32}") != std::string::npos);
  CHECK(pngReadBody.find("rowStatus{pngDecodeRows(rawBytes, offset, width, height, bitDepth, colorType, paletteBytes, pixels)}") !=
        std::string::npos);
  CHECK(pngReadBody.find("initStatus{pngInitRgbPixels(width, height, pixels)}") != std::string::npos);
  CHECK(pngReadBody.find("[mut] passPixels{vector<i32>()}") != std::string::npos);
  CHECK(pngReadBody.find("[mut] passIndex{0i32}") != std::string::npos);
  CHECK(pngReadBody.find("passStartX{pngAdam7PassStartX(passIndex)}") != std::string::npos);
  CHECK(pngReadBody.find("passStatus{pngDecodeRows(rawBytes, offset, passWidth, passHeight, bitDepth, colorType, paletteBytes, passPixels)}") !=
        std::string::npos);
  CHECK(pngReadBody.find("[mut] passRow{0i32}") != std::string::npos);
  CHECK(pngReadBody.find("[mut] passColumn{0i32}") != std::string::npos);
  CHECK(pngReadBody.find("imageX{passStartX + multiply(passColumn, passStepX)}") != std::string::npos);
  CHECK(pngReadBody.find("targetOffset{multiply(multiply(imageY, width) + imageX, 3i32)}") != std::string::npos);
  CHECK(pngReadBody.find("[File<Read>] file{File<Read>(path)?}") != std::string::npos);
  CHECK(pngReadBody.find("signatureStatus{pngValidateSignature(file)}") != std::string::npos);
  CHECK(pngReadBody.find("[mut] sawIhdr{0i32}") != std::string::npos);
  CHECK(pngReadBody.find("[mut] idatBytes{vector<i32>()}") != std::string::npos);
  CHECK(pngReadBody.find("[mut] chunkLength{0i32}") != std::string::npos);
  CHECK(pngReadBody.find("lengthStatus{pngReadU32Be(file, chunkLength)}") != std::string::npos);
  CHECK(pngReadBody.find("[mut] typeA{0i32}") != std::string::npos);
  CHECK(pngReadBody.find("ihdrStatus{pngReadIhdr(file, parsedWidth, parsedHeight, bitDepth, colorType, interlace)}") !=
        std::string::npos);
  CHECK(pngReadBody.find("[mut] ihdrBytes{vector<i32>()}") != std::string::npos);
  CHECK(pngReadBody.find("[mut] chunkBytes{vector<i32>()}") != std::string::npos);
  CHECK(pngReadBody.find("inflateStatus{pngInflateDeflateBlocks(idatBytes, rawBytes)}") != std::string::npos);
  CHECK(pngReadBody.find("decodeStatus{pngDecodeScanlines(rawBytes, parsedWidth, parsedHeight, bitDepth, colorType, interlace, plteBytes, pixels)}") !=
        std::string::npos);
  CHECK(pngReadBody.find("status{readImpl(width, height, pixels, path)}") != std::string::npos);

  CHECK(pngReadBody.find("[i32 mut] offset{0i32}") == std::string::npos);
  CHECK(pngReadBody.find("[i32] rowStatus{pngDecodeRows(rawBytes, offset, width, height, bitDepth, colorType, paletteBytes, pixels)}") ==
        std::string::npos);
  CHECK(pngReadBody.find("[i32] initStatus{pngInitRgbPixels(width, height, pixels)}") == std::string::npos);
  CHECK(pngReadBody.find("[vector<i32> mut] passPixels{vector<i32>()}") == std::string::npos);
  CHECK(pngReadBody.find("[i32 mut] passIndex{0i32}") == std::string::npos);
  CHECK(pngReadBody.find("[i32] passStartX{pngAdam7PassStartX(passIndex)}") == std::string::npos);
  CHECK(pngReadBody.find("[i32] passStatus{pngDecodeRows(rawBytes, offset, passWidth, passHeight, bitDepth, colorType, paletteBytes, passPixels)}") ==
        std::string::npos);
  CHECK(pngReadBody.find("[i32 mut] passRow{0i32}") == std::string::npos);
  CHECK(pngReadBody.find("[i32 mut] passColumn{0i32}") == std::string::npos);
  CHECK(pngReadBody.find("[i32] imageX{passStartX + multiply(passColumn, passStepX)}") == std::string::npos);
  CHECK(pngReadBody.find("[i32] targetOffset{multiply(multiply(imageY, width) + imageX, 3i32)}") == std::string::npos);
  CHECK(pngReadBody.find("      file{File<Read>(path)?}") == std::string::npos);
  CHECK(pngReadBody.find("[i32] signatureStatus{pngValidateSignature(file)}") == std::string::npos);
  CHECK(pngReadBody.find("[i32 mut] sawIhdr{0i32}") == std::string::npos);
  CHECK(pngReadBody.find("[vector<i32> mut] idatBytes{vector<i32>()}") == std::string::npos);
  CHECK(pngReadBody.find("[i32 mut] chunkLength{0i32}") == std::string::npos);
  CHECK(pngReadBody.find("[i32] lengthStatus{pngReadU32Be(file, chunkLength)}") == std::string::npos);
  CHECK(pngReadBody.find("[i32 mut] typeA{0i32}") == std::string::npos);
  CHECK(pngReadBody.find("[i32] ihdrStatus{pngReadIhdr(file, parsedWidth, parsedHeight, bitDepth, colorType, interlace)}") ==
        std::string::npos);
  CHECK(pngReadBody.find("[vector<i32> mut] ihdrBytes{vector<i32>()}") == std::string::npos);
  CHECK(pngReadBody.find("[vector<i32> mut] chunkBytes{vector<i32>()}") == std::string::npos);
  CHECK(pngReadBody.find("[i32] inflateStatus{pngInflateDeflateBlocks(idatBytes, rawBytes)}") == std::string::npos);
  CHECK(pngReadBody.find("[i32] decodeStatus{pngDecodeScanlines(rawBytes, parsedWidth, parsedHeight, bitDepth, colorType, interlace, plteBytes, pixels)}") ==
        std::string::npos);
  CHECK(pngReadBody.find("[i32] status{readImpl(width, height, pixels, path)}") == std::string::npos);

  CHECK(pngWriteBody.find("[mut] rawByteCount{0i32}") != std::string::npos);
  CHECK(pngWriteBody.find("[mut] payloadLength{0i32}") != std::string::npos);
  CHECK(pngWriteBody.find("[File<Write>] file{File<Write>(path)?}") != std::string::npos);
  CHECK(pngWriteBody.find("signatureStatus{pngWriteSignature(file)}") != std::string::npos);
  CHECK(pngWriteBody.find("ihdrStatus{pngWriteIhdrChunk(file, width, height)}") != std::string::npos);
  CHECK(pngWriteBody.find("idatStatus{pngWriteIdatChunk(file, width, height, pixels, rawByteCount, payloadLength)}") !=
        std::string::npos);
  CHECK(pngWriteBody.find("status{writeImpl(path, width, height, pixels)}") != std::string::npos);

  CHECK(pngWriteBody.find("[i32 mut] rawByteCount{0i32}") == std::string::npos);
  CHECK(pngWriteBody.find("[i32 mut] payloadLength{0i32}") == std::string::npos);
  CHECK(pngWriteBody.find("      file{File<Write>(path)?}") == std::string::npos);
  CHECK(pngWriteBody.find("[i32] signatureStatus{pngWriteSignature(file)}") == std::string::npos);
  CHECK(pngWriteBody.find("[i32] ihdrStatus{pngWriteIhdrChunk(file, width, height)}") == std::string::npos);
  CHECK(pngWriteBody.find("[i32] idatStatus{pngWriteIdatChunk(file, width, height, pixels, rawByteCount, payloadLength)}") ==
        std::string::npos);
  CHECK(pngWriteBody.find("[i32] status{writeImpl(path, width, height, pixels)}") == std::string::npos);
}

TEST_CASE("gfx stdlib wrappers stay source locked to parser-safe locals") {
  std::filesystem::path gfxStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "gfx" / "gfx.prime";
  if (!std::filesystem::exists(gfxStdlibPath)) {
    gfxStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "gfx" / "gfx.prime";
  }
  REQUIRE(std::filesystem::exists(gfxStdlibPath));

  const std::string gfxStdlib = readFile(gfxStdlibPath.string());

  CHECK(gfxStdlib.find("[SubstrateSwapchainConfig] config{\n        [window] window,") !=
        std::string::npos);
  CHECK(gfxStdlib.find("[i32] swapchainToken{GraphicsSubstrate.createSwapchain(config)?}") !=
        std::string::npos);
  CHECK(gfxStdlib.find("[i32] vertexCount{count(vertices)}") != std::string::npos);
  CHECK(gfxStdlib.find("[i32] indexCount{count(indices)}") != std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateMeshConfig] config{") != std::string::npos);
  CHECK(gfxStdlib.find("[i32] meshToken{GraphicsSubstrate.createMesh(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("[SubstratePipelineConfig] config{") != std::string::npos);
  CHECK(gfxStdlib.find("[i32] pipelineToken{GraphicsSubstrate.createPipeline(config)?}") !=
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateFrameConfig] config{") != std::string::npos);
  CHECK(gfxStdlib.find("[i32] frameToken{GraphicsSubstrate.acquireFrame(config)?}") !=
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateRenderPassConfig] config{") != std::string::npos);
  CHECK(gfxStdlib.find("[i32] renderPassToken{GraphicsSubstrate.openRenderPass(config)}") !=
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateDrawMeshConfig] config{") != std::string::npos);
  CHECK(gfxStdlib.find("[i32] drawToken{GraphicsSubstrate.drawMesh(config)}") != std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateRenderPassEndConfig] config{") != std::string::npos);
  CHECK(gfxStdlib.find("[i32] endToken{GraphicsSubstrate.endRenderPass(config)}") != std::string::npos);
  CHECK(gfxStdlib.find("[Window] window{[token] 1i32, [width] 1i32, [height] 1i32}") !=
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateDeviceConfig] config{") != std::string::npos);
  CHECK(gfxStdlib.find("[i32] deviceToken{GraphicsSubstrate.createDevice(config)?}") !=
        std::string::npos);
  CHECK(gfxStdlib.find("[i32] queueToken{GraphicsSubstrate.createQueue(config)?}") !=
        std::string::npos);

  CHECK(gfxStdlib.find("      config{\n        SubstrateSwapchainConfig{") == std::string::npos);
  CHECK(gfxStdlib.find("SubstrateSwapchainConfig{") == std::string::npos);
  CHECK(gfxStdlib.find("      swapchainToken{GraphicsSubstrate.createSwapchain(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("      vertexCount{count(vertices)}") == std::string::npos);
  CHECK(gfxStdlib.find("      indexCount{count(indices)}") == std::string::npos);
  CHECK(gfxStdlib.find("      meshToken{GraphicsSubstrate.createMesh(config)?}") == std::string::npos);
  CHECK(gfxStdlib.find("      pipelineToken{GraphicsSubstrate.createPipeline(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("      frameToken{GraphicsSubstrate.acquireFrame(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("      renderPassToken{GraphicsSubstrate.openRenderPass(config)}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("      drawToken{GraphicsSubstrate.drawMesh(config)}") == std::string::npos);
  CHECK(gfxStdlib.find("      endToken{GraphicsSubstrate.endRenderPass(config)}") == std::string::npos);
  CHECK(gfxStdlib.find("    window{Window{[token] 1i32, [width] 1i32, [height] 1i32}}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[Window] window{Window{[token] 1i32, [width] 1i32, [height] 1i32}}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("    deviceToken{GraphicsSubstrate.createDevice(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("    queueToken{GraphicsSubstrate.createQueue(config)?}") ==
        std::string::npos);
}

TEST_CASE("ui stdlib workflows stay source locked to inferred locals") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  CHECK(source.find("[mut] records{self.records}") != std::string::npos);
  CHECK(source.find("len{text.count()}") != std::string::npos);
  CHECK(source.find("self.append_word(at(text, index))") != std::string::npos);
  CHECK(source.find("for([i32 mut] index{0i32}, index < len, ++index)") != std::string::npos);
  CHECK(source.find("[mut] words{vector<i32>()}") != std::string::npos);
  CHECK(source.find("[mut] kinds{self.kinds}") != std::string::npos);
  CHECK(source.find("/std/collections/vector/push(records, value)") != std::string::npos);
  CHECK(source.find("/std/collections/vector/push(words, 1i32)") != std::string::npos);
  CHECK(source.find("/std/collections/vector/at(records, index)") != std::string::npos);
  CHECK(source.find("/std/collections/vector/push(kinds, kind)") != std::string::npos);
  CHECK(source.find("/std/collections/vector/push(rectHeights, 0i32)") != std::string::npos);
  CHECK(source.find("panel{self.append_panel(parentIndex, panelPaddingPx, panelGapPx, 0i32, 0i32)}") !=
        std::string::npos);
  CHECK(source.find("contentWidth{widget_text_width(textSizePx, text) + paddingPx * 2i32}") !=
        std::string::npos);
  CHECK(source.find("[i32 mut] nodeId{self.kinds.count() - 1i32}") != std::string::npos);
  CHECK(source.find("totalNodes{self.kinds.count()}") != std::string::npos);
  CHECK(source.find("paddingPx{/std/collections/vector/at(self.paddingPxs, nodeId)}") != std::string::npos);
  CHECK(source.find("[i32 mut] childY{innerY}") != std::string::npos);

  CHECK(source.find("[vector<i32> mut] records{self.records}") == std::string::npos);
  CHECK(source.find("[i32] len{text.count()}") == std::string::npos);
  CHECK(source.find("text[index]") == std::string::npos);
  CHECK(source.find("/std/collections/vector/at(text, index)") == std::string::npos);
  CHECK(source.find("[vector<i32> mut] words{vector<i32>()}") == std::string::npos);
  CHECK(source.find("[vector<i32> mut] kinds{self.kinds}") == std::string::npos);
  CHECK(source.find("[i32] panel{self.append_panel(parentIndex, panelPaddingPx, panelGapPx, 0i32, 0i32)}") ==
        std::string::npos);
  CHECK(source.find("[i32] contentWidth{widget_text_width(textSizePx, text) + paddingPx * 2i32}") ==
        std::string::npos);
  CHECK(source.find("[mut] nodeId{self.kinds.count() - 1i32}") == std::string::npos);
  CHECK(source.find("[i32] totalNodes{self.kinds.count()}") == std::string::npos);
  CHECK(source.find("[i32] paddingPx{/std/collections/vector/at(self.paddingPxs, nodeId)}") ==
        std::string::npos);
  CHECK(source.find("[mut] childY{innerY}") == std::string::npos);
  CHECK(source.find("self.records[index]") == std::string::npos);
  CHECK(source.find("self.paddingPxs[nodeId]") == std::string::npos);
}

TEST_CASE("surface examples stay source locked to lowering-compatible helper forms") {
  std::filesystem::path featuresOverviewPath =
      std::filesystem::path("..") / "examples" / "3.Surface" / "features_overview.prime";
  std::filesystem::path raytracerPath =
      std::filesystem::path("..") / "examples" / "3.Surface" / "raytracer.prime";
  if (!std::filesystem::exists(featuresOverviewPath)) {
    featuresOverviewPath =
        std::filesystem::current_path() / "examples" / "3.Surface" / "features_overview.prime";
  }
  if (!std::filesystem::exists(raytracerPath)) {
    raytracerPath = std::filesystem::current_path() / "examples" / "3.Surface" / "raytracer.prime";
  }
  REQUIRE(std::filesystem::exists(featuresOverviewPath));
  REQUIRE(std::filesystem::exists(raytracerPath));

  const std::string featuresOverview = readFile(featuresOverviewPath.string());
  const std::string raytracer = readFile(raytracerPath.string());

  CHECK(featuresOverview.find("/std/collections/vector/at(scores, idx)") != std::string::npos);
  CHECK(featuresOverview.find("scores.at(idx)") == std::string::npos);
  CHECK(featuresOverview.find("scores[idx]") == std::string::npos);
  CHECK(featuresOverview.find("if(value > best)") != std::string::npos);
  CHECK(featuresOverview.find("best = value") != std::string::npos);
  CHECK(featuresOverview.find("best = max(best, value)") == std::string::npos);

  CHECK(raytracer.find("[ColorRGB] clamped{") != std::string::npos);
  CHECK(raytracer.find("min(1.0, max(0.0, scaled.r))") != std::string::npos);
  CHECK(raytracer.find("min(1.0, max(0.0, scaled.g))") != std::string::npos);
  CHECK(raytracer.find("min(1.0, max(0.0, scaled.b))") != std::string::npos);
  CHECK(raytracer.find("[Vec3] refrVec{refract_dir(currentDir, hitNormal, ior)}") != std::string::npos);
  CHECK(raytracer.find("clamp(scaled.r, 0.0, 1.0)") == std::string::npos);
  CHECK(raytracer.find("return(scaled.clamp(0.0, 1.0))") == std::string::npos);
  CHECK(raytracer.find("currentDir.refract(hitNormal, ior)") == std::string::npos);
}

TEST_CASE("gfx stdlib compatibility shim stays source locked") {
  std::filesystem::path gfxStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "gfx" / "gfx.prime";
  std::filesystem::path gfxExperimentalPath =
      std::filesystem::path("..") / "stdlib" / "std" / "gfx" / "experimental.prime";
  if (!std::filesystem::exists(gfxStdlibPath)) {
    gfxStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "gfx" / "gfx.prime";
  }
  if (!std::filesystem::exists(gfxExperimentalPath)) {
    gfxExperimentalPath = std::filesystem::current_path() / "stdlib" / "std" / "gfx" / "experimental.prime";
  }
  REQUIRE(std::filesystem::exists(gfxStdlibPath));
  REQUIRE(std::filesystem::exists(gfxExperimentalPath));

  const std::string gfxStdlib = readFile(gfxStdlibPath.string());
  const std::string gfxExperimental = readFile(gfxExperimentalPath.string());

  CHECK(gfxExperimental.find("// Legacy compatibility shim over canonical /std/gfx/*.") !=
        std::string::npos);
  CHECK(gfxExperimental.find("New public gfx code should import /std/gfx/*; this namespace remains only") !=
        std::string::npos);
  CHECK(gfxStdlib.find("return(Queue{[token] this.token + 1i32})") != std::string::npos);
  CHECK(gfxStdlib.find("return(this.token > 0i32)") != std::string::npos);
  CHECK(gfxStdlib.find("if(this.height < 1i32)") != std::string::npos);
  CHECK(gfxStdlib.find("if(this.token < 1i32 || window.token < 1i32)") != std::string::npos);
  CHECK(gfxStdlib.find("if(this.token < 1i32 || vertexCount < 1i32 || indexCount < 1i32)") !=
        std::string::npos);
  CHECK(gfxStdlib.find("if(this.token < 1i32)") != std::string::npos);
  CHECK(gfxStdlib.find("if(drawToken == 0i32)") != std::string::npos);
  CHECK(gfxStdlib.find("if(endToken == 0i32)") != std::string::npos);
  CHECK(gfxStdlib.find("return(this.elementCount < 1i32)") != std::string::npos);
  CHECK(gfxStdlib.find("[swapchainToken] this.token + window.token + 1i32") != std::string::npos);
  CHECK(gfxStdlib.find("[meshToken] this.token + vertexCount + indexCount") != std::string::npos);
  CHECK(gfxStdlib.find("[pipelineToken] shader.value + this.token + 5i32") != std::string::npos);
  CHECK(gfxStdlib.find("[drawToken] this.token + mesh.token + material.token") != std::string::npos);
  CHECK(gfxStdlib.find("if(not_equal(queueToken, deviceToken + 1i32))") != std::string::npos);
  CHECK(gfxStdlib.find("return(less_than(0i32, this.token))") == std::string::npos);
  CHECK(gfxStdlib.find("if(less_than(this.height, 1i32))") == std::string::npos);
  CHECK(gfxStdlib.find("if(or(less_than(this.token, 1i32), less_than(window.token, 1i32)))") ==
        std::string::npos);
  CHECK(gfxStdlib.find("if(or(less_than(this.token, 1i32),\n"
                       "            or(less_than(vertexCount, 1i32), less_than(indexCount, 1i32))))") ==
        std::string::npos);
  CHECK(gfxStdlib.find("if(equal(drawToken, 0i32))") == std::string::npos);
  CHECK(gfxStdlib.find("if(equal(endToken, 0i32))") == std::string::npos);
  CHECK(gfxStdlib.find("plus(") == std::string::npos);
  CHECK(gfxExperimental.find("import /std/gfx/*") != std::string::npos);
  CHECK(gfxExperimental.find("targeted compatibility coverage and staged migration support.") != std::string::npos);
  CHECK(gfxExperimental.find("Route behavior through the canonical helper surface whenever the old type") !=
        std::string::npos);
  CHECK(gfxExperimental.find("return(greater_than(this.token, 0i32))") != std::string::npos);
  CHECK(gfxExperimental.find("return(Queue{[token] plus(this.token, 1i32)})") != std::string::npos);
  CHECK(gfxExperimental.find("return(RenderPass{[token] renderPassToken})") != std::string::npos);
  CHECK(gfxExperimental.find("[SubstrateDrawMeshConfig] config{\n        [renderPass] this,") !=
        std::string::npos);
  CHECK(gfxExperimental.find("SubstrateDrawMeshConfig{") == std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gfx/Buffer/readback<T>(canonical))") !=
        std::string::npos);
  CHECK(gfxExperimental.find("[/std/gfx/Buffer<T>] canonical{/std/gfx/Buffer/upload<T>(values)}") !=
        std::string::npos);
  CHECK(gfxExperimental.find("[Buffer<T>] result{[token] canonical.token, [elementCount] canonical.elementCount}") !=
        std::string::npos);
  CHECK(gfxExperimental.find("return(canonicalWindow(this).is_open())") == std::string::npos);
  CHECK(gfxExperimental.find("return(experimentalQueue(canonicalDevice(this).default_queue()))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("return(experimentalRenderPass(canonicalFrame(this).render_pass(clear_color, clear_depth)))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("canonicalRenderPass(this).draw_mesh(canonicalMesh(mesh), canonicalMaterial(material))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gfx/Buffer/readback<T>(canonicalBuffer<T>(self)))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("return(experimentalBuffer<T>(/std/gfx/Buffer/upload<T>(values)))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gfx/GfxError/why(canonicalGfxError(err)))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gpu/readback(self))") == std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gpu/upload(values))") == std::string::npos);
  CHECK(gfxExperimental.find("return(less_than(0i32, this.token))") == std::string::npos);
}

TEST_CASE("ui stdlib arithmetic and assignment stay source locked to surface operators") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  CHECK(source.find("assign(") == std::string::npos);
  CHECK(source.find("plus(") == std::string::npos);
  CHECK(source.find("minus(") == std::string::npos);
  CHECK(source.find("less_than(") == std::string::npos);
  CHECK(source.find("equal(") == std::string::npos);
  CHECK(source.find("greater_than(") == std::string::npos);
  CHECK(source.find("greater_equal(") == std::string::npos);
  CHECK(source.find("/std/math/max(") == std::string::npos);
  CHECK(source.find("self.commandCount = self.commandCount + 1i32") != std::string::npos);
  CHECK(source.find("self.records = records") != std::string::npos);
  CHECK(source.find("for([i32 mut] index{0i32}, index < len, ++index)") != std::string::npos);
  CHECK(source.find("while(nodeId >= 0i32)") != std::string::npos);
  CHECK(source.find("if(self.kinds.count() == 0i32)") != std::string::npos);
  CHECK(source.find("[i32 mut] nodeId{self.kinds.count() - 1i32}") != std::string::npos);
  CHECK(source.find("childY = childY + /std/collections/vector/at(self.measuredHeights, childId) +") !=
        std::string::npos);
  CHECK(source.find("return(max(1i32, (textSizePx + 1i32) / 2i32))") != std::string::npos);
  CHECK(source.find("return(widget_text_advance(textSizePx) * text.count())") != std::string::npos);
}

TEST_CASE("ui scene producer composite widgets stay locked to basic widgets") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  const size_t drawLoginStart = source.find("draw_login_form(");
  const size_t pushClipStart = source.find("\n    [public effects(heap_alloc), return<void>]\n    push_clip(");
  REQUIRE(drawLoginStart != std::string::npos);
  REQUIRE(pushClipStart != std::string::npos);
  REQUIRE(pushClipStart > drawLoginStart);
  const std::string drawLoginBody = source.substr(drawLoginStart, pushClipStart - drawLoginStart);
  CHECK(drawLoginBody.find("self.begin_panel(") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_label(") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_input(") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_button(") != std::string::npos);
  CHECK(drawLoginBody.find("self.end_panel()") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_text(") == std::string::npos);
  CHECK(drawLoginBody.find("self.draw_rounded_rect(") == std::string::npos);
  CHECK(drawLoginBody.find("self.push_clip(") == std::string::npos);
  CHECK(drawLoginBody.find("self.pop_clip(") == std::string::npos);

  const size_t appendLoginStart = source.find("append_login_form(");
  const size_t appendLeafStart = source.find("\n    [public effects(heap_alloc), return<i32>]\n    append_leaf(");
  REQUIRE(appendLoginStart != std::string::npos);
  REQUIRE(appendLeafStart != std::string::npos);
  REQUIRE(appendLeafStart > appendLoginStart);
  const std::string appendLoginBody = source.substr(appendLoginStart, appendLeafStart - appendLoginStart);
  CHECK(appendLoginBody.find("self.append_panel(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_label(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_input(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_button(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_leaf(") == std::string::npos);
  CHECK(appendLoginBody.find("self.append_column(") == std::string::npos);
  CHECK(appendLoginBody.find("self.append_node(") == std::string::npos);
}

TEST_CASE("ui html adapter stays source locked to shared widgets") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  const size_t emitLoginStart = source.find("emit_login_form(");
  const size_t serializeStart =
      source.find("\n    [public effects(heap_alloc), return<vector<i32>>]\n    serialize(", emitLoginStart);
  REQUIRE(emitLoginStart != std::string::npos);
  REQUIRE(serializeStart != std::string::npos);
  REQUIRE(serializeStart > emitLoginStart);
  const std::string emitLoginBody = source.substr(emitLoginStart, serializeStart - emitLoginStart);
  CHECK(emitLoginBody.find("self.emit_panel(") != std::string::npos);
  CHECK(emitLoginBody.find("self.emit_label(") != std::string::npos);
  CHECK(emitLoginBody.find("self.emit_input(") != std::string::npos);
  CHECK(emitLoginBody.find("self.emit_button(") != std::string::npos);
  CHECK(emitLoginBody.find("self.append_word(") == std::string::npos);
  CHECK(emitLoginBody.find("self.append_color(") == std::string::npos);
  CHECK(emitLoginBody.find("self.append_string(") == std::string::npos);
}

TEST_CASE("ui event stream stays source locked to normalized helpers") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  const size_t pointerMoveStart = source.find("push_pointer_move(");
  const size_t keyDownStart =
      source.find("\n    [public effects(heap_alloc), return<void>]\n    push_key_down(", pointerMoveStart);
  REQUIRE(pointerMoveStart != std::string::npos);
  REQUIRE(keyDownStart != std::string::npos);
  REQUIRE(keyDownStart > pointerMoveStart);
  const std::string pointerBody = source.substr(pointerMoveStart, keyDownStart - pointerMoveStart);
  CHECK(pointerBody.find("self.append_pointer_event(1i32, targetNodeId, pointerId, -1i32, x, y)") !=
        std::string::npos);
  CHECK(pointerBody.find("self.append_pointer_event(2i32, targetNodeId, pointerId, button, x, y)") !=
        std::string::npos);
  CHECK(pointerBody.find("self.append_pointer_event(3i32, targetNodeId, pointerId, button, x, y)") !=
        std::string::npos);
  CHECK(pointerBody.find("self.append_word(") == std::string::npos);

  const size_t eventCountStart =
      source.find("\n    [public return<i32>]\n    event_count(", keyDownStart);
  const size_t imeStart =
      source.find("\n    [public effects(heap_alloc), return<void>]\n    push_ime_preedit(", keyDownStart);
  REQUIRE(imeStart != std::string::npos);
  REQUIRE(imeStart > keyDownStart);
  REQUIRE(eventCountStart != std::string::npos);
  const size_t resizeStart =
      source.find("\n    [public effects(heap_alloc), return<void>]\n    push_resize(", imeStart);
  REQUIRE(resizeStart != std::string::npos);
  REQUIRE(resizeStart > imeStart);
  REQUIRE(eventCountStart > resizeStart);
  const std::string keyBody = source.substr(keyDownStart, imeStart - keyDownStart);
  CHECK(keyBody.find("self.append_key_event(4i32, targetNodeId, keyCode, modifierMask, isRepeat)") !=
        std::string::npos);
  CHECK(keyBody.find("self.append_key_event(5i32, targetNodeId, keyCode, modifierMask, 0i32)") !=
        std::string::npos);
  CHECK(keyBody.find("self.append_word(") == std::string::npos);

  const std::string imeBody = source.substr(imeStart, resizeStart - imeStart);
  CHECK(imeBody.find("self.append_ime_event(6i32, targetNodeId, selectionStart, selectionEnd, text)") !=
        std::string::npos);
  CHECK(imeBody.find("self.append_ime_event(7i32, targetNodeId, -1i32, -1i32, text)") !=
        std::string::npos);
  CHECK(imeBody.find("self.append_word(") == std::string::npos);
  CHECK(imeBody.find("self.append_string(") == std::string::npos);

  const std::string viewBody = source.substr(resizeStart, eventCountStart - resizeStart);
  CHECK(viewBody.find("self.append_view_event(8i32, targetNodeId, width, height)") !=
        std::string::npos);
  CHECK(viewBody.find("self.append_view_event(9i32, targetNodeId, 0i32, 0i32)") !=
        std::string::npos);
  CHECK(viewBody.find("self.append_view_event(10i32, targetNodeId, 0i32, 0i32)") !=
        std::string::npos);
  CHECK(viewBody.find("self.append_word(") == std::string::npos);
}


TEST_SUITE_END();
