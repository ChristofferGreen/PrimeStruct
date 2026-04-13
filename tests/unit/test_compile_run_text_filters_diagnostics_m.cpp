#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("primevm collect-diagnostics keeps execution wrapper at_unsafe arg-shape diagnostics") {
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
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(key)
}

[effects(heap_alloc), return<i32>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(index)
}

[return<i32>]
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapMap().at_unsafe(1i32, 2i32), at_unsafe(wrapVector()))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_unsafe_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_unsafe_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/at_unsafe\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 1);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper unsafe arg-type diagnostics") {
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
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(key)
}

[effects(heap_alloc), return<i32>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(index)
}

[return<i32>]
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(at_unsafe(wrapMap(), true), wrapVector().at_unsafe(true))
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_unsafe_type_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_unsafe_type_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/at_unsafe parameter key") !=
        std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 1);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper unsafe arg-type diagnostics") {
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
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(key)
}

[effects(heap_alloc), return<i32>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(index)
}

[return<i32>]
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(at_unsafe(wrapMap(), true), wrapVector().at_unsafe(true))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_unsafe_type_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_unsafe_type_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/at_unsafe parameter key") !=
        std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 1);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper at arg-shape diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapMap().at(1i32, 2i32), at(wrapVector()))
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_at_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_at_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/at\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 1);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper at arg-shape diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapMap().at(1i32, 2i32), at(wrapVector()))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_at_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_at_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/at\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 1);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper index arg-type diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapMap()[true], wrapVector()[true])
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_index_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_index_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/at parameter key") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 1);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper index arg-type diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapMap()[true], wrapVector()[true])
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_index_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_index_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/at parameter key") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 1);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper count capacity arg-shape diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(count(wrapMap(), 1i32), wrapVector().capacity(1i32))
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_capacity_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_capacity_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper count capacity arg-shape diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(count(wrapMap(), 1i32), wrapVector().capacity(1i32))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_capacity_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_capacity_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/count\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps execution wrapper count capacity arg-shape reverse diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapVector().capacity(1i32), count(wrapMap(), 1i32))
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_capacity_reverse_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_capacity_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector/capacity\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps execution wrapper count capacity arg-shape reverse diagnostics") {
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
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(wrapVector().capacity(1i32), count(wrapMap(), 1i32))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_capacity_reverse_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_execution_wrapper_temp_count_capacity_reverse_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector/capacity\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);
}


TEST_SUITE_END();
