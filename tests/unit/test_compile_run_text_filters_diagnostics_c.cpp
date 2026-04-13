#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("primec collect-diagnostics keeps user wrapper index arg-type diagnostics in definition scope") {
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
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_index_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_index_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/at parameter key") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper index arg-type diagnostics in definition scope") {
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
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_index_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_index_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/at parameter key") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper count capacity arg-shape diagnostics in definition scope") {
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
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper count capacity arg-shape diagnostics in definition scope") {
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
  const std::string srcPath = writeTemp(
      "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper count capacity arg-shape reverse diagnostics in definition scope") {
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
  const std::string srcPath = writeTemp(
      "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector/capacity\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper count capacity arg-shape reverse diagnostics in definition scope") {
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
  const std::string srcPath = writeTemp(
      "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector/capacity\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper count capacity pair extra-arg diagnostics in definition scope") {
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
  count(wrapMap(), 1i32, 2i32)
  wrapVector().capacity(1i32, 2i32)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_extra_arg_shape_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_extra_arg_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper count capacity pair extra-arg diagnostics in definition scope") {
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
  count(wrapMap(), 1i32, 2i32)
  wrapVector().capacity(1i32, 2i32)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_extra_arg_shape_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_extra_arg_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper count capacity pair missing-arg diagnostics in definition scope") {
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
  count(wrapMap())
  wrapVector().capacity()
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_shape_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper count capacity pair missing-arg diagnostics in definition scope") {
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
  count(wrapMap())
  wrapVector().capacity()
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_shape_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE(
    "primec collect-diagnostics keeps user wrapper count capacity pair missing-arg reverse diagnostics in definition scope") {
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
  wrapVector().capacity()
  count(wrapMap())
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp(
      "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_"
      "shape_reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_"
       "shape_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector/capacity\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE(
    "primevm collect-diagnostics keeps user wrapper count capacity pair missing-arg reverse diagnostics in definition scope") {
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
  wrapVector().capacity()
  count(wrapMap())
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp(
      "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_"
      "shape_reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_count_capacity_pair_missing_arg_"
       "shape_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector/capacity\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}


TEST_SUITE_END();
