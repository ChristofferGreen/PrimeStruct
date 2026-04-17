#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("rejects vm vector alias compatibility template forwarding on primitive mismatch from call return") {
  const std::string source = R"(
[return<i32>]
makeMarker() {
  return(1i32)
}

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
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_canonical_forwarding_primitive_call_return_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding when unknown expected meets primitive call return") {
  const std::string source = R"(
Marker() {}

[return<i32>]
makeMarker() {
  return(1i32)
}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_canonical_forwarding_unknown_expected_primitive_call_return.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding when unknown expected meets primitive binding") {
  const std::string source = R"(
Marker() {}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [i32] marker{1i32}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_canonical_forwarding_unknown_expected_primitive_binding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector alias compatibility template forwarding when unknown expected meets vector envelope binding") {
  const std::string source = R"(
Marker() {}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [vector<i32>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [vector<i32>] marker{vector<i32>(1i32)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_canonical_forwarding_unknown_expected_vector_envelope_binding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  expectVmVectorCountCompatibilityTypeMismatchReject(runCmd);
}

TEST_CASE("rejects vm vector helper method expression legacy alias forwarding") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(plus(/vector/push(values, 2i32), values.push(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_helper_method_expression_canonical_stdlib_forwarding.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_vector_helper_method_expression_canonical_stdlib_forwarding_out.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("rejects vm vector alias named-argument compatibility template forwarding") {
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
  return(plus(/vector/count([values] values, [marker] true),
              values.count([marker] true)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_alias_named_argument_compatibility_template_forwarding_reject.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_vector_alias_named_argument_compatibility_template_forwarding_reject_out.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown named argument: marker") != std::string::npos);
}

TEST_CASE("rejects vm wrapper temporary templated vector method compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count<i32>(true))
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_temp_templated_vector_method_compatibility_forwarding_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_temp_templated_vector_method_compatibility_forwarding_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects vm array alias templated forwarding to canonical vector helper") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/array/count<i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("vm_array_alias_templated_vector_forwarding_rejected.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib templated vector count fallback to array alias") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/array/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count<i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_templated_vector_call_array_fallback_rejected.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("compiles and runs vm array alias count through same-path helper") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(46i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/count(wrapVector(), true))
}
)";
  const std::string srcPath = writeTemp("vm_array_alias_count_same_path_wrapper_vector.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 46);
}

TEST_CASE("compiles and runs vm array alias capacity through same-path helper") {
  const std::string source = R"(
[return<int>]
/array/capacity([vector<i32>] values, [bool] marker) {
  return(48i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/capacity(wrapVector(), true))
}
)";
  const std::string srcPath = writeTemp("vm_array_alias_capacity_same_path_wrapper_vector.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 48);
}

TEST_CASE("compiles and runs vm array alias at through same-path helper") {
  const std::string source = R"(
[return<int>]
/array/at([vector<i32>] values, [i32] index, [bool] marker) {
  return(48i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at(wrapVector(), 1i32, true))
}
)";
  const std::string srcPath = writeTemp("vm_array_alias_at_same_path_wrapper_vector.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 48);
}

TEST_CASE("compiles and runs vm array alias at_unsafe through same-path helper") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([vector<i32>] values, [i32] index, [bool] marker) {
  return(50i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at_unsafe(wrapVector(), 1i32, true))
}
)";
  const std::string srcPath = writeTemp("vm_array_alias_at_unsafe_same_path_wrapper_vector.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 50);
}

TEST_CASE("compiles and runs vm array alias slash-method helpers through same-path helpers") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/array/count([vector<i32>] values) {
  return(77i32)
}

[return<int>]
/array/capacity([vector<i32>] values) {
  return(78i32)
}

[return<Marker>]
/array/at([vector<i32>] values, [i32] index) {
  return(Marker(plus(index, 10i32)))
}

[return<Marker>]
/array/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(plus(index, 20i32)))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(plus(values./array/count(), values./array/capacity()),
              plus(plus(values./array/at(1i32).tag(), values./array/at_unsafe(2i32).tag()),
                   plus(wrapVector()./array/at(0i32).tag(), wrapVector()./array/at_unsafe(1i32).tag()))))
}
)";
  const std::string srcPath = writeTemp("vm_array_alias_method_helpers_same_path_vector_receivers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm vector alias templated forwarding past non-templated compatibility helper") {
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
  return(plus(/vector/count<i32>(values, true), values.count<i32>(true)))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_templated_alias_forwarding_non_template_compat_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_vector_templated_alias_forwarding_non_template_compat_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("runs vm vector namespaced mutator alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(vectorAt<i32>(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_vector_namespaced_mutator_alias.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm vector namespaced count capacity access aliases without alias definitions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  [i32] c{/vector/count(values)}
  [i32] k{/vector/capacity(values)}
  [i32] first{/vector/at(values, 0i32)}
  [i32] second{/vector/at_unsafe(values, 1i32)}
  return(plus(plus(c, k), plus(first, second)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_namespaced_count_access_aliases.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_namespaced_count_access_aliases_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("runs vm with collection bracket literals") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>[1i32, 2i32]}
  [vector<i32>] list{vector<i32>[3i32, 4i32]}
  [map<i32, i32>] table{map<i32, i32>[5i32=6i32]}
  return(plus(plus(values.count(), vectorCount<i32>(list)), count(table)))
}
)";
  const std::string srcPath = writeTemp("vm_collection_brackets.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm vector literal count method without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(vector<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("vm_vector_literal_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with vector method call") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/vector/first([vector<i32>] items) {
  return(items[0i32])
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  return(values.first())
}
)";
  const std::string srcPath = writeTemp("vm_vector_method_call.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with bare vector literal unsafe access through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  return(at_unsafe(vector<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_vector_literal_unsafe.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_literal_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend only supports at()") != std::string::npos);
}

