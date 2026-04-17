#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native templated stdlib collection return envelope unsupported arg") {
  const std::string source = R"(
import /std/collections/*

[return<vector<Unknown>>]
wrapUnknown([i32] value) {
  return(vectorSingle<i32>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapUnknown(3i32)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_templated_return_bad_arg.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_bad_arg_err.txt")
                                  .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend does not support return type on /wrapUnknown") !=
        std::string::npos);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32).at(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary index key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(wrapMap<string, i32>("only"raw_utf8, 4i32)[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_index_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib wrapper temporary syntax parity key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(plus(
      plus(mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), 1i32),
           wrapMap<string, i32>("only"raw_utf8, 5i32).at(1i32)),
      wrapMap<string, i32>("only"raw_utf8, 5i32)[1i32]))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_syntax_parity_key_mismatch.prime",
                source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib wrapper temporary syntax parity value mismatch") {
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
  [bool] mapCall{mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [bool] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at("only"raw_utf8)}
  [bool] mapIndex{wrapMap<string, i32>("only"raw_utf8, 5i32)["only"raw_utf8]}
  [bool] vectorCall{vectorAt<i32>(wrapVector<i32>(4i32), 0i32)}
  [bool] vectorMethod{wrapVector<i32>(4i32).at(0i32)}
  [bool] vectorIndex{wrapVector<i32>(4i32)[0i32]}
  return(plus(plus(plus(mapCall, mapMethod), mapIndex), plus(plus(vectorCall, vectorMethod), vectorIndex)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_syntax_parity_value_mismatch.prime",
                source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib wrapper temporary unsafe parity mismatch") {
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
      plus(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), 1i32),
           wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe(1i32)),
      plus(vectorAtUnsafe<i32>(wrapVector<i32>(4i32), true), wrapVector<i32>(4i32).at_unsafe(true))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_unsafe_parity_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib wrapper temporary unsafe parity value mismatch") {
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
  [bool] mapCall{mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [bool] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe("only"raw_utf8)}
  [bool] vectorCall{vectorAtUnsafe<i32>(wrapVector<i32>(4i32), 0i32)}
  [bool] vectorMethod{wrapVector<i32>(4i32).at_unsafe(0i32)}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_temp_unsafe_parity_value_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib wrapper temporary unsafe parity arity mismatch") {
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
      plus(mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8, 1i32),
           wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe("only"raw_utf8, 1i32)),
      plus(vectorAtUnsafe<i32>(wrapVector<i32>(4i32), 0i32, 1i32),
           wrapVector<i32>(4i32).at_unsafe(0i32, 1i32))))
}
)";
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_temp_unsafe_parity_arity_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib wrapper temporary unsafe parity missing arguments") {
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
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_temp_unsafe_parity_missing_arguments.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib wrapper temporary count capacity parity mismatch") {
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
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_temp_count_capacity_parity_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary index value mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [bool] bad{wrapMap<string, i32>("only"raw_utf8, 4i32)["only"raw_utf8]}
  return(plus(0i32, bad))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_index_value_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary call key mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_call_key_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_stdlib_collection_shim_templated_return_temp_call_key_mismatch.err")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > " +
                                 quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/mapAt__") !=
        std::string::npos);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary call value mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_call_value_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary unsafe call key mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_unsafe_call_key_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_stdlib_collection_shim_templated_return_temp_unsafe_call_key_mismatch.err")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > " +
                                 quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/mapAtUnsafe__") !=
        std::string::npos);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary unsafe call value mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_unsafe_call_value_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary count key mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_count_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary count value mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_count_value_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary call arity mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_call_arity_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary call missing key argument") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_call_missing_key.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary unsafe call arity mismatch") {
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
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_temp_unsafe_call_arity_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary unsafe call missing key argument") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_unsafe_call_missing_key.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary count call arity mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_count_call_arity_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary count method arity mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_count_method_arity.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary method arity mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_method_arity.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary method missing key argument") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_method_missing_key.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary unsafe method arity mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_unsafe_method_arity.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary unsafe method missing key argument") {
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
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_temp_unsafe_method_missing_key.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_SUITE_END();
#endif
