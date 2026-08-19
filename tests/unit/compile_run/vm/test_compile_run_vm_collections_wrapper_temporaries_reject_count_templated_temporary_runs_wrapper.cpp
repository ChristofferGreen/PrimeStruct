#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("runs vm imported container error contract conformance") {
  expectContainerErrorConformance("vm");
}

TEST_CASE("runs vm checked pointer conformance harness for imported .prime helpers") {
  expectCheckedPointerHelperSurfaceConformance("vm");
  expectCheckedPointerGrowthConformance("vm");
  expectCheckedPointerUninitializedPrefixMoveConformance("vm");
  expectCheckedPointerOutOfBoundsConformance("vm");
  expectCheckedPointerUninitializedOutOfBoundsConformance("vm");
}

TEST_CASE("runs vm with templated stdlib vector wrapper temporary call forms") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(/std/collections/vector/vector<T>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] a{/std/collections/vector/at<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] b{/std/collections/vector/at_unsafe<i32>(wrapVector<i32>(5i32), 0i32)}
  [i32] c{/std/collections/vector/count<i32>(wrapVector<i32>(6i32))}
  [i32] d{/std/collections/vector/capacity<i32>(wrapVector<i32>(7i32))}
  return(plus(plus(plus(a, b), c), d))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_forms.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm templated stdlib vector wrapper temporary methods in expressions") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(/std/collections/vector/vector<T>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] a{wrapVector<i32>(4i32).at(0i32)}
  [i32] b{wrapVector<i32>(5i32).at_unsafe(0i32)}
  [i32] c{wrapVector<i32>(6i32).count()}
  [i32] d{wrapVector<i32>(7i32).capacity()}
  return(plus(plus(plus(a, b), c), d))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_methods.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_templated_return_vector_temp_methods.out")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 11);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("runs vm with templated stdlib wrapper temporary index forms") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(/std/collections/vector/vector<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] a{wrapVector<i32>(4i32)[0i32]}
  [i32] b{wrapMap<string, i32>("only"raw_utf8, 5i32)["only"raw_utf8]}
  return(plus(a, b))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temp_index_forms.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_stdlib_collection_shim_templated_return_temp_index_forms_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  // TODO-4741: mapSingle<K,V> is unimplemented (no constructor of that name
  // exists even for concrete key/value types).
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapSingle") != std::string::npos);
}

TEST_CASE("runs vm with templated stdlib wrapper temporary syntax parity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(/std/collections/vector/vector<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] vectorCall{/std/collections/vector/at<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] vectorMethod{wrapVector<i32>(4i32).at(0i32)}
  [i32] vectorIndex{wrapVector<i32>(4i32)[0i32]}
  [i32] mapCall{/std/collections/map/at<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at("only"raw_utf8)}
  [i32] mapIndex{wrapMap<string, i32>("only"raw_utf8, 5i32)["only"raw_utf8]}
  return(plus(plus(plus(vectorCall, vectorMethod), vectorIndex), plus(plus(mapCall, mapMethod), mapIndex)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_syntax_parity.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_templated_return_temp_syntax_parity.out")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  // TODO-4741: mapSingle<K,V> is unimplemented (no constructor of that name
  // exists even for concrete key/value types).
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: mapSingle") != std::string::npos);
}

TEST_CASE("runs vm with templated stdlib wrapper temporary unsafe parity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(/std/collections/vector/vector<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] vectorCall{/std/collections/vector/at_unsafe<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] vectorMethod{wrapVector<i32>(4i32).at_unsafe(0i32)}
  [i32] mapCall{/std/collections/map/at_unsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe("only"raw_utf8)}
  return(plus(plus(vectorCall, vectorMethod), plus(mapCall, mapMethod)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_templated_return_temp_unsafe_parity.out")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  // TODO-4741: mapSingle<K,V> is unimplemented (no constructor of that name
  // exists even for concrete key/value types).
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: mapSingle") != std::string::npos);
}

TEST_CASE("runs vm templated stdlib wrapper temporary count capacity parity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(/std/collections/vector/vector<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{/std/collections/map/count<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32))}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).count()}
  [i32] vectorCountCall{/std/collections/vector/count<i32>(wrapVector<i32>(4i32))}
  [i32] vectorCountMethod{wrapVector<i32>(4i32).count()}
  [i32] vectorCapacityCall{/std/collections/vector/capacity<i32>(wrapVector<i32>(4i32))}
  [i32] vectorCapacityMethod{wrapVector<i32>(4i32).capacity()}
  return(plus(plus(plus(mapCall, mapMethod), plus(vectorCountCall, vectorCountMethod)),
              plus(vectorCapacityCall, vectorCapacityMethod)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_capacity_parity.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_stdlib_collection_shim_templated_return_temp_count_capacity_parity_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  // TODO-4741: mapSingle<K,V> is unimplemented (no constructor of that name
  // exists even for concrete key/value types).
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapSingle") != std::string::npos);
}


TEST_SUITE_END();
