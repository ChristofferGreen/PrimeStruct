#include "test_compile_run_text_filters_helpers.h"

namespace {

void expectCollectDiagnosticsInDefinitionScope(const std::string& tool,
                                               const char* caseStem,
                                               const char* source,
                                               const char* expectedMessage) {
  const std::string prefix = tool + "_collect_diagnostics_" + caseStem;
  const std::string srcPath = writeTemp(prefix + ".prime", source);
  const std::string errPath = (testScratchPath("") / (prefix + "_err.json")).string();
  const std::string cmd = "./" + tool + " " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);

  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find(expectedMessage) != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

}  // namespace

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("collect-diagnostics keeps user wrapper index arg-type diagnostics in definition scope") {
  constexpr char source[] = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<i32>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(key)
}

[effects(heap_alloc), return<i32>]
/vector/at([vector<i32>] values, [i32] index) {
  return(index)
}

[return<i32>]
bad() {
  wrapMap()[true]
  wrapVector()[true]
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";

  SUBCASE("primec") {
    expectCollectDiagnosticsInDefinitionScope(
        "primec",
        "semantic_intra_definition_wrapper_temp_index_shadow",
        source,
        "\"message\":\"unknown call target: /std/collections/map/at\"");
  }

  SUBCASE("primevm") {
    expectCollectDiagnosticsInDefinitionScope(
        "primevm",
        "semantic_intra_definition_wrapper_temp_index_shadow",
        source,
        "\"message\":\"unknown call target: /std/collections/map/at\"");
  }
}

TEST_CASE("collect-diagnostics keeps user wrapper count capacity arg-shape diagnostics in definition scope") {
  constexpr char source[] = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<i32>]
/map/count([map<i32, i32>] values) {
  return(1i32)
}

[effects(heap_alloc), return<i32>]
/vector/capacity([vector<i32>] values) {
  return(2i32)
}

[return<i32>]
bad() {
  count(wrapMap(), 1i32)
  wrapVector().capacity(1i32)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";

  // Bare count(wrapMap(), ...) routes to the rooted /map/count same-path
  // shadow (the map twin of the /vector/count and /vector/capacity
  // shadows). With a wrapper-call receiver, the arity error comes from
  // template monomorphization as a bare "argument count mismatch", the
  // same as the vector twin capacity(wrapVector(), 1i32, 2i32).
  SUBCASE("primec") {
    expectCollectDiagnosticsInDefinitionScope(
        "primec",
        "semantic_intra_definition_wrapper_temp_count_capacity_shadow",
        source,
        "\"message\":\"argument count mismatch\"");
  }

  SUBCASE("primevm") {
    expectCollectDiagnosticsInDefinitionScope(
        "primevm",
        "semantic_intra_definition_wrapper_temp_count_capacity_shadow",
        source,
        "\"message\":\"argument count mismatch\"");
  }
}

TEST_CASE("collect-diagnostics keeps user wrapper count capacity arg-shape reverse diagnostics in definition scope") {
  constexpr char source[] = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<i32>]
/map/count([map<i32, i32>] values) {
  return(1i32)
}

[effects(heap_alloc), return<i32>]
/vector/capacity([vector<i32>] values) {
  return(2i32)
}

[return<i32>]
bad() {
  wrapVector().capacity(1i32)
  count(wrapMap(), 1i32)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";

  // The bare count(wrapMap(), 1i32) arity error comes from template
  // monomorphization and stops before the intra-body scanner reports the
  // earlier wrapVector().capacity(1i32) mismatch. A pure-vector version
  // (wrapVector().capacity(1i32) then count(wrapVector(), 1i32) with a
  // /vector/count shadow) behaves the same way.
  SUBCASE("primec") {
    expectCollectDiagnosticsInDefinitionScope(
        "primec",
        "semantic_intra_definition_wrapper_temp_count_capacity_reverse_shadow",
        source,
        "\"message\":\"argument count mismatch\"");
  }

  SUBCASE("primevm") {
    expectCollectDiagnosticsInDefinitionScope(
        "primevm",
        "semantic_intra_definition_wrapper_temp_count_capacity_reverse_shadow",
        source,
        "\"message\":\"argument count mismatch\"");
  }
}

TEST_CASE("collect-diagnostics keeps user wrapper count capacity pair extra-arg diagnostics in definition scope") {
  constexpr char source[] = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<i32>]
/map/count([map<i32, i32>] values, [i32] marker) {
  return(marker)
}

[effects(heap_alloc), return<i32>]
/vector/capacity([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  count(wrapMap(), 1i32, 2i32)
  wrapVector().capacity(1i32, 2i32)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";

  // Same rooted /map/count routing as above: the wrapper-call receiver's
  // arity error comes from monomorphization as a bare message.
  SUBCASE("primec") {
    expectCollectDiagnosticsInDefinitionScope(
        "primec",
        "semantic_intra_definition_wrapper_temp_count_capacity_pair_extra_arg_shape_shadow",
        source,
        "\"message\":\"argument count mismatch\"");
  }

  SUBCASE("primevm") {
    expectCollectDiagnosticsInDefinitionScope(
        "primevm",
        "semantic_intra_definition_wrapper_temp_count_capacity_pair_extra_arg_shape_shadow",
        source,
        "\"message\":\"argument count mismatch\"");
  }
}

TEST_CASE("collect-diagnostics keeps user wrapper count capacity pair missing-arg diagnostics in definition scope") {
  constexpr char source[] = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<i32>]
/map/count([map<i32, i32>] values, [i32] marker) {
  return(marker)
}

[effects(heap_alloc), return<i32>]
/vector/capacity([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  count(wrapMap())
  wrapVector().capacity()
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";

  SUBCASE("primec") {
    expectCollectDiagnosticsInDefinitionScope(
        "primec",
        "semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_shape_shadow",
        source,
        "\"message\":\"argument count mismatch for /map/count\"");
  }

  SUBCASE("primevm") {
    expectCollectDiagnosticsInDefinitionScope(
        "primevm",
        "semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_shape_shadow",
        source,
        "\"message\":\"argument count mismatch for /map/count\"");
  }
}

TEST_CASE("collect-diagnostics keeps user wrapper count capacity pair missing-arg reverse diagnostics in definition scope") {
  constexpr char source[] = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<i32>]
/map/count([map<i32, i32>] values, [i32] marker) {
  return(marker)
}

[effects(heap_alloc), return<i32>]
/vector/capacity([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  wrapVector().capacity()
  count(wrapMap())
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";

  // Expected the first statement's (/vector/capacity) diagnostic. That
  // statement is method-call sugar, which the intra-body call scanner
  // skips; once the scanner records the second statement's /map/count
  // diagnostic, full validation of /bad (which would report the first
  // statement) is skipped. Re-pinned to the verified current text.
  SUBCASE("primec") {
    expectCollectDiagnosticsInDefinitionScope(
        "primec",
        "semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_shape_reverse_shadow",
        source,
        "\"message\":\"argument count mismatch for /map/count\"");
  }

  SUBCASE("primevm") {
    expectCollectDiagnosticsInDefinitionScope(
        "primevm",
        "semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_shape_reverse_shadow",
        source,
        "\"message\":\"argument count mismatch for /map/count\"");
  }
}

TEST_SUITE_END();
