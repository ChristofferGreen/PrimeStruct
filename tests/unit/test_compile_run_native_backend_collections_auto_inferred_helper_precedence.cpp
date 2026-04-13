#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles and runs native named vector push expression receiver precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
/string/push([vector<i32> mut] values, [string] value) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [string] payload{"tag"raw_utf8}
  return(push([value] payload, [values] values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_vector_push_expr_named_receiver_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_user_vector_push_expr_named_receiver_precedence_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs native auto-inferred named vector push expression receiver precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
/string/push([vector<i32> mut] values, [string] value) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{push([value] payload, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_vector_push_expr_named_receiver_precedence_auto.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_user_vector_push_expr_named_receiver_precedence_auto_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("rejects native auto-inferred std namespaced vector push compatibility alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push([value] payload, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_vector_push_expr_named_receiver_precedence_auto.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_push_expr_named_receiver_precedence_auto_exe")
          .string();

  const std::string outPath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_push_expr_named_receiver_precedence_auto_out.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("return type mismatch: expected int") != std::string::npos);
}

TEST_CASE("compiles and runs native auto-inferred std namespaced vector push canonical definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push([value] payload, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_vector_push_expr_named_receiver_canonical_definition_auto.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_push_expr_named_receiver_canonical_definition_auto_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects native auto-inferred std namespaced count helper compatibility alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
)";
  const std::string srcPath = writeTemp("compile_native_std_namespaced_vector_count_receiver_precedence_auto.prime",
                                        source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_count_receiver_precedence_auto_exe")
          .string();

  const std::string outPath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_count_receiver_precedence_auto_out.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("return type mismatch: expected int") != std::string::npos);
}

TEST_CASE("compiles native auto-inferred std namespaced count helper canonical fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
)";
  const std::string srcPath = writeTemp("compile_native_std_namespaced_vector_count_canonical_fallback_auto.prime",
                                        source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_count_canonical_fallback_auto_exe")
          .string();
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_count_canonical_fallback_auto_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects native std namespaced count expression compatibility alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_std_namespaced_vector_count_expr_receiver_precedence.prime",
                                        source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_count_expr_receiver_precedence_exe")
          .string();

  const std::string outPath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_count_expr_receiver_precedence_out.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("return type mismatch: expected int") != std::string::npos);
}

TEST_CASE("rejects native std namespaced count without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_count_import_requirement.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_std_namespaced_count_import_requirement_exe")
                                  .string();
  const std::string outPath = (testScratchPath("") /
                               "primec_native_std_namespaced_count_import_requirement_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects native std namespaced count map target without helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_count_map_target_import_requirement.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_std_namespaced_count_map_target_import_requirement_exe")
                                  .string();
  const std::string outPath = (testScratchPath("") /
                               "primec_native_std_namespaced_count_map_target_import_requirement_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects native alias count map target without helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_alias_count_map_target_import_requirement.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_alias_count_map_target_import_requirement_exe")
                                  .string();
  const std::string outPath = (testScratchPath("") /
                               "primec_native_alias_count_map_target_import_requirement_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("rejects native std namespaced capacity map target without helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/capacity(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_capacity_map_target_import_requirement.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_std_namespaced_capacity_map_target_import_requirement_exe")
                                  .string();
  const std::string outPath = (testScratchPath("") /
                               "primec_native_std_namespaced_capacity_map_target_import_requirement_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("rejects native alias capacity map target without helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/capacity(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_alias_capacity_map_target_import_requirement.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_alias_capacity_map_target_import_requirement_exe")
                                  .string();
  const std::string outPath = (testScratchPath("") /
                               "primec_native_alias_capacity_map_target_import_requirement_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("native alias capacity array target accepts same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/capacity([array<i32>] values) {
  return(46i32)
}

[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/capacity(wrapArray()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_alias_capacity_array_same_path_helper.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_alias_capacity_array_same_path_helper_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 46);
}

TEST_CASE("rejects native alias capacity array target without helper") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/capacity(wrapArray()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_alias_capacity_array_target_import_requirement.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_alias_capacity_array_target_import_requirement_exe")
                                  .string();
  const std::string outPath = (testScratchPath("") /
                               "primec_native_alias_capacity_array_target_import_requirement_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("compiles native std namespaced count expression canonical fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_std_namespaced_vector_count_expr_canonical_fallback.prime",
                                        source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_count_expr_canonical_fallback_exe")
          .string();
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_count_expr_canonical_fallback_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects native std namespaced count non-builtin compatibility fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_count_non_builtin_compat_fallback.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_std_namespaced_count_non_builtin_compat_fallback_exe")
                                  .string();
  const std::string outPath = (testScratchPath("") /
                               "primec_native_std_namespaced_count_non_builtin_compat_fallback_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects native vector namespaced count non-builtin array fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(31i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_vector_namespaced_count_non_builtin_array_fallback_rejected.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_vector_namespaced_count_non_builtin_array_fallback_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native std namespaced capacity expression compatibility alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_vector_capacity_expr_receiver_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_capacity_expr_receiver_precedence_exe")
          .string();

  const std::string outPath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_capacity_expr_receiver_precedence_out.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("return type mismatch: expected int") != std::string::npos);
}

TEST_CASE("compiles and runs native std namespaced capacity expression canonical fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_vector_capacity_expr_canonical_fallback.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_capacity_expr_canonical_fallback_exe")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}


TEST_SUITE_END();
#endif
