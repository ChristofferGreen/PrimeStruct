#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

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
  CHECK(codeExamples.find("`stdlib/std/collections/soa_vector.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/soa_vector_conversions.prime`") != std::string::npos);
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
  CHECK(codeExamples.find("`stdlib/std/collections` is intentionally mixed") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/gfx` is intentionally mixed") != std::string::npos);

  CHECK(primeStructDoc.find("### Stdlib Surface-Style Boundary") != std::string::npos);
  CHECK(primeStructDoc.find("This boundary is the scope reference for the stdlib surface-style cleanup lane in") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/vector.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/collections.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/experimental_vector.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/experimental_map.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/experimental_soa_vector.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/experimental_soa_vector_conversions.prime`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/internal_*`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/gfx/experimental.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections` and `stdlib/std/gfx`") != std::string::npos);

  CHECK(agents.find("For stdlib style work, treat `stdlib/std/math`, `maybe`, `file`, `image`,") !=
        std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/collections.prime`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/experimental_vector.prime`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/experimental_map.prime`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/experimental_soa_vector.prime`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/experimental_soa_vector_conversions.prime`,") !=
        std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/internal_*`, and") != std::string::npos);
  CHECK(agents.find("`stdlib/std/gfx/experimental.prime` as canonical/bridge code") !=
        std::string::npos);
}

TEST_CASE("vector map bridge boundary docs stay source locked") {
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

  CHECK(primeStructDoc.find("### Vector/Map Bridge Contract") != std::string::npos);
  CHECK(primeStructDoc.find("**Bridge-owned public contract:** exact and wildcard `/std/collections`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Migration-only seams:** rooted `/vector/*` and `/map/*` spellings plus") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Out of scope for this bridge lane:** `array<T>` core ownership,") !=
        std::string::npos);

  CHECK(todo.find("### Vector/Map Bridge Contract Summary") != std::string::npos);
  CHECK(todo.find("Bridge-owned public contract: exact and wildcard `/std/collections` imports,") !=
        std::string::npos);
  CHECK(todo.find("Migration-only seams: rooted `/vector/*` and `/map/*` spellings,") !=
        std::string::npos);
  CHECK(todo.find("Outside this lane: `array<T>` core ownership, `soa_vector<T>` maturity, and") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4042:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4043:") == std::string::npos);
  CHECK(todo.find("TODO-4044") == std::string::npos);
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
            "`/std/collections/experimental_vector/*` family now remains only as the internal implementation seam behind that") !=
        std::string::npos);
  CHECK(primeStructDoc.find(
            "`/std/collections/experimental_map/*` family now remains only as the internal implementation seam behind that") !=
        std::string::npos);
  CHECK(primeStructDoc.find("no `experimental` namespace counts as canonical public API") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/experimental_vector/*` | Internal substrate/helper namespace | Internal implementation module behind the canonical `/std/collections/vector/*` public contract; direct imports remain only for targeted compatibility or conformance coverage. | none |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/experimental_map/*` | Internal substrate/helper namespace | Internal implementation module behind the canonical `/std/collections/map/*` public contract; direct imports remain only for targeted compatibility or conformance coverage. | none |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/gfx/experimental/*` | Temporary compatibility namespace | Legacy compatibility shim over canonical `/std/gfx/*`; no longer part of the public gfx contract and retained only for targeted compatibility coverage while the residual seam remains importable. | none |") !=
        std::string::npos);
  CHECK(primeStructDoc.find("| `/std/collections/internal_buffer_checked/*` | Internal substrate/helper namespace | Explicitly internal checked buffer plumbing for container conformance and memory-wrapper flows, not a stable user-facing stdlib API. | none |") !=
        std::string::npos);

  CHECK(todo.find("### Stdlib De-Experimentalization Policy Summary") != std::string::npos);
  CHECK(todo.find("Canonical public API: non-`experimental` namespaces are the intended") !=
        std::string::npos);
  CHECK(todo.find("Canonical collection contract: `/std/collections/vector/*` and") !=
        std::string::npos);
  CHECK(todo.find("Internal collection implementation modules:") != std::string::npos);
  CHECK(todo.find("/std/collections/experimental_vector/*` and") != std::string::npos);
  CHECK(todo.find("/std/collections/experimental_map/*` now remain implementation-owned seams") !=
        std::string::npos);
  CHECK(todo.find("Stdlib de-experimentalization: TODO-4059") == std::string::npos);
  CHECK(todo.find("TODO-4103: Rename the remaining experimental SoA storage namespace") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4059:") == std::string::npos);
  CHECK(todo.find("Legacy gfx compatibility seam: `/std/gfx/experimental/*` remains importable") !=
        std::string::npos);
  CHECK(todo.find("/std/collections/experimental_soa_vector/*`, and") !=
        std::string::npos);
  CHECK(todo.find("/std/collections/internal_buffer_checked/*`,") != std::string::npos);
  CHECK(todo.find("/std/collections/internal_buffer_unchecked/*`, and") != std::string::npos);
  CHECK(todo.find("/std/collections/internal_soa_storage/*` are implementation-facing") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4103:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4052:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4053:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4054:") == std::string::npos);
  CHECK(todo.find("TODO-4055") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4056:") == std::string::npos);
}

TEST_CASE("soa maturity track docs stay source locked") {
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

  CHECK(primeStructDoc.find("### SoA Maturity Track") != std::string::npos);
  CHECK(primeStructDoc.find("`soa_vector<T>` remains an incubating public extension") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Current user-facing experiment surface:** `/std/collections/soa_vector/*`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`/std/collections/soa_vector_conversions/*` are the canonical spellings") !=
        std::string::npos);
  CHECK(primeStructDoc.find("**Current canonical SoA experiment surface:** public docs/examples should now use") !=
        std::string::npos);
  CHECK(primeStructDoc.find("/std/collections/experimental_soa_vector/*") != std::string::npos);
  CHECK(primeStructDoc.find("/std/collections/internal_soa_storage/*") != std::string::npos);
  CHECK(primeStructDoc.find("Incubation boundary locked; add a new promotion/retreat task only if the maturity decision changes.") !=
        std::string::npos);
  CHECK(primeStructDoc.find("borrowed-view/lifetime rules, backend/runtime parity") !=
        std::string::npos);

  CHECK(todo.find("### SoA Maturity Track Summary") != std::string::npos);
  CHECK(todo.find("`soa_vector<T>` remains an incubating public extension") !=
        std::string::npos);
  CHECK(todo.find("/std/collections/soa_vector/*` and") != std::string::npos);
  CHECK(todo.find("/std/collections/experimental_soa_vector_conversions/*` remain bridge seams") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4058:") == std::string::npos);
  CHECK(todo.find("TODO-4103") == std::string::npos);
  CHECK(todo.find("TODO-4059") == std::string::npos);
}

TEST_CASE("skipped doctest debt queue stays source locked") {
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

  CHECK(todo.find("### Ready Now (Live Leaves; No Unmet TODO Dependencies)\n\n- TODO-4134\n- TODO-4133\n- TODO-4132") !=
        std::string::npos);
  CHECK(todo.find("### Immediate Next 10 (After Ready Now)\n\n- TODO-4122\n- TODO-4123\n- TODO-4124\n- TODO-4125") !=
        std::string::npos);
  CHECK(todo.find("- Skipped doctest debt: TODO-4107") ==
        std::string::npos);
  CHECK(todo.find("### Execution Queue (Recommended)\n\n1. TODO-4134\n2. TODO-4133\n3. TODO-4132\n4. TODO-4122") !=
        std::string::npos);
  CHECK(todo.find("| Release benchmark/example suite stability and doctest governance | none |") !=
        std::string::npos);
  CHECK(todo.find("### Skipped Doctest Debt Summary") != std::string::npos);
  CHECK(todo.find("Retained `doctest::skip(true)` coverage is currently absent from the active") !=
        std::string::npos);
  CHECK(todo.find("queue because no skipped doctest cases remain under `tests/unit`.") !=
        std::string::npos);
  CHECK(todo.find("New skipped doctest coverage must create a new explicit TODO before it lands.") !=
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4117:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4118:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4110:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4106: Re-enable or prune skipped collection compatibility suites") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4107: Re-enable or prune residual skipped IR, docs, and smoke suites") ==
        std::string::npos);
  CHECK(todo.find("### Ready Now (Live Leaves; No Unmet TODO Dependencies)\n\n- none") ==
        std::string::npos);
  CHECK(todo.find("- [ ] TODO-4105:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4109:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4112:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4114:") == std::string::npos);
  CHECK(todo.find("- [ ] TODO-4116:") == std::string::npos);

  CHECK(todoFinished.find("✓ TODO-4118: Re-enable or prune remaining VM non-i32 map key runtime skips.") !=
        std::string::npos);
  CHECK(todoFinished.find("✓ TODO-4107: Re-enable or prune residual skipped IR, docs, and smoke suites.") !=
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

  CHECK(vmMath.find("TEST_CASE(\"runs vm with qualified math names\")") != std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"runs vm support-matrix math nominal helpers\")") !=
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"rejects vm quaternion reference multiply and rotation during lowering\")") !=
        std::string::npos);
  CHECK(vmMath.find("TEST_CASE(\"rejects vm matrix composition order references during lowering\")") !=
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

  CHECK(examplesDocs.find("TEST_CASE(\"collection docs snippets stay c++ style and executable\")") !=
        std::string::npos);
  CHECK(examplesDocs.find(
            "TEST_CASE(\"collection docs snippets stay c++ style and executable\" * doctest::skip(true))") ==
        std::string::npos);
}

TEST_CASE("software renderer command list docs stay source locked") {
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
  CHECK(graphicsDoc.find("[CommandList mut] commands{CommandList()}") != std::string::npos);
  CHECK(graphicsDoc.find("tree.append_root_column(2i32, 3i32, 10i32, 4i32)") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_label(root, 10i32, \"Hi\"utf8)") != std::string::npos);
  CHECK(graphicsDoc.find("commands.draw_button(") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_panel(root, 2i32, 1i32, 20i32, 12i32)") != std::string::npos);
  CHECK(graphicsDoc.find("commands.begin_panel(layout, panel, 4i32") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("commands.draw_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("[HtmlCommandList mut] html{HtmlCommandList()}") != std::string::npos);
  CHECK(graphicsDoc.find("html.emit_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("[UiEventStream mut] events{UiEventStream()}") != std::string::npos);
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
  CHECK(imageStdlib.find("ImageError(1i32)") != std::string::npos);
  CHECK(imageStdlib.find("ImageError(2i32)") != std::string::npos);
  CHECK(imageStdlib.find("ImageError(3i32)") != std::string::npos);
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
  std::filesystem::path lowererPath = std::filesystem::path("..") / "src" / "ir_lowerer" / "IrLowererFileWriteHelpers.cpp";
  std::filesystem::path fileStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "file" / "file.prime";
  std::filesystem::path fileErrorsPath = std::filesystem::path("..") / "stdlib" / "std" / "file" / "errors.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(preludePath)) {
    preludePath = std::filesystem::current_path() / "src" / "emitter" / "EmitterEmitPrelude.h";
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
  REQUIRE(std::filesystem::exists(lowererPath));
  REQUIRE(std::filesystem::exists(fileStdlibPath));
  REQUIRE(std::filesystem::exists(fileErrorsPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string prelude = readFile(preludePath.string());
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
  CHECK(collectionErrors.find("missingKey() {\n      return(ContainerError(1i32))\n    }") !=
        std::string::npos);
  CHECK(collectionErrors.find("missing_key() {\n      return(missingKey())\n    }") !=
        std::string::npos);
  CHECK(collectionErrors.find("indexOutOfBounds() {\n      return(ContainerError(2i32))\n    }") !=
        std::string::npos);
  CHECK(collectionErrors.find("index_out_of_bounds() {\n      return(indexOutOfBounds())\n    }") !=
        std::string::npos);
  CHECK(collectionErrors.find("capacityExceeded() {\n      return(ContainerError(4i32))\n    }") !=
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
  CHECK(primeStructDoc.find("Compatibility wrappers keep `is_empty()` / `is_some()`") != std::string::npos);
  CHECK(primeStructDoc.find("**Helper surface (stdlib):** `is_empty()` / `is_some()`") == std::string::npos);
  CHECK(primeStructDoc.find("Destroy() {\n      if(not(this.empty)) {\n        drop(this.value)\n      }\n    }") !=
        std::string::npos);
  CHECK(primeStructDoc.find("isEmpty() {\n      return(this.empty)\n    }") != std::string::npos);
  CHECK(primeStructDoc.find("isSome() {\n      return(not(this.empty))\n    }") != std::string::npos);
  CHECK(primeStructDoc.find("clear() {\n      if(not(this.empty)) {\n        drop(this.value)\n      }\n      this.empty = true\n    }") !=
        std::string::npos);
  CHECK(maybeStdlib.find("isEmpty() {\n      return(this.empty)\n    }") != std::string::npos);
  CHECK(maybeStdlib.find("isSome() {\n      return(not(this.empty))\n    }") != std::string::npos);
  CHECK(maybeStdlib.find("is_empty() {\n      return(this.isEmpty())\n    }") != std::string::npos);
  CHECK(maybeStdlib.find("is_some() {\n      return(this.isSome())\n    }") != std::string::npos);
  CHECK(maybeStdlib.find("if(not(this.empty)) {\n        drop(this.value)\n      }") != std::string::npos);
  CHECK(maybeStdlib.find("this.empty = true") != std::string::npos);
  CHECK(maybeStdlib.find("this.empty = false") != std::string::npos);
  CHECK(maybeStdlib.find("ref.empty = false") != std::string::npos);
  CHECK(maybeStdlib.find("assign(this.empty, true)") == std::string::npos);
  CHECK(maybeStdlib.find("assign(this.empty, false)") == std::string::npos);
  CHECK(maybeStdlib.find("assign(ref.empty, false)") == std::string::npos);
  CHECK(maybeStdlib.find("if(not(this.empty), then() { drop(this.value) }, else() { })") ==
        std::string::npos);
}

TEST_CASE("small stdlib wrappers stay source locked to inferred locals") {
  std::filesystem::path codeExamplesPath = std::filesystem::path("..") / "docs" / "CodeExamples.md";
  std::filesystem::path maybeStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "maybe" / "maybe.prime";
  std::filesystem::path vectorStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "vector.prime";
  std::filesystem::path mapStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "map.prime";
  std::filesystem::path experimentalVectorStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_vector.prime";
  std::filesystem::path experimentalMapStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_map.prime";
  std::filesystem::path soaConversionsPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "soa_vector_conversions.prime";
  if (!std::filesystem::exists(codeExamplesPath)) {
    codeExamplesPath = std::filesystem::current_path() / "docs" / "CodeExamples.md";
  }
  if (!std::filesystem::exists(maybeStdlibPath)) {
    maybeStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "maybe" / "maybe.prime";
  }
  if (!std::filesystem::exists(vectorStdlibPath)) {
    vectorStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "collections" / "vector.prime";
  }
  if (!std::filesystem::exists(mapStdlibPath)) {
    mapStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "collections" / "map.prime";
  }
  if (!std::filesystem::exists(experimentalVectorStdlibPath)) {
    experimentalVectorStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "experimental_vector.prime";
  }
  if (!std::filesystem::exists(experimentalMapStdlibPath)) {
    experimentalMapStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "experimental_map.prime";
  }
  if (!std::filesystem::exists(soaConversionsPath)) {
    soaConversionsPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "soa_vector_conversions.prime";
  }
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(maybeStdlibPath));
  REQUIRE(std::filesystem::exists(vectorStdlibPath));
  REQUIRE(std::filesystem::exists(mapStdlibPath));
  REQUIRE(std::filesystem::exists(experimentalVectorStdlibPath));
  REQUIRE(std::filesystem::exists(experimentalMapStdlibPath));
  REQUIRE(std::filesystem::exists(soaConversionsPath));

  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::string maybeStdlib = readFile(maybeStdlibPath.string());
  const std::string vectorStdlib = readFile(vectorStdlibPath.string());
  const std::string mapStdlib = readFile(mapStdlibPath.string());
  const std::string experimentalVectorStdlib = readFile(experimentalVectorStdlibPath.string());
  const std::string experimentalMapStdlib = readFile(experimentalMapStdlibPath.string());
  const std::string soaConversions = readFile(soaConversionsPath.string());

  CHECK(codeExamples.find("Internal implementation, bridge, or substrate-oriented code:") !=
        std::string::npos);
  CHECK(codeExamples.find("Intentionally canonical or substrate-oriented code:") == std::string::npos);
  CHECK(codeExamples.find("[mut] current{start}") != std::string::npos);
  CHECK(codeExamples.find("limit{5}") != std::string::npos);

  CHECK(maybeStdlib.find("out{take(this.value)}") != std::string::npos);
  CHECK(maybeStdlib.find("[mut] out{Maybe<T>()}") != std::string::npos);
  CHECK(maybeStdlib.find("[mut] ref{location(out)}") != std::string::npos);
  CHECK(maybeStdlib.find("[T] out{take(this.value)}") == std::string::npos);
  CHECK(maybeStdlib.find("[Maybe<T> mut] out{Maybe<T>()}") == std::string::npos);
  CHECK(maybeStdlib.find("[Reference<Maybe<T>> mut] ref{location(out)}") == std::string::npos);

  CHECK(vectorStdlib.find(
            "// Canonical public wrapper layer over the internal experimental_vector implementation module.") !=
        std::string::npos);
  CHECK(vectorStdlib.find("[mut] out{/std/collections/experimental_vector/vector<T>()}") !=
        std::string::npos);
  CHECK(vectorStdlib.find("valueCount{count(values)}") != std::string::npos);
  CHECK(vectorStdlib.find("[mut] index{0i32}") != std::string::npos);
  CHECK(vectorStdlib.find("[Vector<T> mut] out{/std/collections/experimental_vector/vector<T>()}") ==
        std::string::npos);
  CHECK(vectorStdlib.find("[i32] valueCount{count(values)}") == std::string::npos);
  CHECK(vectorStdlib.find("[i32 mut] index{0i32}") == std::string::npos);

  CHECK(mapStdlib.find(
            "// Canonical public wrapper layer over the internal experimental_map implementation module.") !=
        std::string::npos);
  CHECK(mapStdlib.find("[mut] out{/std/collections/experimental_map/mapNew<K, V>()}") !=
        std::string::npos);
  CHECK(mapStdlib.find("entryCount{count(entries)}") != std::string::npos);
  CHECK(mapStdlib.find("[mut] index{0i32}") != std::string::npos);
  CHECK(mapStdlib.find("current{entries[index]}") != std::string::npos);
  CHECK(mapStdlib.find("[map<K, V> mut] out{/std/collections/experimental_map/mapNew<K, V>()}") ==
        std::string::npos);
  CHECK(mapStdlib.find("[i32] entryCount{count(entries)}") == std::string::npos);
  CHECK(mapStdlib.find("[Entry<K, V>] current{entries[index]}") == std::string::npos);

  CHECK(experimentalVectorStdlib.find("// Internal implementation module behind canonical /std/collections/vector/*.") !=
        std::string::npos);
  CHECK(experimentalVectorStdlib.find(
            "// Direct imports remain only for targeted compatibility or conformance coverage.") !=
        std::string::npos);
  CHECK(experimentalMapStdlib.find("[Vector<K>] keys{this.keys}") != std::string::npos);
  CHECK(experimentalMapStdlib.find("// Internal implementation module behind canonical /std/collections/map/*.") !=
        std::string::npos);
  CHECK(experimentalMapStdlib.find(
            "// Direct imports remain only for targeted compatibility or conformance coverage.") !=
        std::string::npos);
  CHECK(experimentalMapStdlib.find("return(/std/collections/experimental_vector/vectorCount<K>(keys))") !=
        std::string::npos);
  CHECK(experimentalMapStdlib.find("return(mapCount<K, V>(this))") == std::string::npos);

  CHECK(soaConversions.find("valueCount{/std/collections/soa_vector/count<T>(values)}") !=
        std::string::npos);
  CHECK(soaConversions.find("valueCount{/std/collections/soa_vector/count_ref<T>(values)}") !=
        std::string::npos);
  CHECK(soaConversions.find("[vector<T> mut] out{vector<T>()}") != std::string::npos);
  CHECK(soaConversions.find("[mut] index{0i32}") != std::string::npos);
  CHECK(soaConversions.find("[i32] valueCount{/std/collections/soa_vector/count<T>(values)}") ==
        std::string::npos);
  CHECK(soaConversions.find("[i32] valueCount{/std/collections/soa_vector/count_ref<T>(values)}") ==
        std::string::npos);
  CHECK(soaConversions.find("[mut] out{vector<T>()}") == std::string::npos);
  CHECK(soaConversions.find("[i32 mut] index{0i32}") == std::string::npos);
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

TEST_CASE("gfx stdlib wrappers stay source locked to inferred locals") {
  std::filesystem::path gfxStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "gfx" / "gfx.prime";
  if (!std::filesystem::exists(gfxStdlibPath)) {
    gfxStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "gfx" / "gfx.prime";
  }
  REQUIRE(std::filesystem::exists(gfxStdlibPath));

  const std::string gfxStdlib = readFile(gfxStdlibPath.string());

  CHECK(gfxStdlib.find("config{\n        SubstrateSwapchainConfig(") != std::string::npos);
  CHECK(gfxStdlib.find("swapchainToken{GraphicsSubstrate.createSwapchain(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("vertexCount{count(vertices)}") != std::string::npos);
  CHECK(gfxStdlib.find("indexCount{count(indices)}") != std::string::npos);
  CHECK(gfxStdlib.find("meshToken{GraphicsSubstrate.createMesh(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("pipelineToken{GraphicsSubstrate.createPipeline(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("frameToken{GraphicsSubstrate.acquireFrame(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("renderPassToken{GraphicsSubstrate.openRenderPass(config)}") != std::string::npos);
  CHECK(gfxStdlib.find("drawToken{GraphicsSubstrate.drawMesh(config)}") != std::string::npos);
  CHECK(gfxStdlib.find("endToken{GraphicsSubstrate.endRenderPass(config)}") != std::string::npos);
  CHECK(gfxStdlib.find("window{Window([token] 1i32, [width] 1i32, [height] 1i32)}") != std::string::npos);
  CHECK(gfxStdlib.find("deviceToken{GraphicsSubstrate.createDevice(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("queueToken{GraphicsSubstrate.createQueue(config)?}") != std::string::npos);

  CHECK(gfxStdlib.find("[SubstrateSwapchainConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] swapchainToken{GraphicsSubstrate.createSwapchain(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[i32] vertexCount{count(vertices)}") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] indexCount{count(indices)}") == std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateMeshConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] meshToken{GraphicsSubstrate.createMesh(config)?}") == std::string::npos);
  CHECK(gfxStdlib.find("[SubstratePipelineConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] pipelineToken{GraphicsSubstrate.createPipeline(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateFrameConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] frameToken{GraphicsSubstrate.acquireFrame(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateRenderPassConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] renderPassToken{GraphicsSubstrate.openRenderPass(config)}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateDrawMeshConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] drawToken{GraphicsSubstrate.drawMesh(config)}") == std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateRenderPassEndConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] endToken{GraphicsSubstrate.endRenderPass(config)}") == std::string::npos);
  CHECK(gfxStdlib.find("[Window] window{Window([token] 1i32, [width] 1i32, [height] 1i32)}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateDeviceConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] deviceToken{GraphicsSubstrate.createDevice(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[i32] queueToken{GraphicsSubstrate.createQueue(config)?}") ==
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
  CHECK(gfxStdlib.find("return(Queue([token] this.token + 1i32))") != std::string::npos);
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
  CHECK(gfxExperimental.find("return(canonicalWindow(this).is_open())") != std::string::npos);
  CHECK(gfxExperimental.find("return(experimentalQueue(canonicalDevice(this).default_queue()))") !=
        std::string::npos);
  CHECK(gfxExperimental.find("return(experimentalRenderPass(canonicalFrame(this).render_pass(clear_color, clear_depth)))") !=
        std::string::npos);
  CHECK(gfxExperimental.find("canonicalRenderPass(this).draw_mesh(canonicalMesh(mesh), canonicalMaterial(material))") !=
        std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gfx/Buffer/readback<T>(canonicalBuffer<T>(self)))") !=
        std::string::npos);
  CHECK(gfxExperimental.find("return(experimentalBuffer<T>(/std/gfx/Buffer/upload<T>(values)))") !=
        std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gfx/GfxError/why(canonicalGfxError(err)))") !=
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

TEST_CASE("software renderer composite widgets stay source locked to basic widgets") {
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

TEST_CASE("software renderer html adapter stays source locked to shared widgets") {
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

TEST_CASE("software renderer ui event stream stays source locked to normalized helpers") {
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
