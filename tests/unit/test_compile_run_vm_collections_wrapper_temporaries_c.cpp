#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("rejects vm templated stdlib wrapper temporary unsafe parity missing arguments") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(plus(
      plus(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32)),
           wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe()),
      plus(vectorAtUnsafe<i32>(wrapVector<i32>(4i32)), wrapVector<i32>(4i32).at_unsafe())))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity_missing_arguments.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary count capacity parity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(plus(
      plus(mapCount<i32, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32)),
           wrapMap<string, i32>("only"raw_utf8, 5i32).count(1i32)),
      plus(vectorCount<bool>(wrapVector<i32>(4i32)),
           plus(vectorCapacity<bool>(wrapVector<i32>(4i32)), wrapVector<i32>(4i32).capacity(1i32)))))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_capacity_parity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary index value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [bool] bad{wrapMap<string, i32>("only"raw_utf8, 4i32)["only"raw_utf8]}
  return(plus(0i32, bad))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_index_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary call key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_key_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_vm_stdlib_collection_shim_templated_return_temp_call_key_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " +
                             quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("at requires string map key") !=
        std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary call value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAt<string, bool>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe call key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_call_key_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_vm_stdlib_collection_shim_templated_return_temp_unsafe_call_key_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " +
                             quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("at_unsafe requires string map key") !=
        std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe call value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAtUnsafe<string, bool>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_call_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary count key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapCount<i32, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary count value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapCount<string, bool>(wrapMap<string, i32>("only"raw_utf8, 4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary call missing key argument") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_missing_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe call missing key argument") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_call_missing_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary count call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(mapCount<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary count method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).count(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at("only"raw_utf8, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary method missing key argument") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at())
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_method_missing_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at_unsafe("only"raw_utf8, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary unsafe method missing key argument") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at_unsafe())
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_method_missing_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary call type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAt<bool>(wrapVector<i32>(4i32), 0i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary call index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAt<i32>(wrapVector<i32>(4i32), true))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_index_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAt<i32>(wrapVector<i32>(4i32), 0i32, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary call missing index") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAt<i32>(wrapVector<i32>(4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_missing_index.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe call type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAtUnsafe<bool>(wrapVector<i32>(4i32), 0i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe call index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAtUnsafe<i32>(wrapVector<i32>(4i32), true))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_index_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAtUnsafe<i32>(wrapVector<i32>(4i32), 0i32, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary unsafe call missing index") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorAtUnsafe<i32>(wrapVector<i32>(4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_missing_index.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary count type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorCount<bool>(wrapVector<i32>(4i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_count_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary count call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorCount<i32>(wrapVector<i32>(4i32), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_count_call_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary count method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).count(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_count_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib vector wrapper temporary method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at(0i32, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_method_arity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_SUITE_END();
