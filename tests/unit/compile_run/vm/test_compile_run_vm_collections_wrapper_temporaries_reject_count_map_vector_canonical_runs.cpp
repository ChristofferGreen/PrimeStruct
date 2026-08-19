#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("runs vm helper-wrapped dereferenced experimental map struct storage fields") {
  expectWrappedExperimentalMapStorageDerefFieldConformance("vm");
}

TEST_CASE("rejects vm canonical namespaced map helpers on borrowed experimental map values") {
  expectCanonicalMapNamespaceExperimentalReferenceConformance("vm");
}

TEST_CASE("runs vm canonical namespaced map _ref helpers on borrowed experimental map values") {
  expectCanonicalMapNamespaceExperimentalBorrowedRefConformance("vm");
}

TEST_CASE("runs vm experimental map methods") {
  expectExperimentalMapMethodConformance("vm");
}

TEST_CASE("runs vm borrowed experimental map helpers") {
  expectExperimentalMapReferenceHelperConformance("vm");
}

TEST_CASE("runs vm public borrowed map wrappers") {
  expectPublicMapReferenceWrapperConformance("vm");
}

TEST_CASE("runs vm borrowed experimental map methods") {
  expectExperimentalMapReferenceMethodConformance("vm");
}

TEST_CASE("runs vm experimental map inserts") {
  expectExperimentalMapInsertConformance("vm");
}

TEST_CASE("runs vm experimental map ownership-sensitive values") {
  expectExperimentalMapOwnershipConformance("vm");
}

TEST_CASE("runs vm canonical namespaced map inserts on explicit experimental map bindings") {
  expectCanonicalMapNamespaceExperimentalInsertConformance("vm");
}

TEST_CASE("runs vm builtin canonical map first-growth inserts") {
  expectBuiltinCanonicalMapInsertFirstGrowthConformance("vm");
}

TEST_CASE("runs vm builtin canonical map repeated-growth inserts") {
  expectBuiltinCanonicalMapInsertRepeatedGrowthConformance("vm");
}

TEST_CASE("runs vm builtin canonical map insert overwrites") {
  expectBuiltinCanonicalMapInsertOverwriteConformance("vm");
}

TEST_CASE("runs vm builtin canonical map non-local growth") {
  expectBuiltinCanonicalMapInsertNonLocalGrowthConformance("vm");
}

TEST_CASE("runs vm builtin canonical map nested non-local growth") {
  expectBuiltinCanonicalMapInsertNestedNonLocalGrowthConformance("vm");
}

TEST_CASE("runs vm builtin canonical map helper-return borrowed method inserts") {
  expectBuiltinCanonicalMapInsertHelperReturnBorrowedMethodConformance("vm");
}

TEST_CASE("runs vm builtin canonical map struct-field initializer") {
  expectBuiltinCanonicalMapStructFieldInitializerConformance("vm");
}

TEST_CASE("runs vm builtin canonical map direct insert on helper-return value receivers") {
  expectBuiltinCanonicalMapInsertHelperReturnValueDirectConformance("vm");
}

TEST_CASE("runs vm builtin canonical map method insert on helper-return value receivers") {
  expectBuiltinCanonicalMapInsertHelperReturnValueMethodConformance("vm");
}

TEST_CASE("runs vm builtin canonical map direct insert on borrowed holder field receivers") {
  expectBuiltinCanonicalMapInsertBorrowedHolderFieldDirectConformance("vm");
}

TEST_CASE("rejects vm canonical map constructor ownership growth") {
  expectCanonicalMapNamespaceOwnershipReject("vm");
}

TEST_CASE("runs vm experimental map bracket access") {
  expectExperimentalMapIndexConformance("vm");
}