TEST_CASE("runs vm bare vector at through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_vector_at_imported.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("rejects vm bare vector at without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_vector_at_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_vector_at_import_requirement_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects vm bare vector at method without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_vector_at_method_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_vector_at_method_import_requirement_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects vm wrapper temporary vector at method without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_wrapper_vector_at_method_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_wrapper_vector_at_method_import_requirement_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("runs vm bare vector at_unsafe through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_vector_at_unsafe_imported.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("rejects vm bare vector at_unsafe without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_vector_at_unsafe_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_vector_at_unsafe_import_requirement_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects vm bare vector at_unsafe method without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_vector_at_unsafe_method_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_vector_at_unsafe_method_import_requirement_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects vm wrapper temporary vector at_unsafe method without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().at_unsafe(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_vector_at_unsafe_method_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_wrapper_vector_at_unsafe_method_import_requirement_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("runs vm with map at helper") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at(values, 3i32))
}
)";
  const std::string srcPath = writeTemp("vm_map_at.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with map at_unsafe helper") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_map_at_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}
TEST_CASE("runs vm with array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_array_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with bare vector count through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(vectorCount<i32>(values), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with bare vector access through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32, 4i32)}
  return(plus(
      plus(vectorAt<i32>(values, 0i32), vectorAtUnsafe<i32>(values, 1i32)),
      plus(/std/collections/vector/at<i32>(values, 2i32), /std/collections/vector/at_unsafe<i32>(values, 3i32))))
}
)";
  const std::string srcPath = writeTemp("vm_vector_access_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("rejects vm bare vector count without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_vector_count_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_count_import_requirement_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects vm bare vector count method without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_vector_count_method_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_count_method_import_requirement_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("rejects vm wrapper vector count slash-method chains before receiver typing") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector()./vector/count().tag())
}
)";
  const std::string srcPath = writeTemp("vm_wrapper_vector_count_slash_chain_unknown_method.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_vector_count_slash_chain_unknown_method_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects vm wrapper vector capacity slash-method chains before receiver typing") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector()./vector/capacity().tag())
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_vector_capacity_slash_chain_unknown_method.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_vector_capacity_slash_chain_unknown_method_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown method: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("rejects vm local alias slash-method vector capacity on string receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("vm_local_alias_slash_vector_capacity_string_no_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_local_alias_slash_vector_capacity_string_no_helper_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("rejects vm local alias slash-method vector capacity on array receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("vm_local_alias_slash_vector_capacity_array_no_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_local_alias_slash_vector_capacity_array_no_helper_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("capacity requires vector target") != std::string::npos);
}

TEST_SUITE_END();
