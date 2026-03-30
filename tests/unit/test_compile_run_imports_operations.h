#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.imports");

TEST_CASE("compiles and runs collection literals in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  return(plus(at_unsafe(array<i32>{1i32, 2i32, 3i32}, 1i32),
              at(map<i32, i32>{1i32=10i32, 2i32=20i32}, 2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_collections_exe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 22);
}

TEST_CASE("compiles and runs string-keyed map literals in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  return(map<string, i32>{"a"utf8=1i32, "b"utf8=2i32}["b"utf8])
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_collections_string_map_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs string-keyed map literals with bracket sugar in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  return(map<string, i32>["a"=1i32, "b"=2i32]["b"utf8])
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map_brackets.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_collections_string_map_brackets_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs canonical namespaced map helpers on experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalValueConformance("exe");
}

TEST_CASE("compiles and runs wrapper map helpers on experimental map values in C++ emitter") {
  expectWrapperMapHelperExperimentalValueConformance("exe");
}

TEST_CASE("compiles and runs ownership-sensitive experimental map value methods in C++ emitter") {
  expectExperimentalMapOwnershipMethodConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped inferred experimental map returns in C++ emitter") {
  expectWrappedInferredExperimentalMapReturnConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped experimental map parameters in C++ emitter") {
  expectWrappedExperimentalMapParameterConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped experimental map bindings in C++ emitter") {
  expectWrappedExperimentalMapBindingConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped experimental map assignment RHS values in C++ emitter") {
  expectWrappedExperimentalMapAssignConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map constructors on explicit experimental map bindings in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalConstructorConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map constructors through explicit experimental map returns in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalReturnConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map constructors through explicit experimental map parameters in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalParameterConformance("exe");
}

TEST_CASE("compiles and runs wrapper map constructors on explicit experimental map bindings in C++ emitter") {
  expectWrapperMapConstructorExperimentalBindingConformance("exe");
}

TEST_CASE("compiles and runs wrapper map constructors through explicit experimental map returns in C++ emitter") {
  expectWrapperMapConstructorExperimentalReturnConformance("exe");
}

TEST_CASE("compiles and runs wrapper map constructors through explicit experimental map parameters in C++ emitter") {
  expectWrapperMapConstructorExperimentalParameterConformance("exe");
}

TEST_CASE("compiles and runs experimental map constructor assignments in C++ emitter") {
  expectExperimentalMapAssignConformance("exe");
}

TEST_CASE("compiles and runs implicit map auto constructor inference in C++ emitter") {
  expectImplicitMapAutoInferenceConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map returns in C++ emitter") {
  expectInferredExperimentalMapReturnConformance("exe");
}

TEST_CASE("compiles and runs block inferred experimental map returns in C++ emitter") {
  expectBlockInferredExperimentalMapReturnConformance("exe");
}

TEST_CASE("compiles and runs auto block inferred experimental map returns in C++ emitter") {
  expectAutoBlockInferredExperimentalMapReturnConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map call receivers in C++ emitter") {
  expectInferredExperimentalMapCallReceiverConformance("exe");
}

TEST_CASE("rejects explicit experimental map struct field constructors in C++ emitter") {
  expectExperimentalMapStructFieldConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map struct fields in C++ emitter") {
  expectInferredExperimentalMapStructFieldConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped inferred experimental map struct fields in C++ emitter") {
  expectWrappedInferredExperimentalMapStructFieldConformance("exe");
}

TEST_CASE("compiles and runs experimental map method parameters in C++ emitter") {
  expectExperimentalMapMethodParameterConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map parameters in C++ emitter") {
  expectInferredExperimentalMapParameterConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map default parameters in C++ emitter") {
  expectInferredExperimentalMapDefaultParameterConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped inferred experimental map default parameters in C++ emitter") {
  expectWrappedInferredExperimentalMapDefaultParameterConformance("exe");
}

