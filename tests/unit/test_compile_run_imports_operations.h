#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.imports");

TEST_CASE("compiles and runs collection literals in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>{1i32, 2i32, 3i32}
  map<i32, i32>{1i32=10i32, 2i32=20i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_collections_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs string-keyed map literals in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  map<string, i32>{"a"utf8=1i32, "b"utf8=2i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_collections_string_map_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs string-keyed map literals with bracket sugar in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  map<string, i32>["a"=1i32, "b"=2i32]
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map_brackets.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_collections_string_map_brackets_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs shared map conformance harness in C++ emitter") {
  expectSharedMapConformanceHarness("exe");
}

TEST_CASE("compiles and runs canonical namespaced map helpers on experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalValueConformance("exe");
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

TEST_CASE("compiles and runs experimental map struct fields in C++ emitter") {
  expectExperimentalMapStructFieldConformance("exe");
}

TEST_CASE("compiles and runs experimental map method parameters in C++ emitter") {
  expectExperimentalMapMethodParameterConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map parameters in C++ emitter") {
  expectInferredExperimentalMapParameterConformance("exe");
}

TEST_CASE("compiles and runs experimental map field assignments in C++ emitter") {
  expectExperimentalMapFieldAssignConformance("exe");
}

TEST_CASE("rejects canonical namespaced map helpers on borrowed experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalReferenceConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map _ref helpers on borrowed experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalBorrowedRefConformance("exe");
}

TEST_CASE("compiles and runs experimental map methods in C++ emitter") {
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

TEST_CASE("compiles and runs experimental map bracket access in C++ emitter") {
  expectExperimentalMapIndexConformance("exe");
}

TEST_CASE("compiles and runs shared vector conformance harness in C++ emitter") {
  expectSharedVectorConformanceHarness("exe");
}

TEST_CASE("compiles and runs experimental vector helper runtime contracts in C++ emitter") {
  expectExperimentalVectorRuntimeContracts("exe");
}

TEST_CASE("rejects experimental vector ownership-sensitive helpers in C++ emitter") {
  expectExperimentalVectorOwnershipRejects("exe");
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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exec_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects execution body arguments") {
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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exec_body_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_plus_u64_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_i64_literal_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_u64_literal_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_assign_op_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_gt_op_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_lt_op_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ge_op_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_le_op_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_and_op_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_or_op_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_not_op_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_not_paren_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_unary_minus_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_eq_op_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_neq_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_SUITE_END();
