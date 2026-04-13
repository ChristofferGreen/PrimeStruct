#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("primec collect-diagnostics keeps execution wrapper method count pair arg-shape diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapMap().count(), wrapVector().count())
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_shape_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper method count pair arg-shape diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapMap().count(), wrapVector().count())
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_shape_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper method count pair missing-arg reverse diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapVector().count(), wrapMap().count())
)";
  const std::string srcPath = writeTemp(
      "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_missing_arg_shape_"
      "reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_missing_arg_shape_"
       "reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper method count pair missing-arg reverse diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapVector().count(), wrapMap().count())
)";
  const std::string srcPath = writeTemp(
      "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_missing_arg_shape_"
      "reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_missing_arg_shape_"
       "reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper method count pair arg-type reverse diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapVector().count(true), wrapMap().count(true))
)";
  const std::string srcPath = writeTemp(
      "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_type_reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_type_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /vector/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper method count pair arg-type reverse diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapVector().count(true), wrapMap().count(true))
)";
  const std::string srcPath = writeTemp(
      "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_type_reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_type_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /vector/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper method count pair mixed-shape diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapMap().count(true), wrapVector().count())
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_mixed_shape_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_mixed_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper method count pair mixed-shape diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapMap().count(true), wrapVector().count())
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_mixed_shape_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_mixed_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/count parameter") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper method count pair mixed-shape reverse diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapVector().count(), wrapMap().count(true))
)";
  const std::string srcPath = writeTemp(
      "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_mixed_shape_"
      "reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_mixed_shape_"
       "reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper method count pair mixed-shape reverse diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapVector().count(), wrapMap().count(true))
)";
  const std::string srcPath = writeTemp(
      "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_mixed_shape_"
      "reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_mixed_shape_"
       "reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper method count pair extra-arg diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapMap().count(1i32, 2i32), wrapVector().count(1i32, 2i32))
)";
  const std::string srcPath = writeTemp(
      "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_extra_arg_shape_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_extra_arg_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}


TEST_SUITE_END();
