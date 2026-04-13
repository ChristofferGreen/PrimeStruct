#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native templated stdlib vector wrapper temporary call type mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_call_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary call index mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_call_index_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary call arity mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_call_arity_mismatch.prime",
                source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary call missing index") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_call_missing_index.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary unsafe call type mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary unsafe call index mismatch") {
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
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_index_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary unsafe call arity mismatch") {
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
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_arity_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary unsafe call missing index") {
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
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_vector_temp_unsafe_call_missing_index.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary count type mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_count_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary count call arity mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_count_call_arity_mismatch.prime",
                source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary count method arity mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_count_method_arity.prime",
                source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary method arity mismatch") {
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_method_arity.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary method missing index") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_method_missing_index.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary unsafe method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at_unsafe(0i32, 1i32))
}
)";
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_vector_temp_unsafe_method_arity.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary unsafe method missing index") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at_unsafe())
}
)";
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_vector_temp_unsafe_method_missing_index.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary capacity type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorCapacity<bool>(wrapVector<i32>(4i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_capacity_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary capacity call arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(vectorCapacity<i32>(wrapVector<i32>(4i32), 1i32))
}
)";
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_vector_temp_capacity_call_arity_mismatch.prime",
      source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary capacity method arity mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).capacity(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_capacity_method_arity.prime",
                source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary method index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_method_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32)[true])
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_index_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib vector wrapper temporary index value mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [bool] bad{wrapVector<i32>(4i32)[0i32]}
  return(plus(0i32, bad))
}
)";
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_templated_return_vector_temp_index_value_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}


TEST_SUITE_END();
#endif
