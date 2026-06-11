#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("primevm collect-diagnostics emits intra-definition multi-semantic payload") {
  const std::string source = R"(
[return<int>]
bad() {
  nope(1i32)
  missing(2i32)
  return(0i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_intra_definition.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_collect_diagnostics_semantic_intra_definition_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown call target: nope\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits intra-definition argument-shape payload") {
  const std::string source = R"(
[return<i32>]
take_two([i32] a, [i32] b) {
  return(a)
}
[return<i32>]
bad() {
  take_two(a=1i32, a=2i32)
  take_two(1i32)
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_intra_definition_argshape.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_argshape_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /take_two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics emits intra-definition argument-shape payload") {
  const std::string source = R"(
[return<i32>]
take_two([i32] a, [i32] b) {
  return(a)
}
[return<i32>]
bad() {
  take_two(a=1i32, a=2i32)
  take_two(1i32)
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_argshape.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_argshape_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /take_two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user vector arg-shape diagnostics") {
  const std::string source = R"(
[return<i32>]
vector([i32] value) {
  return(value)
}
[return<i32>]
bad() {
  vector(value=1i32, value=2i32)
  vector()
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_vector_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_vector_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user vector arg-shape diagnostics") {
  const std::string source = R"(
[return<i32>]
vector([i32] value) {
  return(value)
}
[return<i32>]
bad() {
  vector(value=1i32, value=2i32)
  vector()
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_vector_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_vector_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /vector\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user array arg-shape diagnostics") {
  const std::string source = R"(
[return<i32>]
array([i32] value) {
  return(value)
}
[return<i32>]
bad() {
  array(value=1i32, value=2i32)
  array()
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_array_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_array_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /array\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user array arg-shape diagnostics") {
  const std::string source = R"(
[return<i32>]
array([i32] value) {
  return(value)
}
[return<i32>]
bad() {
  array(value=1i32, value=2i32)
  array()
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_array_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_array_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /array\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user map arg-shape diagnostics") {
  const std::string source = R"(
[return<i32>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}
[return<i32>]
bad() {
  map(key=1i32, key=2i32)
  map()
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_map_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_map_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"assign target must be a mutable binding: key\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user map arg-shape diagnostics") {
  const std::string source = R"(
[return<i32>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}
[return<i32>]
bad() {
  map(key=1i32, key=2i32)
  map()
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_map_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_map_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"assign target must be a mutable binding: key\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper at_unsafe arg-shape diagnostics in definition scope") {
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
bad() {
  at_unsafe(wrapMap(), 1i32, 2i32)
  wrapVector().at_unsafe()
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_unsafe_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_unsafe_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/at_unsafe\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper at_unsafe arg-shape diagnostics in definition scope") {
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
bad() {
  at_unsafe(wrapMap(), 1i32, 2i32)
  wrapVector().at_unsafe()
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_unsafe_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_unsafe_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/at_unsafe\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper unsafe arg-type diagnostics in definition scope") {
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
bad() {
  at_unsafe(wrapMap(), true)
  wrapVector().at_unsafe(true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_unsafe_type_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_unsafe_type_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/at_unsafe parameter key") !=
        std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper unsafe arg-type diagnostics in definition scope") {
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
bad() {
  at_unsafe(wrapMap(), true)
  wrapVector().at_unsafe(true)
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_unsafe_type_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_unsafe_type_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /map/at_unsafe parameter key") !=
        std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics keeps user wrapper at arg-shape diagnostics in definition scope") {
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
  at(wrapMap(), 1i32, 2i32)
  wrapVector().at()
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_at_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_collect_diagnostics_semantic_intra_definition_wrapper_temp_at_shadow_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/at\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}

TEST_CASE("primevm collect-diagnostics keeps user wrapper at arg-shape diagnostics in definition scope") {
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
  at(wrapMap(), 1i32, 2i32)
  wrapVector().at()
  return(0i32)
}

[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_at_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primevm_collect_diagnostics_semantic_intra_definition_wrapper_temp_at_shadow_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /map/at\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);
}


TEST_SUITE_END();
