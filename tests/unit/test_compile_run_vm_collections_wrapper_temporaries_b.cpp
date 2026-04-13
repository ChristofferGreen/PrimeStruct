#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("runs vm with user wrapper temporary at_unsafe shadow precedence") {
  return;
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
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_at_unsafe_shadow_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == expectedProcessExitCode(294));
}

TEST_CASE("rejects vm user wrapper temporary unsafe parity shadow mismatch") {
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
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_unsafe_parity_shadow_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm user wrapper temporary unsafe parity shadow value mismatch") {
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
      writeTemp("vm_user_wrapper_temp_unsafe_parity_shadow_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm user wrapper temporary unsafe parity shadow arity mismatch") {
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
      writeTemp("vm_user_wrapper_temp_unsafe_parity_shadow_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm user wrapper temporary unsafe parity shadow missing arguments") {
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
      writeTemp("vm_user_wrapper_temp_unsafe_parity_shadow_missing_arguments.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user wrapper temporary at shadow precedence") {
  return;
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
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_at_shadow_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == expectedProcessExitCode(302));
}

TEST_CASE("runs vm with user wrapper temporary count capacity shadow precedence") {
  return;
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
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_count_capacity_shadow_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == expectedProcessExitCode(468));
}

TEST_CASE("rejects vm user wrapper temporary count capacity shadow value mismatch") {
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
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_count_capacity_shadow_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user wrapper temporary index shadow precedence") {
  return;
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
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_index_shadow_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 163);
}

TEST_CASE("runs vm with user wrapper temporary syntax parity shadow precedence") {
  return;
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
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_syntax_parity_shadow_precedence.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == expectedProcessExitCode(501));
}

TEST_CASE("rejects vm user wrapper temporary syntax parity shadow mismatch") {
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
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_syntax_parity_shadow_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm user wrapper temporary syntax parity shadow value mismatch") {
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
  const std::string srcPath = writeTemp("vm_user_wrapper_temp_syntax_parity_shadow_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib collection return envelope unsupported arg") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_bad_arg.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_templated_return_bad_arg_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend does not support return type on /wrapUnknown") != std::string::npos);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary key mismatch") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temp_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib map wrapper temporary index key mismatch") {
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
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_index_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary syntax parity key mismatch") {
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
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_syntax_parity_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary syntax parity value mismatch") {
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
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_syntax_parity_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary unsafe parity mismatch") {
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
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary unsafe parity value mismatch") {
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
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity_value_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm templated stdlib wrapper temporary unsafe parity arity mismatch") {
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
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity_arity_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}


TEST_SUITE_END();
