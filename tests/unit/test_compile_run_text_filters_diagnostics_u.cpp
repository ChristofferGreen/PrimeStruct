#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("primevm collect-diagnostics keeps execution wrapper method count pair extra-arg diagnostics") {
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
      "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_extra_arg_shape_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_extra_arg_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper method count pair extra-arg reverse diagnostics") {
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

execute_repeat(wrapVector().count(1i32, 2i32), wrapMap().count(1i32, 2i32))
)";
  const std::string srcPath = writeTemp(
      "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_extra_arg_shape_"
      "reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_extra_arg_shape_"
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

TEST_CASE("primevm collect-diagnostics keeps execution wrapper method count pair extra-arg reverse diagnostics") {
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

execute_repeat(wrapVector().count(1i32, 2i32), wrapMap().count(1i32, 2i32))
)";
  const std::string srcPath = writeTemp(
      "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_extra_arg_shape_"
      "reverse_shadow.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_method_count_pair_extra_arg_shape_"
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

TEST_CASE("primec collect-diagnostics keeps execution wrapper count pair extra-arg diagnostics") {
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

execute_repeat(count(wrapMap(), 1i32, 2i32), wrapVector().count(1i32, 2i32))
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_pair_extra_arg_shape_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_pair_extra_arg_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper count pair extra-arg diagnostics") {
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

execute_repeat(count(wrapMap(), 1i32, 2i32), wrapVector().count(1i32, 2i32))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_pair_extra_arg_shape_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_pair_extra_arg_shape_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper count pair extra-arg reverse diagnostics") {
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

execute_repeat(wrapVector().count(1i32, 2i32), count(wrapMap(), 1i32, 2i32))
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_pair_extra_arg_"
                "reverse_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_pair_extra_arg_"
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

TEST_CASE("primevm collect-diagnostics keeps execution wrapper count pair extra-arg reverse diagnostics") {
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

execute_repeat(wrapVector().count(1i32, 2i32), count(wrapMap(), 1i32, 2i32))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_pair_extra_arg_"
                "reverse_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_pair_extra_arg_"
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

TEST_CASE("primec collect-diagnostics emits intra-execution argument-type payload") {
  const std::string source = R"(
[return<i32>]
main() {
  return(0i32)
}

[return<i32>]
expects_bool([bool] cond, [i32] value) {
  return(value)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(expects_bool(1i32, 7i32), expects_bool(true, false))
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_intra_execution_argtype.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_argtype_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"");
  const size_t secondMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits intra-execution argument-type payload") {
  const std::string source = R"(
[return<i32>]
main() {
  return(0i32)
}

[return<i32>]
expects_bool([bool] cond, [i32] value) {
  return(value)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(expects_bool(1i32, 7i32), expects_bool(true, false))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_argtype.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_argtype_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"");
  const size_t secondMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits intra-execution flow-effect payload") {
  const std::string source = R"(
[return<i32>]
main() {
  return(0i32)
}

[effects(io_out) return<i32>]
apply_effects([i32] value) {
  return(value)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

[effects(io_out)] execute_repeat([effects(io_in)] apply_effects(1i32), [capabilities(io_in)] apply_effects(2i32))
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_intra_execution_flow_effect.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_flow_effect_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"execution effects must be a subset of enclosing effects on /apply_effects: io_in\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: capabilities\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find(
      "\"message\":\"execution effects must be a subset of enclosing effects on /apply_effects: io_in\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: capabilities\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits intra-execution flow-effect payload") {
  const std::string source = R"(
[return<i32>]
main() {
  return(0i32)
}

[effects(io_out) return<i32>]
apply_effects([i32] value) {
  return(value)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

[effects(io_out)] execute_repeat([effects(io_in)] apply_effects(1i32), [capabilities(io_in)] apply_effects(2i32))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_flow_effect.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_flow_effect_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"execution effects must be a subset of enclosing effects on /apply_effects: io_in\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: capabilities\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find(
      "\"message\":\"execution effects must be a subset of enclosing effects on /apply_effects: io_in\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: capabilities\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}


TEST_SUITE_END();
