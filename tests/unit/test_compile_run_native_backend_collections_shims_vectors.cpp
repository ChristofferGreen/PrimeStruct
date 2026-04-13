#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles and runs native stdlib collection shim access helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(7i32)}
  [map<i32, i32>] pairs{mapSingle<i32, i32>(3i32, 9i32)}
  [i32] a{vectorAt<i32>(values, 0i32)}
  [i32] b{vectorAtUnsafe<i32>(values, 0i32)}
  [i32] c{mapAt<i32, i32>(pairs, 3i32)}
  [i32] d{mapAtUnsafe<i32, i32>(pairs, 3i32)}
  return(plus(plus(a, b), plus(c, d)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_access.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_access_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 32);
}

TEST_CASE("compiles and runs native stdlib collection shim capacity helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorSingle<i32>(7i32)}
  reserve(values, 4i32)
  return(vectorCapacity<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_capacity.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_capacity_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native stdlib collection shim vector capacity") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorSingle<i32>(9i32)}
  vectorReserve<i32>(values, 5i32)
  return(plus(vectorCapacity<i32>(values), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_capacity.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_capacity_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("rejects native stdlib collection shim vector capacity type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(9i32)}
  return(vectorCapacity<bool>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_capacity_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector count") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(plus(vectorCount<i32>(values), 10i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_count_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("rejects native stdlib collection shim vector count type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(vectorCount<bool>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_count_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector at") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(plus(vectorAt<i32>(values, 1i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_at.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_at_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("rejects native stdlib collection shim vector at type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(vectorAt<bool>(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_at_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector at unsafe") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(plus(vectorAtUnsafe<i32>(values, 2i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_at_unsafe.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_vector_at_unsafe_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("rejects native stdlib collection shim vector at unsafe type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  return(vectorAtUnsafe<bool>(values, 2i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_vector_at_unsafe_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector push") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorNew<i32>()}
  vectorPush<i32>(values, 5i32)
  vectorPush<i32>(values, 8i32)
  return(plus(vectorAt<i32>(values, 1i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_push.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_push_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("rejects native stdlib collection shim vector push type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorNew<i32>()}
  vectorPush<bool>(values, true)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_push_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector pop") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorPop<i32>(values)
  return(plus(vectorAt<i32>(values, 1i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_pop.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_pop_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("rejects native stdlib collection shim vector pop type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorPop<bool>(values)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_pop_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector reserve") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorNew<i32>()}
  vectorReserve<i32>(values, 6i32)
  return(plus(vectorCapacity<i32>(values), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_reserve.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_reserve_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("rejects native stdlib collection shim vector reserve type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorNew<i32>()}
  vectorReserve<bool>(values, 6i32)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_vector_reserve_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector clear") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorClear<i32>(values)
  return(plus(vectorCount<i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_clear.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_clear_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects native stdlib collection shim vector clear type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorClear<bool>(values)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_clear_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector remove at") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorRemoveAt<i32>(values, 1i32)
  return(plus(vectorAt<i32>(values, 1i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_remove_at.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_vector_remove_at_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("rejects native stdlib collection shim vector remove at type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorRemoveAt<bool>(values, 1i32)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_vector_remove_at_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector remove swap") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorRemoveSwap<i32>(values, 0i32)
  return(plus(vectorAt<i32>(values, 0i32), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_remove_swap.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_vector_remove_swap_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("rejects native stdlib collection shim vector remove swap type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorTriple<i32>(2i32, 4i32, 6i32)}
  vectorRemoveSwap<bool>(values, 0i32)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_vector_remove_swap_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector mutators") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vectorNew<i32>()}
  vectorReserve<i32>(values, 4i32)
  vectorPush<i32>(values, 11i32)
  vectorPush<i32>(values, 22i32)
  vectorPush<i32>(values, 33i32)
  [i32 mut] snapshot{plus(vectorCount<i32>(values), vectorCapacity<i32>(values))}
  vectorPop<i32>(values)
  vectorRemoveAt<i32>(values, 0i32)
  vectorPush<i32>(values, 44i32)
  vectorRemoveSwap<i32>(values, 0i32)
  assign(snapshot, plus(snapshot, vectorCount<i32>(values)))
  vectorClear<i32>(values)
  return(plus(snapshot, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_mutators.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_mutators_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs native bare vector capacity through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  [i32] a{vectorCapacity<i32>(values)}
  return(plus(a, vectorCapacity<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_capacity_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_capacity_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("rejects native bare vector capacity without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_capacity_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_capacity_import_requirement_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("compiles and runs native bare vector capacity method without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_capacity_method_import_requirement.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_capacity_method_import_requirement_exe")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("rejects native wrapper temporary vector capacity method without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_vector_capacity_method_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_vector_capacity_method_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE("compiles and runs native bare vector capacity after pop through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  vectorPop<i32>(values)
  return(vectorCapacity<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_capacity_after_pop.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_capacity_after_pop_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("native bare vector mutators compile without imported helpers") {
  expectBareVectorMutatorImportRequirement("native", "push", "values, 7i32");
  expectBareVectorMutatorImportRequirement("native", "pop", "values");
  expectBareVectorMutatorImportRequirement("native", "reserve", "values, 8i32");
  expectBareVectorMutatorImportRequirement("native", "clear", "values");
  expectBareVectorMutatorImportRequirement("native", "remove_at", "values, 1i32");
  expectBareVectorMutatorImportRequirement("native", "remove_swap", "values, 1i32");
}

TEST_SUITE_END();
#endif