TEST_CASE("compiles and runs experimental map helper receivers in C++ emitter") {
  expectExperimentalMapHelperReceiverConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped experimental map helper receivers in C++ emitter") {
  expectWrappedExperimentalMapHelperReceiverConformance("exe");
}

TEST_CASE("runs direct-constructor experimental map method receivers in C++ emitter") {
  expectExperimentalMapMethodReceiverConformance("exe");
}

TEST_CASE("runs helper-wrapped experimental map method receivers in C++ emitter") {
  expectWrappedExperimentalMapMethodReceiverConformance("exe");
}

TEST_CASE("compiles and runs experimental map field assignments through canonical helper access in C++ emitter") {
  expectExperimentalMapFieldAssignConformance("exe");
}

TEST_CASE("compiles and runs dereferenced experimental map storage references in C++ emitter") {
  expectExperimentalMapStorageReferenceConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped Result.ok experimental map result struct fields in C++ emitter") {
  expectWrappedExperimentalMapResultFieldAssignConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped dereferenced Result.ok experimental map result struct fields in C++ emitter") {
  expectWrappedExperimentalMapResultDerefFieldAssignConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped experimental map struct storage fields in C++ emitter") {
  expectWrappedExperimentalMapStorageFieldConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped dereferenced experimental map struct storage fields in C++ emitter") {
  expectWrappedExperimentalMapStorageDerefFieldConformance("exe");
}

TEST_CASE("rejects canonical namespaced map helpers on borrowed experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalReferenceConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map _ref helpers on borrowed experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalBorrowedRefConformance("exe");
}

TEST_CASE("compiles and runs experimental map methods on bound map values in C++ emitter") {
  expectExperimentalMapMethodConformance("exe");
}

TEST_CASE("compiles and runs borrowed experimental map helpers in C++ emitter") {
  expectExperimentalMapReferenceHelperConformance("exe");
}

TEST_CASE("compiles and runs borrowed experimental map methods in C++ emitter") {
  expectExperimentalMapReferenceMethodConformance("exe");
}

TEST_CASE("compiles and runs experimental map inserts in C++ emitter") {
  expectExperimentalMapInsertConformance("exe");
}

TEST_CASE("compiles and runs experimental map ownership-sensitive values in C++ emitter") {
  expectExperimentalMapOwnershipConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map inserts on explicit experimental map bindings in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalInsertConformance("exe");
}

TEST_CASE("rejects builtin canonical map inserts with pending runtime diagnostic in C++ emitter") {
  expectBuiltinCanonicalMapInsertPendingDiagnostic("exe");
}

TEST_CASE("compiles and runs builtin canonical map first-growth inserts in C++ emitter") {
  expectBuiltinCanonicalMapInsertFirstGrowthConformance("exe");
}

TEST_CASE("compiles and runs builtin canonical map pair-growth inserts in C++ emitter") {
  expectBuiltinCanonicalMapInsertPairGrowthConformance("exe");
}

TEST_CASE("compiles and runs builtin canonical map insert overwrites in C++ emitter") {
  expectBuiltinCanonicalMapInsertOverwriteConformance("exe");
}

TEST_CASE("rejects canonical map constructor ownership growth in C++ emitter") {
  expectCanonicalMapNamespaceOwnershipReject("exe");
}

TEST_CASE("rejects experimental map bracket access in C++ emitter") {
  expectExperimentalMapIndexConformance("exe");
}

TEST_CASE("compiles and runs shared vector conformance harness in C++ emitter") {
  expectSharedVectorConformanceHarness("exe");
}

TEST_CASE("compiles and runs canonical namespaced vector helpers in C++ emitter") {
  expectCanonicalVectorNamespaceConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced vector helpers on explicit Vector bindings in C++ emitter") {
  expectCanonicalVectorNamespaceExplicitVectorBindingConformance("exe");
}

