#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles and runs native array literals") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_array_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array access with u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1u64])
}
)";
  const std::string srcPath = writeTemp("compile_native_array_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_array_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array access rejects negative index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  return(plus(100i32, values[-1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_negative.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_array_negative_exe").string();
  const std::string errPath = (testScratchPath("") / "primec_native_array_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("compiles and runs native array unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_unsafe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_array_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array unsafe access with u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_unsafe_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_array_unsafe_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_literal_count_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal_unsafe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_literal_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_count_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal_count_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_literal_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native vector literals") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_vector_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native stdlib namespaced vector builtin aliases") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  /std/collections/vector/push(values, 6i32)
  [i32] countValue{/std/collections/vector/count(values)}
  [i32] capacityValue{/std/collections/vector/capacity(values)}
  [i32] tailValue{/std/collections/vector/at_unsafe(values, 2i32)}
  return(plus(plus(countValue, tailValue), minus(capacityValue, capacityValue)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_namespaced_vector_aliases.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_namespaced_vector_aliases_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native namespaced wrapper string access method chain compatibility fallback") {
  const std::string source = R"(
import /std/collections/*

namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(plus(/std/collections/vector/at(wrapText(), 1i32).tag(),
              /vector/at_unsafe(wrapText(), 0i32).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_namespaced_wrapper_string_access_method_chain_compatibility_fallback.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_namespaced_wrapper_string_access_method_chain_compatibility_fallback_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_namespaced_wrapper_string_access_method_chain_compatibility_fallback_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects native slash-method wrapper string access method chain compatibility fallback") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 1i32))
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(plus(wrapText()./std/collections/vector/at(1i32).tag(),
              wrapText()./vector/at_unsafe(0i32).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_slash_method_wrapper_string_access_method_chain_compatibility_fallback.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_slash_method_wrapper_string_access_method_chain_compatibility_fallback.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("native keeps slash-method wrapper string access i32 diagnostics") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/at(1i32).missing_tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_slash_method_wrapper_string_access_method_chain_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_slash_method_wrapper_string_access_method_chain_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/missing_tag") != std::string::npos);
}

TEST_CASE("rejects native alias slash-method vector access on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(plus(wrapArray()./vector/at(1i32),
              wrapArray()./vector/at_unsafe(0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_alias_slash_vector_access_array_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_alias_slash_vector_access_array_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("compiles and runs native unchecked pointer conformance harness for imported .prime helpers") {
  expectUncheckedPointerHelperSurfaceConformance("native");
  expectUncheckedPointerGrowthConformance("native");
}

TEST_CASE("rejects native array namespaced vector constructor alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/array/vector<i32>(4i32, 5i32)}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_array_namespaced_vector_constructor_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_constructor_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_constructor_alias_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/vector") != std::string::npos);
}

TEST_CASE("rejects native array namespaced vector at alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  [i32] headValue{/array/at(values, 1i32)}
  return(headValue)
}
)";
  const std::string srcPath = writeTemp("compile_native_array_namespaced_vector_at_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_at_alias_out.txt").string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_at_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at") != std::string::npos);
}

TEST_CASE("rejects native array namespaced vector at_unsafe alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  [i32] tailValue{/array/at_unsafe(values, 1i32)}
  return(tailValue)
}
)";
  const std::string srcPath = writeTemp("compile_native_array_namespaced_vector_at_unsafe_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_at_unsafe_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_at_unsafe_alias_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects native wrapper array namespaced vector at alias") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(4i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at(wrapVector(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_wrapper_array_namespaced_vector_at_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_wrapper_array_namespaced_vector_at_alias_out.txt").string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_wrapper_array_namespaced_vector_at_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at") != std::string::npos);
}

TEST_CASE("rejects native wrapper array namespaced vector at_unsafe alias") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(4i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at_unsafe(wrapVector(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_wrapper_array_namespaced_vector_at_unsafe_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_wrapper_array_namespaced_vector_at_unsafe_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_wrapper_array_namespaced_vector_at_unsafe_alias_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects native array namespaced vector count builtin alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/array/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_namespaced_vector_count_builtin_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_count_builtin_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_count_builtin_alias_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("rejects native array namespaced vector count method alias") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(values./array/count(true))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_namespaced_vector_count_method_alias.prime",
                                        source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_count_method_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_count_method_alias_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("rejects native array namespaced vector capacity method alias") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(values./array/capacity(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_array_namespaced_vector_capacity_method_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_capacity_method_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_namespaced_vector_capacity_method_alias_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects native map namespaced count compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_namespaced_count_compatibility_alias_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_map_namespaced_count_compatibility_alias_reject_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_map_namespaced_count_compatibility_alias_reject_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("rejects native map namespaced contains compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [bool] found{/map/contains(values, 1i32)}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_namespaced_contains_compatibility_alias_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_map_namespaced_contains_compatibility_alias_reject_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_map_namespaced_contains_compatibility_alias_reject_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /map/contains") != std::string::npos);
}

TEST_CASE("rejects native map namespaced tryAt compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(/map/tryAt(values, 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_namespaced_try_at_compatibility_alias_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_map_namespaced_try_at_compatibility_alias_reject_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_map_namespaced_try_at_compatibility_alias_reject_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /map/tryAt") != std::string::npos);
}

TEST_CASE("rejects native map namespaced at compatibility alias without explicit alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_namespaced_at_compatibility_alias_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_map_namespaced_at_compatibility_alias_reject_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_map_namespaced_at_compatibility_alias_reject_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("rejects native map namespaced at unsafe compatibility alias without explicit alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{/map/at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_namespaced_at_unsafe_compatibility_alias_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_map_namespaced_at_unsafe_compatibility_alias_reject_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_map_namespaced_at_unsafe_compatibility_alias_reject_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_SUITE_END();
#endif
