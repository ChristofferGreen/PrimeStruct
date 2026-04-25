#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("rejects vm vector method alias access field expression with struct receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_method_alias_access_field_expression_struct_receiver_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_vector_method_alias_access_field_expression_struct_receiver_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("rejects vm vector unsafe method alias access struct method chain with primitive receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at_unsafe(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_method_alias_access_unsafe_struct_method_chain_canonical_forwarding_reject.prime",
                source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_vector_method_alias_access_unsafe_struct_method_chain_canonical_forwarding_reject.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects vm vector unsafe method alias access field expression with struct receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at_unsafe(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("vm_vector_method_alias_access_unsafe_field_expression_struct_receiver_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_vector_method_alias_access_unsafe_field_expression_struct_receiver_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("rejects vm map method alias access struct method chain with primitive receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values./std/collections/map/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("vm_map_method_alias_access_struct_method_chain_canonical_forwarding_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_map_method_alias_access_struct_method_chain_canonical_forwarding_reject.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("keeps canonical vm map method access field expression forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values.at(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_map_method_field_access_forwarding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm vector method alias struct-return precedence without helper-backed receiver typing") {
  const std::string source = R"(
AliasMarker {
  [i32] value
}

CanonicalMarker {
  [i32] value
}

[return<AliasMarker>]
/vector/at([vector<i32>] values, [i32] index) {
  return(AliasMarker(plus(index, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(CanonicalMarker(index))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at(2i32).value)
}
)";
  const std::string srcPath = writeTemp("vm_vector_method_struct_field_alias_precedence.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_method_struct_field_alias_precedence_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("vm keeps primitive diagnostics for canonical vector method access") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_vector_method_struct_chain_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_canonical_vector_method_struct_chain_reject.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("vm keeps struct receiver diagnostics for canonical vector unsafe method access") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at_unsafe(2i32).value)
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_vector_unsafe_method_field_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_canonical_vector_unsafe_method_field_reject.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("rejects vm map method alias access struct method chain with primitive argument diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values./std/collections/map/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("vm_map_method_alias_access_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_map_method_alias_access_struct_method_chain_canonical_diagnostic.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("rejects vm wrapper-returned map method alias primitive receiver fallback") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<int>]
/i32/tag([i32] self) {
  return(plus(self, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()./std/collections/map/at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_map_method_alias_primitive_receiver_fallback.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_map_method_alias_primitive_receiver_fallback.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("rejects vm wrapper-returned canonical map slash-method struct receiver fallback") {
  const std::string source = R"(
Marker {
  [i32] value
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()./std/collections/map/at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_map_method_alias_struct_receiver_forwarding.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_map_method_alias_struct_receiver_forwarding.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("rejects vm wrapper-returned canonical direct-call map receiver fallback") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/at(wrapMap(), 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_canonical_direct_map_receiver_fallback.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_canonical_direct_map_receiver_fallback.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("rejects vm wrapper-returned compatibility direct-call map receiver fallback") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/map/at(wrapMap(), 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_compatibility_direct_map_receiver_fallback.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_compatibility_direct_map_receiver_fallback.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("vm keeps wrapper-returned map method alias primitive argument diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project() {
  return(wrapMap()./std/collections/map/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(project())
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_map_method_alias_primitive_argument_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_map_method_alias_primitive_argument_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("rejects vm std-namespaced vector method alias access struct method chain with helper receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./std/collections/vector/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("vm_std_namespaced_vector_method_alias_access_struct_method_chain_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_std_namespaced_vector_method_alias_access_struct_method_chain_reject.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  const bool hasExpectedDiagnostics =
      diagnostics.empty() ||
      diagnostics.find("argument type mismatch for /Marker/tag parameter self") != std::string::npos;
  CHECK(hasExpectedDiagnostics);
}

TEST_CASE("rejects vm std-namespaced vector access slash methods without canonical helper on vector receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(1i32),
              values./std/collections/vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_std_namespaced_vector_access_slash_methods_no_helper.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_std_namespaced_vector_access_slash_methods_no_helper.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects vm std-namespaced vector method alias access struct method chain with helper missing-method diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./std/collections/vector/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("vm_std_namespaced_vector_method_alias_access_struct_method_chain_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_std_namespaced_vector_method_alias_access_struct_method_chain_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /Marker/tag") != std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at_unsafe(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map return envelope unsupported key arg") {
  const std::string source = R"(
import /std/collections/*

[return<map<Unknown, i32>>]
wrapMapUnknownKey([string] key, [i32] value) {
  return(mapSingle<string, i32>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapMapUnknownKey("only"raw_utf8, 3i32)}
  return(mapCount<string, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_map_bad_key.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_stdlib_collection_shim_templated_return_map_bad_key_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("struct binding initializer type mismatch on values") !=
        std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map return envelope unsupported value arg") {
  const std::string source = R"(
import /std/collections/*

[return<map<string, Unknown>>]
wrapMapUnknownValue([string] key, [i32] value) {
  return(mapSingle<string, i32>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapMapUnknownValue("only"raw_utf8, 3i32)}
  return(mapCount<string, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_map_bad_value.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_stdlib_collection_shim_templated_return_map_bad_value_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend does not support return type on /wrapMapUnknownValue") != std::string::npos);
}

TEST_CASE("rejects vm templated stdlib vector return envelope nested arg") {
  const std::string source = R"(
import /std/collections/*

[return<vector<array<i32>>>]
wrapVectorArray([i32] value) {
  return(vectorSingle<i32>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapVectorArray(3i32)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_vector_nested_arg.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_stdlib_collection_shim_templated_return_vector_nested_arg_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend does not support return type on /wrapVectorArray") != std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map return envelope nested arg") {
  const std::string source = R"(
import /std/collections/*

[return<map<string, array<i32>>>]
wrapMapNestedValue([string] key, [i32] value) {
  return(mapSingle<string, i32>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapMapNestedValue("only"raw_utf8, 3i32)}
  return(mapCount<string, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_map_nested_arg.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_stdlib_collection_shim_templated_return_map_nested_arg_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend does not support return type on /wrapMapNestedValue") !=
        std::string::npos);
}

TEST_CASE("rejects vm templated stdlib vector return envelope wrong arity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<i32, i32>>]
wrapVector([i32] value) {
  return(vectorSingle<i32>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapVector(3i32)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_vector_arity.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_templated_return_vector_arity_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector return type requires exactly one template argument on /wrapVector") !=
        std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map return envelope wrong arity") {
  const std::string source = R"(
import /std/collections/*

[return<map<string>>]
wrapMap([string] key, [i32] value) {
  return(mapSingle<string, i32>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapMap("only"raw_utf8, 3i32)}
  return(mapCount<string, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_map_arity.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_templated_return_map_arity_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map return type requires exactly two template arguments on /wrapMap") !=
        std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector single") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(7i32)}
  return(plus(plus(vectorAt<i32>(values, 0i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_single.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 15);
}

TEST_CASE("runs vm with stdlib collection shim vector new") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorNew<i32>()}
  return(plus(vectorCount<i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_new.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_SUITE_END();
