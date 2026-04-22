#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("runs vm with numeric array literals") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  print_line(values.count())
  print_line(values[1i64])
  print_line(values[2u64])
  return(values[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_array_literals.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_array_literals_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(outPath) == "3\n7\n9\n");
}

TEST_CASE("runs vm with numeric vector literals") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(io_out, heap_alloc)]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  print_line(vectorCount<i32>(values))
  print_line(vectorAt<i32>(values, 1i32))
  print_line(vectorAt<i32>(values, 2i32))
  return(vectorAt<i32>(values, 0i32))
}
)";
  const std::string srcPath = writeTemp("vm_vector_literals.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_literals_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(outPath) == "3\n7\n9\n");
}

TEST_CASE("runs vm with stdlib namespaced vector builtin aliases") {
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
  const std::string srcPath = writeTemp("vm_stdlib_namespaced_vector_aliases.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("rejects vm namespaced wrapper string access method chain compatibility fallback") {
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
      writeTemp("vm_namespaced_wrapper_string_access_method_chain_compatibility_fallback.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_namespaced_wrapper_string_access_method_chain_compatibility_fallback.err")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/vector/at__t") != std::string::npos);
}

TEST_CASE("rejects vm slash-method wrapper string access method chain compatibility fallback") {
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
      writeTemp("vm_slash_method_wrapper_string_access_method_chain_compatibility_fallback.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_slash_method_wrapper_string_access_method_chain_compatibility_fallback.out")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("argument type mismatch for /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("vm keeps slash-method wrapper string access i32 diagnostics") {
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
      writeTemp("vm_slash_method_wrapper_string_access_method_chain_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_slash_method_wrapper_string_access_method_chain_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/missing_tag") != std::string::npos);
}

TEST_CASE("rejects vm array namespaced vector constructor alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/array/vector<i32>(4i32, 5i32)}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_array_namespaced_vector_constructor_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_array_namespaced_vector_constructor_alias_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/vector") != std::string::npos);
}

TEST_CASE("rejects vm array namespaced vector at alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  [i32] headValue{/array/at(values, 0i32)}
  return(headValue)
}
)";
  const std::string srcPath = writeTemp("vm_array_namespaced_vector_at_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_array_namespaced_vector_at_alias_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at") != std::string::npos);
}

TEST_CASE("rejects vm array namespaced vector at_unsafe alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  [i32] tailValue{/array/at_unsafe(values, 1i32)}
  return(tailValue)
}
)";
  const std::string srcPath = writeTemp("vm_array_namespaced_vector_at_unsafe_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_array_namespaced_vector_at_unsafe_alias_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects vm wrapper array namespaced vector at alias") {
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
  const std::string srcPath = writeTemp("vm_wrapper_array_namespaced_vector_at_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_wrapper_array_namespaced_vector_at_alias_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at") != std::string::npos);
}

