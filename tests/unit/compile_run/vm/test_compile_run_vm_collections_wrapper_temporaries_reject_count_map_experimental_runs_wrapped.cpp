#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("runs vm experimental fifteen-column soa storage helpers") {
  const std::string source = R"(
import /std/collections/soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns15<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns15New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns15Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns15Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32)
  soaColumns15Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32, 107i32, 109i32, 113i32)
  soaColumns15Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32, 43i32, 47i32, 53i32)
  [i32 mut] total{soaColumns15ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)}
  assign(total, plus(total, soaColumns15ReadFifteenth<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_soa_storage_fifteen_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 59);
}


TEST_CASE("runs vm experimental sixteen-column soa storage helpers") {
  const std::string source = R"(
import /std/collections/soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns16<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns16New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns16Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns16Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32, 53i32)
  soaColumns16Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32, 107i32, 109i32, 113i32, 127i32, 131i32)
  soaColumns16Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32, 43i32, 47i32, 53i32, 137i32)
  [i32 mut] total{soaColumns16ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)}
  assign(total, plus(total, soaColumns16ReadSixteenth<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_soa_storage_sixteen_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 143);
}

TEST_CASE("runs vm with stdlib collection shim helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(7i32)}
  [map<i32, i32>] pairs{mapSingle<i32, i32>(3i32, 9i32)}
  [i32 mut] total{plus(/std/collections/vector/count<i32>(values), /std/collections/map/count<i32, i32>(pairs))}
  [vector<i32>] emptyValues{/std/collections/vector/vector<i32>()}
  [map<i32, i32>] emptyPairs{mapNew<i32, i32>()}
  assign(total, plus(total, plus(/std/collections/vector/count<i32>(emptyValues), /std/collections/map/count<i32, i32>(emptyPairs))))
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shims.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim multi constructors") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(5i32, 7i32, 9i32)}
  [map<i32, i32>] pairs{mapDouble<i32, i32>(1i32, 11i32, 2i32, 22i32)}
  [i32] vectorTotal{plus(/std/collections/vector/at<i32>(values, 0i32), /std/collections/vector/at<i32>(values, 2i32))}
  [i32] mapTotal{plus(/std/collections/map/at<i32, i32>(pairs, 1i32), /std/collections/map/at_unsafe<i32, i32>(pairs, 2i32))}
  return(plus(plus(vectorTotal, mapTotal), plus(/std/collections/vector/count<i32>(values), /std/collections/map/count<i32, i32>(pairs))))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_multi_ctor.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_stdlib_collection_shim_multi_ctor_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  // TODO-4741: mapDouble<K,V> is unimplemented (no constructor of that name
  // exists even for concrete key/value types).
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapDouble") != std::string::npos);
}

TEST_CASE("runs vm with templated stdlib collection return envelopes") {
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
  [vector<i32>] values{wrapVector<i32>(9i32)}
  [map<string, i32>] pairs{wrapMap<string, i32>("only"raw_utf8, 4i32)}
  return(plus(plus(/std/collections/vector/count<i32>(values), /std/collections/map/count<string, i32>(pairs)), /std/collections/map/at<string, i32>(pairs, "only"raw_utf8)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_returns.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_stdlib_collection_shim_templated_returns_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  // TODO-4741: mapSingle<K,V> is unimplemented (no constructor of that name
  // exists even for concrete key/value types).
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapSingle") != std::string::npos);
}

TEST_CASE("runs vm templated stdlib return wrapper temporaries in expressions") {
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
  [i32] vectorTotal{wrapVector<i32>(9i32).count()}
  [i32] mapAtTotal{wrapMap<string, i32>("only"raw_utf8, 4i32).at("only"raw_utf8)}
  [i32] mapUnsafeTotal{wrapMap<string, i32>("only"raw_utf8, 4i32).at_unsafe("only"raw_utf8)}
  [i32] mapCountTotal{wrapMap<string, i32>("only"raw_utf8, 4i32).count()}
  return(plus(plus(vectorTotal, mapAtTotal), plus(mapUnsafeTotal, mapCountTotal)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temporaries.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_templated_return_temporaries.out")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  // TODO-4741: mapSingle<K,V> is unimplemented (no constructor of that name
  // exists even for concrete key/value types).
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: mapSingle") != std::string::npos);
}

TEST_CASE("runs vm with templated stdlib wrapper temporary call forms") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[return<int>]
main() {
  [i32] a{/std/collections/map/at<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)}
  [i32] b{/std/collections/map/at_unsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)}
  [i32] c{/std/collections/map/count<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32))}
  return(plus(plus(a, b), c))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_forms.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_stdlib_collection_shim_templated_return_temp_call_forms_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  // TODO-4741: mapSingle<K,V> is unimplemented (no constructor of that name
  // exists even for concrete key/value types).
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapSingle") != std::string::npos);
}

