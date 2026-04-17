#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("runs vm bare map at_unsafe through canonical helper" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_map_at_unsafe_with_canonical_helper.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_bare_map_at_unsafe_with_canonical_helper_out.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 17);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("rejects vm bare map at call without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_map_at_without_helper_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_map_at_without_helper_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("rejects vm bare map at_unsafe call without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_map_at_unsafe_without_helper_reject.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_bare_map_at_unsafe_without_helper_reject_out.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects vm map namespaced count method compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/count())
}
)";
  const std::string srcPath = writeTemp("vm_map_namespaced_count_method_compatibility_alias_reject.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_map_namespaced_count_method_compatibility_alias_reject_out.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("Semantic error") != std::string::npos);
}

TEST_CASE("rejects vm bare map count method without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_bare_map_count_method_without_import.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_map_count_method_without_import_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("compiles and runs vm bare map contains through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(contains(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_map_contains_call_with_canonical_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_map_contains_call_with_canonical_helper_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("rejects vm bare map contains call without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(contains(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_map_contains_call_without_import.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_map_contains_call_without_import_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("rejects vm bare map contains method without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.contains(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_map_contains_method_without_import.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_map_contains_method_without_import_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("rejects vm bare map tryAt method without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(values.tryAt(1i32)))
}
)";
  const std::string srcPath = writeTemp("vm_bare_map_tryat_method_without_import.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_map_tryat_method_without_import_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("rejects vm bare map access methods without imported canonical helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(values.at(1i32), values.at_unsafe(2i32)))
}
)";
  const std::string srcPath = writeTemp("vm_bare_map_access_methods_without_import.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_map_access_methods_without_import_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("rejects vm map namespaced at method compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_map_namespaced_at_method_compatibility_alias_reject.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_map_namespaced_at_method_compatibility_alias_reject_out.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("Semantic error") != std::string::npos);
}

TEST_CASE("rejects vm array namespaced vector capacity alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/array/capacity(values))
}
)";
  const std::string srcPath = writeTemp("vm_array_namespaced_vector_capacity_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_array_namespaced_vector_capacity_alias_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects vm array namespaced wrapper vector capacity alias") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(4i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/capacity(wrapVector()))
}
)";
  const std::string srcPath = writeTemp("vm_array_namespaced_wrapper_vector_capacity_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_array_namespaced_wrapper_vector_capacity_alias_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects vm array namespaced wrapper vector count alias") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(4i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/count(wrapVector()))
}
)";
  const std::string srcPath = writeTemp("vm_array_namespaced_wrapper_vector_count_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_array_namespaced_wrapper_vector_count_alias_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("rejects vm array namespaced vector mutator alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  /array/push(values, 6i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_array_namespaced_vector_mutator_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_array_namespaced_vector_mutator_alias_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/push") != std::string::npos);
}

TEST_CASE("rejects vm stdlib canonical vector helper method-precedence forwarding") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [bool] index) {
  return(40i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count(true), values.at(true)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_vector_method_helper_precedence_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_vector_method_helper_precedence_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("rejects vm templated stdlib canonical vector helper method template args") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count<i32>(true), values.at<i32>(2i32)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_vector_template_method_helper_precedence.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_vector_template_method_helper_precedence_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("rejects vm vector namespaced call aliases without alias definitions") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values), /vector/at(values, 2i32)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_namespaced_call_alias_canonical_precedence.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_namespaced_call_alias_canonical_precedence_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("rejects vm vector namespaced templated canonical helper alias call without alias definition") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count<i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("vm_vector_namespaced_templated_canonical_alias_call.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_vector_namespaced_templated_canonical_alias_call_out.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("rejects vm vector alias arity-mismatch compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, true), values.count(true)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_arity_mismatch_compatibility_template_forwarding_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_vector_alias_arity_mismatch_compatibility_template_forwarding_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("argument count mismatch") != std::string::npos);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding on bool type mismatch" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [i32] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, true), values.count(true)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_implicit_canonical_forwarding_bool_type_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding on non-bool type mismatch") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [string] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, 1i32), values.count(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_implicit_canonical_forwarding_non_bool_type_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding on struct type mismatch") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [MarkerB] marker{MarkerB()}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_implicit_canonical_forwarding_struct_type_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding on constructor temporary struct mismatch") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, MarkerB()), values.count(MarkerB())))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_canonical_forwarding_constructor_struct_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding on method-call temporary struct mismatch") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
Holder() {}

[return<MarkerB>]
/Holder/fetch([Holder] self) {
  return(MarkerB())
}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [Holder] holder{Holder()}
  return(plus(/vector/count(values, holder.fetch()), values.count(holder.fetch())))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_canonical_forwarding_method_struct_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding on chained method-call temporary struct mismatch") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
Middle() {}
Holder() {}

[return<Middle>]
/Holder/first([Holder] self) {
  return(Middle())
}

[return<MarkerB>]
/Middle/next([Middle] self) {
  return(MarkerB())
}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [Holder] holder{Holder()}
  return(plus(/vector/count(values, holder.first().next()),
              values.count(holder.first().next())))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_canonical_forwarding_chained_method_struct_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding on array envelope element mismatch") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [array<i32>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [array<i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [array<i64>] marker{array<i64>(1i64)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_canonical_forwarding_array_envelope_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding on map envelope mismatch") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [map<i32, string>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [map<i32, i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [map<i32, i64>] marker{map<i32, i64>(1i32, 2i64)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_canonical_forwarding_map_envelope_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding on map envelope mismatch from call return") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i64>>]
makeMarker() {
  return(map<i32, i64>(1i32, 2i64))
}

[return<int>]
/vector/count([vector<i32>] values, [map<i32, string>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [map<i32, i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_canonical_forwarding_map_call_return_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm local alias slash-method vector count on string receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./vector/count())
}
)";
  const std::string srcPath =
      writeTemp("vm_local_alias_slash_vector_count_string_no_helper.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_local_alias_slash_vector_count_string_no_helper_out.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /string/count") != std::string::npos);
}

TEST_CASE("rejects vm local alias slash-method vector count on array receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./vector/count())
}
)";
  const std::string srcPath =
      writeTemp("vm_local_alias_slash_vector_count_array_no_helper.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_local_alias_slash_vector_count_array_no_helper_out.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /array/count") != std::string::npos);
}

TEST_SUITE_END();
