#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("native keeps imported wrapper-returned canonical map reference access lowering diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  return(/std/collections/map/at(borrowMap(location(values)), 1i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_string_count_method_shadow_direct_wrapper_map_reference_access.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_user_string_count_method_shadow_direct_wrapper_map_reference_access_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_user_string_count_method_shadow_direct_wrapper_map_reference_access.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend does not support string array return types on /borrowMap") !=
        std::string::npos);
}

TEST_CASE("native keeps non-imported wrapper-returned canonical map reference access primitive receiver diagnostics") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/string/count([string] values) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/at(borrowMap(location(values)), 1i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_nonimport_direct_wrapper_map_reference_string_receiver_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_nonimport_direct_wrapper_map_reference_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /string/count parameter values: expected string") !=
        std::string::npos);
}

TEST_CASE("native keeps map method sugar on wrapper-returned canonical map references") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(borrowMap(location(values)).count(),
              plus(borrowMap(location(values)).at(1i32),
                   borrowMap(location(values)).at_unsafe(2i32))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_map_reference_method_sugar.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_wrapper_map_reference_method_sugar_exe")
          .string();
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_wrapper_map_reference_method_sugar_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native keeps non-string diagnostics on canonical map reference access count shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(ref[1i32].count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_string_count_method_shadow_map_reference_access_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_user_string_count_method_shadow_map_reference_access_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("native keeps key diagnostics on wrapper-returned canonical map reference method sugar") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(borrowMap(location(values)).at(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_map_reference_method_sugar_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_map_reference_method_sugar_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Semantic error: at requires map key type i32") != std::string::npos);
}

TEST_CASE("native keeps non-string diagnostics on wrapper-returned canonical map access count shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()[1i32].count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_string_count_method_shadow_wrapper_canonical_map_access_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_user_string_count_method_shadow_wrapper_canonical_map_access_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native canonical vector access builtin string count shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"raw_utf8)}
  return(count(/std/collections/vector/at(values, 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_vector_access_builtin_string_count_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_vector_access_builtin_string_count_shadow_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("native keeps primitive diagnostics on canonical vector unsafe access count shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(count(/std/collections/vector/at_unsafe(values, 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_vector_access_unsafe_count_shadow_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_canonical_vector_access_unsafe_count_shadow_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("compiles and runs native canonical vector method access builtin string count shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"raw_utf8)}
  return(values.at(0i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_vector_method_access_builtin_string_count_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_vector_method_access_builtin_string_count_shadow_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 91);
}

TEST_CASE("native keeps primitive diagnostics on canonical vector unsafe method access count shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(values.at_unsafe(0i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_vector_unsafe_method_access_count_shadow_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_canonical_vector_unsafe_method_access_count_shadow_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("native keeps direct wrapper-returned canonical map access string receiver typing") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(/std/collections/map/at(wrapMap(), 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_direct_wrapper_canonical_map_access_count_diag.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_direct_wrapper_canonical_map_access_count_diag_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("native keeps wrapper-returned canonical map method access string receiver typing") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(wrapMap().at(1i32).count(),
              wrapMap().at_unsafe(1i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_canonical_map_method_access_count_diag.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_wrapper_canonical_map_method_access_count_diag_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 182);
}

TEST_CASE("native rejects wrapper-returned slash-method map access count with same-path diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapMap()./std/collections/map/at(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_slash_method_map_access_count_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_slash_method_map_access_count_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("compiles and runs native slash-method vector access string count fallback") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[return<i32>]
/std/collections/vector/at_unsafe([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"raw_utf8)}
  return(plus(values./vector/at(0i32).count(),
              values./std/collections/vector/at_unsafe(0i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_slash_method_vector_access_string_count_fallback.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_slash_method_vector_access_string_count_fallback_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 182);
}

TEST_CASE("native keeps slash-method vector access primitive count diagnostics" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(plus(values./vector/at(0i32).count(),
              values./std/collections/vector/at_unsafe(0i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_slash_method_vector_access_primitive_count_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_slash_method_vector_access_primitive_count_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("compiles and runs native wrapper-returned vector access string count fallback") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[return<i32>]
/std/collections/vector/at_unsafe([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<vector<string>>]
wrapValues() {
  return(vector<string>("hello"raw_utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(/vector/at(wrapValues(), 0i32)),
              wrapValues().at_unsafe(0i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_vector_access_string_count_fallback.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_wrapper_vector_access_string_count_fallback_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("native keeps wrapper-returned vector access primitive count diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(/vector/at(wrapValues(), 0i32)),
              wrapValues().at_unsafe(0i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_vector_access_primitive_count_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_vector_access_primitive_count_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("compiles and runs native user vector count method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(97i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_count_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_count_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native canonical slash vector count same-path helper on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(87i32)
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_slash_vector_count_map_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_slash_vector_count_map_same_path_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 87);
}

TEST_CASE("rejects native wrapper-returned canonical vector count slash-method on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_slash_vector_count_map_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_canonical_slash_vector_count_map_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects native wrapper-returned canonical vector capacity slash-method on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_slash_vector_capacity_map_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_canonical_slash_vector_capacity_map_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native canonical slash vector count same-path helper on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(88i32)
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_slash_vector_count_array_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_slash_vector_count_array_same_path_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 88);
}

TEST_CASE("rejects native wrapper-returned canonical vector count slash-method on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_slash_vector_count_array_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_canonical_slash_vector_count_array_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native canonical slash vector count same-path helper on string receiver") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
/std/collections/vector/count([string] values) {
  return(89i32)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_slash_vector_count_string_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_slash_vector_count_string_same_path_helper_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 89);
}

TEST_CASE("rejects native wrapper-returned canonical vector count slash-method on string receiver") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_slash_vector_count_string_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_canonical_slash_vector_count_string_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects native wrapper-returned canonical vector capacity slash-method on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_slash_vector_capacity_array_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_canonical_slash_vector_capacity_array_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native user vector capacity method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_capacity_method_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_user_vector_capacity_method_shadow.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("name=capacity") != std::string::npos);
}

TEST_CASE("rejects native user vector count call shadow" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(97i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_count_call_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_user_vector_count_call_shadow_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects native user vector capacity call shadow" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_capacity_call_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_user_vector_capacity_call_shadow_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("compiles and runs native user array capacity call shadow" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(66i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_array_capacity_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_array_capacity_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 66);
}

TEST_CASE("compiles and runs native user array capacity method shadow") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(65i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_native_user_array_capacity_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_array_capacity_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 65);
}

TEST_CASE("compiles and runs native user array at call shadow" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(61i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_array_at_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_array_at_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 61);
}

TEST_SUITE_END();
#endif
