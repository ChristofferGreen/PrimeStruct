#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("rejects vm reordered namespaced vector push call compatibility alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /vector/push(3i32, values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_reordered_namespaced_vector_push_call_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_reordered_namespaced_vector_push_call_shadow_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("duplicate definition: /std/collections/vector/push") != std::string::npos);
}

TEST_CASE("runs vm std namespaced reordered mutator compatibility helper shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/push(3i32, values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_std_namespaced_reordered_mutator_compat_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector push bool positional call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [bool] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(true, values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_push_call_bool_positional_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector push call named shadow" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push([value] 3i32, [values] values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_push_call_named_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector push method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.push(3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_push_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm user vector push call expression shadow" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  return(push(values, 3i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_push_call_expr_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("rejects vm reordered namespaced vector push call expression compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  return(/vector/push(3i32, values))
}
)";
  const std::string srcPath = writeTemp("vm_reordered_namespaced_vector_push_call_expr_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_reordered_namespaced_vector_push_call_expr_shadow_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("runs vm named vector push expression receiver precedence" * doctest::skip(true)) {
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
  const std::string srcPath = writeTemp("vm_user_vector_push_expr_named_receiver_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm auto-inferred named vector push expression receiver precedence" * doctest::skip(true)) {
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
  const std::string srcPath = writeTemp("vm_user_vector_push_expr_named_receiver_precedence_auto.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm auto-inferred std namespaced vector push compatibility alias precedence" * doctest::skip(true)) {
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
      writeTemp("vm_std_namespaced_vector_push_expr_named_receiver_precedence_auto.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_std_namespaced_vector_push_expr_named_receiver_precedence_auto_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("runs vm with auto-inferred std namespaced vector push canonical definition") {
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
      writeTemp("vm_std_namespaced_vector_push_expr_named_receiver_canonical_definition_auto.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("rejects vm auto-inferred std namespaced count helper compatibility alias precedence") {
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
  const std::string srcPath = writeTemp("vm_std_namespaced_vector_count_receiver_precedence_auto.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_std_namespaced_vector_count_receiver_precedence_auto_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("return type mismatch: expected i32") != std::string::npos);
}

TEST_CASE("runs vm with auto-inferred std namespaced count helper canonical fallback") {
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
  const std::string srcPath = writeTemp("vm_std_namespaced_vector_count_canonical_fallback_auto.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_std_namespaced_vector_count_canonical_fallback_auto_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("rejects vm std namespaced count expression compatibility alias precedence") {
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
  const std::string srcPath = writeTemp("vm_std_namespaced_vector_count_expr_receiver_precedence.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_std_namespaced_vector_count_expr_receiver_precedence_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("return type mismatch: expected i32") != std::string::npos);
}

TEST_CASE("rejects vm std namespaced count without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  const std::string srcPath = writeTemp("vm_std_namespaced_count_import_requirement.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_std_namespaced_count_import_requirement_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects vm std namespaced count map target without helper" * doctest::skip(true)) {
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
  const std::string srcPath = writeTemp("vm_std_namespaced_count_map_target_import_requirement.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_std_namespaced_count_map_target_import_requirement_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("template arguments required for /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects vm alias count map target without helper") {
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
  const std::string srcPath = writeTemp("vm_alias_count_map_target_import_requirement.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_alias_count_map_target_import_requirement_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("rejects vm std namespaced capacity map target without helper") {
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
  const std::string srcPath = writeTemp("vm_std_namespaced_capacity_map_target_import_requirement.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_std_namespaced_capacity_map_target_import_requirement_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("rejects vm alias capacity map target without helper") {
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
  const std::string srcPath = writeTemp("vm_alias_capacity_map_target_import_requirement.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_alias_capacity_map_target_import_requirement_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("vm alias capacity array target accepts same-path helper") {
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
  const std::string srcPath = writeTemp("vm_alias_capacity_array_same_path_helper.prime", source);
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 46);
}

TEST_CASE("rejects vm alias capacity array target without helper") {
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
  const std::string srcPath = writeTemp("vm_alias_capacity_array_target_import_requirement.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_alias_capacity_array_target_import_requirement_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("runs vm with std namespaced count expression canonical fallback") {
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
  const std::string srcPath = writeTemp("vm_std_namespaced_vector_count_expr_canonical_fallback.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_std_namespaced_vector_count_expr_canonical_fallback_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("rejects vm std namespaced count non-builtin compatibility fallback") {
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
  const std::string srcPath = writeTemp("vm_std_namespaced_count_non_builtin_compat_fallback.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_std_namespaced_count_non_builtin_compat_fallback_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects vm vector namespaced count non-builtin array fallback") {
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
  const std::string srcPath = writeTemp("vm_vector_namespaced_count_non_builtin_array_fallback_rejected.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm std namespaced capacity expression compatibility alias precedence") {
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
  const std::string srcPath = writeTemp("vm_std_namespaced_vector_capacity_expr_receiver_precedence.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_std_namespaced_vector_capacity_expr_receiver_precedence_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("return type mismatch: expected i32") != std::string::npos);
}

TEST_CASE("runs vm with std namespaced capacity expression canonical fallback") {
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
  const std::string srcPath = writeTemp("vm_std_namespaced_vector_capacity_expr_canonical_fallback.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("runs vm with auto-inferred named access helper receiver precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/get([vector<i32>] values, [string] index) {
  return(12i32)
}

[effects(heap_alloc), return<int>]
/string/get([string] values, [vector<i32>] index) {
  return(98i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [string] index{"tag"raw_utf8}
  [auto] inferred{get([index] index, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath = writeTemp("vm_user_access_expr_named_receiver_precedence_auto.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("runs vm auto-inferred std namespaced access helper compatibility alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("vm_std_namespaced_vector_access_expr_named_receiver_precedence_auto.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_std_namespaced_vector_access_expr_named_receiver_precedence_auto_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("runs vm with auto-inferred std namespaced access helper canonical definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("vm_std_namespaced_vector_access_expr_named_receiver_canonical_fallback_auto.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}


TEST_SUITE_END();