TEST_CASE("runs vm experimental map custom comparable struct keys") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[return<bool>]
/Key/less_than([Key] left, [Key] right) {
  return(less_than(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapPair<Key, i32>(Key{2i32}, 7i32, Key{5i32}, 11i32)}
  [i32 mut] total{/std/collections/map/count<Key, i32>(values)}
  assign(total, plus(total, /std/collections/map/at<Key, i32>(values, Key{2i32})))
  assign(total, plus(total, /std/collections/map/at_unsafe<Key, i32>(values, Key{5i32})))
  if(/std/collections/map/contains<Key, i32>(values, Key{2i32}),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_experimental_map_custom_comparable_key.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_experimental_map_custom_comparable_key_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  // TODO-4741: experimental Map<K,V> rejects custom Comparable struct keys
  // even when the struct defines equal/less_than.
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find(
            "map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): Key") !=
        std::string::npos);
}

TEST_CASE("runs vm shared vector conformance harness for stdlib and experimental helpers") {
  expectSharedVectorConformanceHarness("vm");
}

TEST_CASE("runs vm canonical namespaced vector helpers") {
  expectCanonicalVectorNamespaceConformance("vm");
}

TEST_CASE("runs vm canonical namespaced vector helpers on explicit Vector bindings") {
  expectCanonicalVectorNamespaceExplicitVectorBindingConformance("vm");
}

TEST_CASE("runs vm stdlib wrapper vector helpers on explicit Vector bindings") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingConformance("vm");
}

TEST_CASE("rejects vm stdlib wrapper vector helper explicit Vector mismatch") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingMismatchReject("vm");
}

TEST_CASE("runs vm stdlib wrapper vector constructors on explicit Vector bindings") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingConformance("vm");
}

TEST_CASE("keeps vm stdlib wrapper vector constructor explicit Vector mismatch contract") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingMismatchContract("vm");
}

TEST_CASE("runs vm stdlib wrapper vector constructors on inferred auto bindings") {
  expectStdlibWrapperVectorConstructorAutoInferenceConformance("vm");
}

TEST_CASE("rejects vm stdlib wrapper vector constructor auto inference mismatch") {
  expectStdlibWrapperVectorConstructorAutoInferenceMismatchReject("vm");
}

TEST_CASE("rejects vm stdlib wrapper vector constructor receivers") {
  expectStdlibWrapperVectorConstructorReceiverConformance("vm");
}

TEST_CASE("rejects vm stdlib wrapper vector helper receiver mismatch") {
  expectStdlibWrapperVectorConstructorHelperReceiverMismatchReject("vm");
}

TEST_CASE("rejects vm stdlib wrapper vector method receiver mismatch") {
  expectStdlibWrapperVectorConstructorMethodReceiverMismatchReject("vm");
}

TEST_CASE("rejects vm canonical namespaced vector constructor temporaries") {
  expectCanonicalVectorNamespaceTemporaryReceiverConformance("vm");
}

TEST_CASE("rejects vm canonical namespaced vector explicit builtin bindings") {
  expectCanonicalVectorNamespaceExplicitBindingReject("vm");
}

TEST_CASE("rejects vm canonical namespaced vector named-argument temporaries") {
  expectCanonicalVectorNamespaceNamedArgsTemporaryReceiverConformance("vm");
}

TEST_CASE("rejects vm canonical namespaced vector named-argument explicit builtin bindings") {
  expectCanonicalVectorNamespaceNamedArgsExplicitBindingReject("vm");
}

TEST_CASE("rejects vm canonical namespaced vector mutators without imported helpers") {
  expectCanonicalVectorClearImportRequirement("vm");
  expectCanonicalVectorRemoveAtImportRequirement("vm");
  expectCanonicalVectorRemoveSwapImportRequirement("vm");
}

TEST_CASE("runs vm experimental vector helper runtime contracts") {
  expectExperimentalVectorRuntimeContracts("vm");
}

TEST_CASE("runs vm experimental vector ownership-sensitive helpers") {
  expectExperimentalVectorOwnershipContracts("vm");
}

TEST_CASE("runs vm canonical vector helpers on experimental vector receivers") {
  expectExperimentalVectorCanonicalHelperRoutingConformance("vm");
}

TEST_CASE("runs vm vector pop empty runtime contract") {
  SUBCASE("call") {
    expectVectorPopEmptyRuntimeContract("vm", false);
  }

  SUBCASE("method") {
    expectVectorPopEmptyRuntimeContract("vm", true);
  }
}

TEST_CASE("runs vm vector index runtime contract") {
  expectVectorIndexRuntimeContract("vm", "access_call");
  expectVectorIndexRuntimeContract("vm", "access_method");
  expectVectorIndexRuntimeContract("vm", "access_bracket");
  expectVectorIndexRuntimeContract("vm", "remove_at_call");
  expectVectorIndexRuntimeContract("vm", "remove_at_method");
  expectVectorIndexRuntimeContract("vm", "remove_swap_call");
  expectVectorIndexRuntimeContract("vm", "remove_swap_method");
}


TEST_SUITE_END();