TEST_CASE("rejects vm wrapper array namespaced vector at_unsafe alias") {
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
  const std::string srcPath = writeTemp("vm_wrapper_array_namespaced_vector_at_unsafe_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_wrapper_array_namespaced_vector_at_unsafe_alias_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects vm array namespaced vector count builtin alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/array/count(values))
}
)";
  const std::string srcPath = writeTemp("vm_array_namespaced_vector_count_builtin_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_array_namespaced_vector_count_builtin_alias_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("rejects vm array namespaced vector count method alias") {
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
  const std::string srcPath = writeTemp("vm_array_namespaced_vector_count_method_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_array_namespaced_vector_count_method_alias_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("rejects vm array namespaced vector capacity method alias") {
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
  const std::string srcPath = writeTemp("vm_array_namespaced_vector_capacity_method_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_array_namespaced_vector_capacity_method_alias_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects vm map namespaced count compatibility alias") {
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
  const std::string srcPath = writeTemp("vm_map_namespaced_count_compatibility_alias_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_map_namespaced_count_compatibility_alias_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("rejects vm map namespaced contains compatibility alias") {
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
  const std::string srcPath = writeTemp("vm_map_namespaced_contains_compatibility_alias_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_map_namespaced_contains_compatibility_alias_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /map/contains") != std::string::npos);
}

TEST_CASE("rejects vm map namespaced tryAt compatibility alias") {
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
  const std::string srcPath = writeTemp("vm_map_namespaced_try_at_compatibility_alias_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_map_namespaced_try_at_compatibility_alias_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /map/tryAt") != std::string::npos);
}

TEST_CASE("runs vm unchecked pointer conformance harness for imported .prime helpers") {
  expectUncheckedPointerHelperSurfaceConformance("vm");
  expectUncheckedPointerGrowthConformance("vm");
}

TEST_CASE("rejects vm map namespaced at compatibility alias without explicit alias") {
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
  const std::string srcPath = writeTemp("vm_map_namespaced_at_compatibility_alias_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_map_namespaced_at_compatibility_alias_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("rejects vm map namespaced at unsafe compatibility alias without explicit alias") {
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
      writeTemp("vm_map_namespaced_at_unsafe_compatibility_alias_reject.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_map_namespaced_at_unsafe_compatibility_alias_reject_out.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("runs vm explicit map helper count/contains/tryAt through same-path aliases while direct access stays builtin") {
  const std::string source = R"(
import /std/collections/*

[effects(io_err)]
unexpectedDirectMapAliasTryAt([ContainerError] err) {}

[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(70i32)
}

[effects(heap_alloc), return<bool>]
/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(80i32))
}

[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(60i32)
}

[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(30i32)
}

[effects(heap_alloc), return<int> on_error<ContainerError, /unexpectedDirectMapAliasTryAt>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [i32] countValue{/map/count(values)}
  [bool] containsValue{/map/contains(values, 2i32)}
  [i32] tryValue{try(/map/tryAt(values, 2i32))}
  [i32] atValue{/map/at(values, 1i32)}
  [i32] unsafeValue{/map/at_unsafe(values, 2i32)}
  [i32] containsScore{if(containsValue, then(){ 1i32 }, else(){ 3i32 })}
  return(plus(plus(countValue, tryValue),
              plus(atValue, plus(unsafeValue, containsScore))))
}
)";
  const std::string srcPath = writeTemp("vm_direct_map_alias_helper_same_path_precedence.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_direct_map_alias_helper_same_path_precedence_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 162);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("rejects vm explicit canonical map helper overrides in expressions during lowering") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(io_err)]
unexpectedCanonicalMapTryAt([i32] err) {}

[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(70i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<Result<i32, i32>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(80i32))
}

[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(60i32)
}

[effects(heap_alloc), return<Marker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(Marker(30i32))
}

[effects(heap_alloc), return<int> on_error<i32, /unexpectedCanonicalMapTryAt>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [i32] countValue{/std/collections/map/count(values)}
  [bool] containsValue{/std/collections/map/contains(values, 2i32)}
  [i32] tryValue{try(/std/collections/map/tryAt(values, 2i32))}
  [i32] atValue{/std/collections/map/at(values, 1i32)}
  [i32] unsafeValue{/std/collections/map/at_unsafe(values, 2i32).tag()}
  [i32] containsScore{if(containsValue, then(){ 1i32 }, else(){ 3i32 })}
  return(plus(plus(countValue, tryValue),
              plus(atValue, plus(unsafeValue, containsScore))))
}
)";
  const std::string srcPath = writeTemp("vm_direct_canonical_map_helper_same_path_precedence.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_direct_canonical_map_helper_same_path_precedence_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("VM lowering error") != std::string::npos);
  CHECK(readFile(outPath).find("call=/std/collections/map/at") != std::string::npos);
}

TEST_CASE("runs vm stdlib namespaced map helpers on canonical map references") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  [i32] c{/std/collections/map/count<i32, i32>(ref)}
  [i32] first{/std/collections/map/at<i32, i32>(ref, 1i32)}
  [i32] second{/std/collections/map/at_unsafe<i32, i32>(ref, 2i32)}
  return(plus(c, plus(first, second)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_map_reference_helpers.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_map_reference_helpers_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 11);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("runs vm canonical map method with slash return type receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(73i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
makeValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(makeValues().count())
}
)";
  const std::string srcPath = writeTemp("vm_canonical_map_method_slash_return_type_receiver.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_map_method_slash_return_type_receiver_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 73);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("runs vm canonical map access direct calls on wrapper slash return receiver while method sugar stays builtin") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(42i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(at(wrapValues(), 1i32), wrapValues().at(1i32)),
              plus(at_unsafe(wrapValues(), 1i32), wrapValues().at_unsafe(1i32))))
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_map_access_helpers_wrapper_slash_return_receiver.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_map_access_helpers_wrapper_slash_return_receiver_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 91);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("rejects vm canonical map access helper key mismatch on wrapper slash return receiver") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().at(true))
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_map_access_helper_key_mismatch_wrapper_slash_return_receiver.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_map_access_helper_key_mismatch_wrapper_slash_return_receiver_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("Semantic error: argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("runs vm explicit canonical map typed bindings with builtin helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(plus(count(values), at(values, 1i32)), at_unsafe(values, 2i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_explicit_canonical_map_typed_binding_builtin_helpers.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_explicit_canonical_map_typed_binding_builtin_helpers_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 11);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("rejects vm explicit canonical map typed binding key mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, true))
}
)";
  const std::string srcPath =
      writeTemp("vm_explicit_canonical_map_typed_binding_builtin_helpers_diag.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_explicit_canonical_map_typed_binding_builtin_helpers_diag_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("Semantic error: at requires map key type i32") !=
        std::string::npos);
}

TEST_CASE("rejects vm stdlib namespaced map constructor alias fallback without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/map<T, U>([T] key, [U] value) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/map<i32, i32>(1i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_namespaced_map_constructor_alias_fallback.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_namespaced_map_constructor_alias_fallback_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/map") != std::string::npos);
}

TEST_CASE("rejects vm stdlib namespaced map at fallback without import") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(66i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/at(wrapMap(), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_namespaced_map_at_alias_fallback.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_namespaced_map_at_alias_fallback_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("rejects vm stdlib namespaced map at unsafe fallback without import") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(67i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/at_unsafe(wrapMap(), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_namespaced_map_at_unsafe_alias_fallback.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_namespaced_map_at_unsafe_alias_fallback_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("keeps vm bare map count builtin even with canonical helper present") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_map_unnamespaced_count_builtin_fallback_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_map_unnamespaced_count_builtin_fallback_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("rejects vm bare map count without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_map_unnamespaced_count_builtin_fallback_no_canonical_reject.prime",
                                        source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_map_unnamespaced_count_builtin_fallback_no_canonical_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("keeps vm bare map at builtin even with canonical helper present") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_bare_map_at_with_canonical_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_bare_map_at_with_canonical_helper_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(outPath).empty());
}

TEST_SUITE_END();
