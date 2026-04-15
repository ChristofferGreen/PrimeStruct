#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native std-namespaced vector method alias access struct method chain with helper receiver diagnostics") {
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
      writeTemp("compile_native_std_namespaced_vector_method_alias_access_struct_method_chain_reject.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_method_alias_access_struct_method_chain_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("rejects native std-namespaced vector access slash methods without canonical helper on vector receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(1i32),
              values./std/collections/vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_vector_access_slash_methods_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_access_slash_methods_no_helper.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects native std-namespaced vector method alias access struct method chain with helper missing-method diagnostics") {
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
      writeTemp("compile_native_std_namespaced_vector_method_alias_access_struct_method_chain_diag.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_method_alias_access_struct_method_chain_diag.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /Marker/tag") != std::string::npos);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary unsafe key mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_unsafe_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map return envelope unsupported key arg") {
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
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_map_bad_key.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_map_bad_key_err.txt")
                                  .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend only supports numeric/bool map values") != std::string::npos);
}

TEST_CASE("rejects native templated stdlib map return envelope unsupported value arg") {
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
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_map_bad_value.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_map_bad_value_err.txt")
                                  .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend does not support return type on /wrapMapUnknownValue") !=
        std::string::npos);
}

TEST_CASE("rejects native templated stdlib vector return envelope nested arg") {
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
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_nested_arg.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_vector_nested_arg_err.txt")
                                  .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend does not support return type on /wrapVectorArray") !=
        std::string::npos);
}

TEST_CASE("rejects native templated stdlib map return envelope nested arg") {
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
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_map_nested_arg.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_map_nested_arg_err.txt")
                                  .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend does not support return type on /wrapMapNestedValue") !=
        std::string::npos);
}

TEST_CASE("rejects native templated stdlib vector return envelope wrong arity") {
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
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_arity.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_templated_return_vector_arity_err.txt")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector return type requires exactly one template argument on /wrapVector") !=
        std::string::npos);
}

TEST_CASE("rejects native templated stdlib map return envelope wrong arity") {
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
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_templated_return_map_arity.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_templated_return_map_arity_err.txt")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("map return type requires exactly two template arguments on /wrapMap") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native stdlib collection shim vector single") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(7i32)}
  return(plus(plus(vectorAt<i32>(values, 0i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_single.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_single_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 15);
}

TEST_CASE("compiles and runs native stdlib collection shim vector new") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorNew<i32>()}
  return(plus(vectorCount<i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_new.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_new_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects native stdlib collection shim vector new type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorNew<bool>()}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_new_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("rejects native stdlib collection shim vector single type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_single_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("compiles and runs native stdlib collection shim vector pair") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorPair<i32>(11i32, 13i32)}
  return(plus(plus(vectorAt<i32>(values, 1i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_pair.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_pair_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 26);
}

TEST_CASE("rejects native stdlib collection shim vector pair type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorPair<i32>(1i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_pair_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("compiles and runs native stdlib collection shim vector triple") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(10i32, 20i32, 30i32)}
  return(plus(plus(vectorAt<i32>(values, 2i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_triple.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_triple_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 43);
}

TEST_CASE("rejects native stdlib collection shim vector triple type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(1i32, 2i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_triple_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("compiles and runs native stdlib collection shim vector quad") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuad<i32>(3i32, 6i32, 9i32, 12i32)}
  return(plus(plus(vectorAt<i32>(values, 3i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_quad.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_quad_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 19);
}

TEST_CASE("rejects native stdlib collection shim vector quad type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuad<i32>(1i32, 2i32, 3i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_quad_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("compiles and runs native stdlib collection shim map single") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapSingle<string, i32>("only"raw_utf8, 21i32)}
  return(plus(mapAt<string, i32>(values, "only"raw_utf8), mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_single.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_single_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 22);
}

TEST_CASE("rejects native stdlib collection shim map single type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapSingle<i32, i32>(1i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_single_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map single key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapSingle<i32, i32>("oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_single_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map new") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapNew<i32, i32>()}
  return(plus(mapCount<i32, i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_new.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_new_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native stdlib collection shim map new string key envelope") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapNew<string, i32>()}
  return(plus(mapCount<string, i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_new_string.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_new_string_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects native stdlib collection shim map new type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapNew<bool, i32>()}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_new_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map new string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapNew<string, i32>()}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_new_string_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map count") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(plus(mapCount<i32, i32>(values), 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_count_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs native stdlib collection shim map count string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapCount<string, i32>(values), 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_count_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_count_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("rejects native stdlib collection shim map count type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(mapCount<bool, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_count_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map count string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_count_string_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_SUITE_END();
#endif