TEST_CASE("compiles and runs stdlib wrapper vector helpers on explicit Vector bindings in C++ emitter") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingConformance("exe");
}

TEST_CASE("rejects stdlib wrapper vector helper explicit Vector mismatch in C++ emitter") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingMismatchReject("exe");
}

TEST_CASE("compiles and runs stdlib wrapper vector constructors on explicit Vector bindings in C++ emitter") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingConformance("exe");
}

TEST_CASE("rejects stdlib wrapper vector constructor explicit Vector mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingMismatchReject("exe");
}

TEST_CASE("compiles and runs stdlib wrapper vector constructors on inferred auto bindings in C++ emitter") {
  expectStdlibWrapperVectorConstructorAutoInferenceConformance("exe");
}

TEST_CASE("rejects stdlib wrapper vector constructor auto inference mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorAutoInferenceMismatchReject("exe");
}

TEST_CASE("rejects stdlib wrapper vector constructor receivers in C++ emitter") {
  expectStdlibWrapperVectorConstructorReceiverConformance("exe");
}

TEST_CASE("rejects stdlib wrapper vector helper receiver mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorHelperReceiverMismatchReject("exe");
}

TEST_CASE("rejects stdlib wrapper vector method receiver mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorMethodReceiverMismatchReject("exe");
}

TEST_CASE("rejects canonical namespaced vector constructor temporaries in C++ emitter") {
  expectCanonicalVectorNamespaceTemporaryReceiverConformance("exe");
}

TEST_CASE("rejects canonical namespaced vector explicit builtin bindings in C++ emitter") {
  expectCanonicalVectorNamespaceExplicitBindingReject("exe");
}

TEST_CASE("rejects canonical namespaced vector named-argument temporaries in C++ emitter") {
  expectCanonicalVectorNamespaceNamedArgsTemporaryReceiverConformance("exe");
}

TEST_CASE("rejects canonical namespaced vector named-argument explicit builtin bindings in C++ emitter") {
  expectCanonicalVectorNamespaceNamedArgsExplicitBindingReject("exe");
}

TEST_CASE("rejects canonical namespaced vector mutators without imported helpers in C++ emitter") {
  expectCanonicalVectorClearImportRequirement("exe");
  expectCanonicalVectorRemoveAtImportRequirement("exe");
  expectCanonicalVectorRemoveSwapImportRequirement("exe");
}

