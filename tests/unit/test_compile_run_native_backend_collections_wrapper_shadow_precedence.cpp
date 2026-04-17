#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles and runs native user wrapper temporary at_unsafe shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(73i32)
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(74i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(at_unsafe(wrapMap(), 1i32), wrapMap().at_unsafe(1i32)),
              plus(at_unsafe(wrapVector(), 0i32), wrapVector().at_unsafe(0i32))))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_wrapper_temp_at_unsafe_shadow_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_wrapper_temp_at_unsafe_shadow_precedence_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 81);
}

TEST_CASE("rejects native user wrapper temporary unsafe parity shadow mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(73i32)
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(74i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(
      plus(at_unsafe(wrapMap(), true), wrapMap().at_unsafe(true)),
      plus(at_unsafe(wrapVector(), true), wrapVector().at_unsafe(true))))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_wrapper_temp_unsafe_parity_shadow_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native user wrapper temporary unsafe parity shadow value mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<bool>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{at_unsafe(wrapMap(), 1i32)}
  [i32] mapMethod{wrapMap().at_unsafe(1i32)}
  [i32] vectorCall{at_unsafe(wrapVector(), 0i32)}
  [i32] vectorMethod{wrapVector().at_unsafe(0i32)}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_wrapper_temp_unsafe_parity_shadow_value_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native user wrapper temporary unsafe parity shadow arity mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(73i32)
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(74i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(
      plus(at_unsafe(wrapMap(), 1i32, 2i32), wrapMap().at_unsafe(1i32, 2i32)),
      plus(at_unsafe(wrapVector(), 0i32, 1i32), wrapVector().at_unsafe(0i32, 1i32))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_wrapper_temp_unsafe_parity_shadow_arity_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native user wrapper temporary unsafe parity shadow missing arguments") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(73i32)
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(74i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(
      plus(at_unsafe(wrapMap()), wrapMap().at_unsafe()),
      plus(at_unsafe(wrapVector()), wrapVector().at_unsafe())))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_wrapper_temp_unsafe_parity_shadow_missing_arguments.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native user wrapper temporary at shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(75i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(76i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(at(wrapMap(), 1i32), wrapMap().at(1i32)),
              plus(at(wrapVector(), 0i32), wrapVector().at(0i32))))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_wrapper_temp_at_shadow_precedence.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_user_wrapper_temp_at_shadow_precedence.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > " + quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("compiles and runs native user wrapper temporary count capacity shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/count([map<i32, i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(78i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(79i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(count(wrapMap()), wrapMap().count()),
              plus(plus(count(wrapVector()), wrapVector().count()),
                   plus(capacity(wrapVector()), wrapVector().capacity()))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_wrapper_temp_count_capacity_shadow_precedence.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_user_wrapper_temp_count_capacity_shadow_precedence.err").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > " + quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("rejects native user wrapper temporary count capacity shadow value mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<bool>]
/map/count([map<i32, i32>] values) {
  return(true)
}

[effects(heap_alloc), return<bool>]
/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
/vector/capacity([vector<i32>] values) {
  return(true)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{count(wrapMap())}
  [i32] mapMethod{wrapMap().count()}
  [i32] vectorCountCall{count(wrapVector())}
  [i32] vectorCountMethod{wrapVector().count()}
  [i32] vectorCapacityCall{capacity(wrapVector())}
  [i32] vectorCapacityMethod{wrapVector().capacity()}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_wrapper_temp_count_capacity_shadow_value_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native user wrapper temporary index shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(81i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(82i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(wrapMap()[1i32], wrapVector()[0i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_wrapper_temp_index_shadow_precedence.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_user_wrapper_temp_index_shadow_precedence.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > " + quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("compiles and runs native user wrapper temporary syntax parity shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(83i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(84i32)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{at(wrapMap(), 1i32)}
  [i32] mapMethod{wrapMap().at(1i32)}
  [i32] mapIndex{wrapMap()[1i32]}
  [i32] vectorCall{at(wrapVector(), 0i32)}
  [i32] vectorMethod{wrapVector().at(0i32)}
  [i32] vectorIndex{wrapVector()[0i32]}
  return(plus(plus(plus(mapCall, mapMethod), mapIndex), plus(plus(vectorCall, vectorMethod), vectorIndex)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_wrapper_temp_syntax_parity_shadow_precedence.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_user_wrapper_temp_syntax_parity_shadow_precedence.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > " + quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("rejects native user wrapper temporary syntax parity shadow mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(83i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(84i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(
      plus(plus(at(wrapMap(), true), wrapMap().at(true)), wrapMap()[true]),
      plus(plus(at(wrapVector(), true), wrapVector().at(true)), wrapVector()[true])))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_wrapper_temp_syntax_parity_shadow_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native user wrapper temporary syntax parity shadow value mismatch") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<bool>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{at(wrapMap(), 1i32)}
  [i32] mapMethod{wrapMap().at(1i32)}
  [i32] mapIndex{wrapMap()[1i32]}
  [i32] vectorCall{at(wrapVector(), 0i32)}
  [i32] vectorMethod{wrapVector().at(0i32)}
  [i32] vectorIndex{wrapVector()[0i32]}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_wrapper_temp_syntax_parity_shadow_value_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}


TEST_SUITE_END();
#endif