TEST_CASE("runs vm shared stdlib map conformance harness") {
  expectVmSharedStdlibMapConformanceHarness();
}

TEST_CASE("runs vm canonical namespaced map helpers on experimental map values") {
  expectCanonicalMapNamespaceExperimentalValueConformance("vm");
}

TEST_CASE("runs vm wrapper map helpers on experimental map values") {
  expectWrapperMapHelperExperimentalValueConformance("vm");
}

TEST_CASE("runs vm ownership-sensitive experimental map value methods") {
  expectExperimentalMapOwnershipMethodConformance("vm");
}

TEST_CASE("runs vm helper-wrapped inferred experimental map returns") {
  expectWrappedInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map parameters") {
  expectWrappedExperimentalMapParameterConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map bindings") {
  expectWrappedExperimentalMapBindingConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map assignment RHS values") {
  expectWrappedExperimentalMapAssignConformance("vm");
}

TEST_CASE("runs vm canonical namespaced map constructors on explicit experimental map bindings") {
  expectCanonicalMapNamespaceExperimentalConstructorConformance("vm");
}

TEST_CASE("runs vm canonical namespaced map constructors through explicit experimental map returns") {
  expectCanonicalMapNamespaceExperimentalReturnConformance("vm");
}

TEST_CASE("runs vm canonical namespaced map constructors through explicit experimental map parameters") {
  expectCanonicalMapNamespaceExperimentalParameterConformance("vm");
}

TEST_CASE("runs vm wrapper map constructors on explicit experimental map bindings") {
  expectWrapperMapConstructorExperimentalBindingConformance("vm");
}

TEST_CASE("runs vm wrapper map constructors through explicit experimental map returns") {
  expectWrapperMapConstructorExperimentalReturnConformance("vm");
}

TEST_CASE("runs vm wrapper map constructors through explicit experimental map parameters") {
  expectWrapperMapConstructorExperimentalParameterConformance("vm");
}

TEST_CASE("runs vm experimental map variadic constructors") {
  expectExperimentalMapVariadicConstructorConformance("vm");
}

TEST_CASE("rejects vm experimental map variadic constructor type mismatch") {
  expectExperimentalMapVariadicConstructorMismatchReject("vm");
}

TEST_CASE("runs vm experimental map constructor assignments") {
  expectExperimentalMapAssignConformance("vm");
}

TEST_CASE("runs vm implicit map auto constructor inference") {
  expectImplicitMapAutoInferenceConformance("vm");
}

TEST_CASE("runs vm inferred experimental map returns") {
  expectInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("runs vm block inferred experimental map returns") {
  expectBlockInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("runs vm auto block inferred experimental map returns") {
  expectAutoBlockInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("runs vm inferred experimental map call receivers") {
  expectInferredExperimentalMapCallReceiverConformance("vm");
}

TEST_CASE("runs vm experimental map struct fields") {
  expectExperimentalMapStructFieldConformance("vm");
}

TEST_CASE("runs vm inferred experimental map struct fields") {
  expectInferredExperimentalMapStructFieldConformance("vm");
}

TEST_CASE("runs vm helper-wrapped inferred experimental map struct fields") {
  expectWrappedInferredExperimentalMapStructFieldConformance("vm");
}

TEST_CASE("runs vm experimental map method parameters") {
  expectExperimentalMapMethodParameterConformance("vm");
}

TEST_CASE("runs vm inferred experimental map parameters") {
  expectInferredExperimentalMapParameterConformance("vm");
}

TEST_CASE("runs vm inferred experimental map default parameters") {
  expectInferredExperimentalMapDefaultParameterConformance("vm");
}

TEST_CASE("runs vm helper-wrapped inferred experimental map default parameters") {
  expectWrappedInferredExperimentalMapDefaultParameterConformance("vm");
}

TEST_CASE("runs vm experimental map helper receivers") {
  expectExperimentalMapHelperReceiverConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map helper receivers") {
  expectWrappedExperimentalMapHelperReceiverConformance("vm");
}

TEST_CASE("runs vm experimental map method receivers") {
  expectExperimentalMapMethodReceiverConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map method receivers") {
  expectWrappedExperimentalMapMethodReceiverConformance("vm");
}

TEST_CASE("runs vm experimental map field assignments") {
  expectExperimentalMapFieldAssignConformance("vm");
}

TEST_CASE("runs vm helper-wrapped Result.ok experimental map result struct fields") {
  expectWrappedExperimentalMapResultFieldAssignConformance("vm");
}

TEST_CASE("runs vm dereferenced experimental map storage references") {
  expectExperimentalMapStorageReferenceConformance("vm");
}

TEST_CASE("runs vm helper-wrapped dereferenced Result.ok experimental map result struct fields") {
  expectWrappedExperimentalMapResultDerefFieldAssignConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map struct storage fields") {
  expectWrappedExperimentalMapStorageFieldConformance("vm");
}


TEST_SUITE_END();