TEST_CASE("compiles and runs bare vector count and capacity through imported stdlib helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(vectorCount<i32>(values), vectorCapacity<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_exe_bare_vector_count_capacity_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_bare_vector_count_capacity_imported_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs bare vector access through imported stdlib helpers in C++ emitter") {
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
  const std::string srcPath = writeTemp("compile_exe_bare_vector_access_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_bare_vector_access_imported_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("rejects bare vector count without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_exe_bare_vector_count_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_exe_bare_vector_count_import_requirement_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects bare vector capacity without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_exe_bare_vector_capacity_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_exe_bare_vector_capacity_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("bare vector mutators reject without imported helpers in C++ emitter") {
  expectBareVectorMutatorImportRequirement("exe", "push", "values, 7i32");
  expectBareVectorMutatorImportRequirement("exe", "pop", "values");
  expectBareVectorMutatorImportRequirement("exe", "reserve", "values, 8i32");
  expectBareVectorMutatorImportRequirement("exe", "clear", "values");
  expectBareVectorMutatorImportRequirement("exe", "remove_at", "values, 1i32");
  expectBareVectorMutatorImportRequirement("exe", "remove_swap", "values, 1i32");
}

TEST_CASE("compiles and runs experimental vector helper runtime contracts in C++ emitter") {
  expectExperimentalVectorRuntimeContracts("exe");
}

TEST_CASE("compiles and runs experimental vector ownership-sensitive helpers in C++ emitter") {
  expectExperimentalVectorOwnershipContracts("exe");
}

TEST_CASE("compiles and runs canonical vector helpers on experimental vector receivers in C++ emitter") {
  expectExperimentalVectorCanonicalHelperRoutingConformance("exe");
}
TEST_CASE("compiles and runs vector pop empty runtime contract in C++ emitter") {
  SUBCASE("call") {
    expectVectorPopEmptyRuntimeContract("exe", false);
  }

  SUBCASE("method") {
    expectVectorPopEmptyRuntimeContract("exe", true);
  }
}

TEST_CASE("compiles and runs vector index runtime contract in C++ emitter") {
  expectVectorIndexRuntimeContract("exe", "access_call");
  expectVectorIndexRuntimeContract("exe", "access_method");
  expectVectorIndexRuntimeContract("exe", "access_bracket");
  expectVectorIndexRuntimeContract("exe", "remove_at_call");
  expectVectorIndexRuntimeContract("exe", "remove_at_method");
  expectVectorIndexRuntimeContract("exe", "remove_swap_call");
  expectVectorIndexRuntimeContract("exe", "remove_swap_method");
}

TEST_CASE("compiles and runs container error contract conformance in C++ emitter") {
  expectContainerErrorConformance("exe");
}

TEST_CASE("compiles and runs checked pointer conformance harness in C++ emitter") {
  expectCheckedPointerHelperSurfaceConformance("exe");
  expectCheckedPointerGrowthConformance("exe");
  expectCheckedPointerOutOfBoundsConformance("exe");
}

TEST_CASE("compiles and runs unchecked pointer conformance harness in C++ emitter") {
  expectUncheckedPointerHelperSurfaceConformance("exe");
  expectUncheckedPointerGrowthConformance("exe");
}

TEST_CASE("compiles with executions using collection arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task([i32] items, [i32] pairs) {
  return(1i32)
}

execute_task([items] array<i32>(1i32, 2i32), [pairs] map<i32, i32>(1i32, 2i32))
)";
  const std::string srcPath = writeTemp("compile_exec_collections.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exec_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compile run rejects execution body arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat([i32] count) {
  return(1i32)
}

execute_repeat(2i32) {
  main(),
  main()
}
)";
  const std::string srcPath = writeTemp("compile_exec_body.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exec_body_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs pointer plus u64 offset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 0u64)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_pointer_plus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs i64 literals") {
  const std::string source = R"(
[return<i64>]
main() {
  return(9i64)
}
)";
  const std::string srcPath = writeTemp("compile_i64_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_i64_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs u64 literals") {
  const std::string source = R"(
[return<u64>]
main() {
  return(10u64)
}
)";
  const std::string srcPath = writeTemp("compile_u64_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_u64_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs assignment operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  value=2i32
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_assign_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs comparison operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32>1i32)
}
)";
  const std::string srcPath = writeTemp("compile_gt_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_gt_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs less_than operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(1i32<2i32)
}
)";
  const std::string srcPath = writeTemp("compile_lt_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_lt_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs greater_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32>=2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ge_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_ge_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs less_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32<=2i32)
}
)";
  const std::string srcPath = writeTemp("compile_le_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_le_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs and operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true&&true)
}
)";
  const std::string srcPath = writeTemp("compile_and_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_and_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs or operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(false||true)
}
)";
  const std::string srcPath = writeTemp("compile_or_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_or_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(!false)
}
)";
  const std::string srcPath = writeTemp("compile_not_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_not_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not operator with parentheses") {
  const std::string source = R"(
[return<bool>]
main() {
  return(!(false))
}
)";
  const std::string srcPath = writeTemp("compile_not_paren.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_not_paren_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs unary minus operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(plus(-value, 5i32))
}
)";
  const std::string srcPath = writeTemp("compile_unary_minus.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_unary_minus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs equality operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32==2i32)
}
)";
  const std::string srcPath = writeTemp("compile_eq_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_eq_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32!=3i32)
}
)";
  const std::string srcPath = writeTemp("compile_neq_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_neq_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_SUITE_END();
