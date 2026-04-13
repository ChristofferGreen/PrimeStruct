#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("primevm collect-diagnostics keeps user wrapper method count capacity pair mixed-shape diagnostics in definition scope") {
  const std::string source = R"(
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
  wrapMap().count(true)
  wrapVector().capacity()
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp(
      "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_method_count_capacity_pair_mixed_shape_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_method_count_capacity_pair_mixed_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE(
    "primec collect-diagnostics keeps user wrapper method count capacity pair mixed-shape reverse diagnostics in definition scope") {
  const std::string source = R"(
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
  wrapMap().count()
  wrapVector().capacity(true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_method_count_capacity_pair_"
                "mixed_shape_reverse_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_method_count_capacity_pair_"
       "mixed_shape_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE(
    "primevm collect-diagnostics keeps user wrapper method count capacity pair mixed-shape reverse diagnostics in definition scope") {
  const std::string source = R"(
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
  wrapMap().count()
  wrapVector().capacity(true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_method_count_capacity_pair_"
                "mixed_shape_reverse_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_method_count_capacity_pair_"
       "mixed_shape_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper count pair arg-type diagnostics in definition scope") {
  const std::string source = R"(
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
/vector/count([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  count(wrapMap(), true)
  wrapVector().count(true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_pair_type_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_pair_type_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper count pair arg-type diagnostics in definition scope") {
  const std::string source = R"(
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
/vector/count([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  count(wrapMap(), true)
  wrapVector().count(true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_pair_type_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_pair_type_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper count pair arg-type reverse diagnostics in definition scope") {
  const std::string source = R"(
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
/vector/count([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  wrapVector().count(true)
  count(wrapMap(), true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_pair_type_reverse_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_pair_type_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /vector/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper count pair arg-type reverse diagnostics in definition scope") {
  const std::string source = R"(
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
/vector/count([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  wrapVector().count(true)
  count(wrapMap(), true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_pair_type_reverse_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_pair_type_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /vector/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper method count pair arg-type diagnostics in definition scope") {
  const std::string source = R"(
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
/vector/count([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  wrapMap().count(true)
  wrapVector().count(true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_method_count_pair_type_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_method_count_pair_type_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper method count pair arg-type diagnostics in definition scope") {
  const std::string source = R"(
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
/vector/count([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  wrapMap().count(true)
  wrapVector().count(true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_method_count_pair_type_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_method_count_pair_type_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper count call-pair arg-type diagnostics in definition scope") {
  const std::string source = R"(
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
/vector/count([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  count(wrapMap(), true)
  count(wrapVector(), true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp(
      "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_call_pair_type_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_call_pair_type_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper count call-pair arg-type diagnostics in definition scope") {
  const std::string source = R"(
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
/vector/count([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  count(wrapMap(), true)
  count(wrapVector(), true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp(
      "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_call_pair_type_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_call_pair_type_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper count call-pair arg-type reverse diagnostics in definition scope") {
  const std::string source = R"(
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
/vector/count([vector<i32>] values, [i32] marker) {
  return(marker)
}

[return<i32>]
bad() {
  count(wrapVector(), true)
  count(wrapMap(), true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp(
      "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_call_pair_type_reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_call_pair_type_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for builtin count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 1);
}


TEST_SUITE_END();
