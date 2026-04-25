#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("vm keeps canonical map unknown-target diagnostics on wrapper-returned map access count shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()[1i32].count())
}
)";
  const std::string srcPath =
      writeTemp("vm_user_string_count_method_shadow_wrapper_canonical_map_access_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_user_string_count_method_shadow_wrapper_canonical_map_access_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("vm keeps builtin string count on direct wrapper-returned canonical map access") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(/std/collections/map/at(wrapMap(), 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_direct_wrapper_canonical_map_access_count_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_direct_wrapper_canonical_map_access_count_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("vm keeps wrapper-returned canonical map method access string receiver typing") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(wrapMap().at(1i32).count(),
              wrapMap().at_unsafe(1i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_canonical_map_method_access_count_diag.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 182);
}

TEST_CASE("vm keeps builtin string count on wrapper-returned slash-method map access") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapMap()./std/collections/map/at(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_slash_method_map_access_count_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_slash_method_map_access_count_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("rejects vm canonical vector access string literals") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"raw_utf8)}
  return(count(/std/collections/vector/at(values, 0i32)))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_vector_access_builtin_string_count_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_vector_access_builtin_string_count_shadow.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("count requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("rejects vm canonical vector unsafe access count shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(count(/std/collections/vector/at_unsafe(values, 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_vector_access_unsafe_count_shadow_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_vector_access_unsafe_count_shadow_reject.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("vm keeps primitive diagnostics for canonical vector method access count shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"raw_utf8)}
  return(values.at(0i32).count())
}
)";
  const std::string srcPath = writeTemp("vm_canonical_vector_method_access_builtin_string_count_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_vector_method_access_builtin_string_count_shadow.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("vm keeps builtin string count on canonical vector unsafe method access shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(values.at_unsafe(0i32).count())
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_vector_unsafe_method_access_count_shadow_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_vector_unsafe_method_access_count_shadow_reject.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 91);
}

TEST_CASE("rejects vm slash-method vector access string count fallback") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[return<i32>]
/std/collections/vector/at_unsafe([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"raw_utf8)}
  return(plus(values./vector/at(0i32).count(),
              values./std/collections/vector/at_unsafe(0i32).count()))
}
)";
  const std::string srcPath = writeTemp("vm_slash_method_vector_access_string_count_fallback.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_slash_method_vector_access_string_count_fallback.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("vm keeps slash-method vector access unknown-method diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(plus(values./vector/at(0i32).count(),
              values./std/collections/vector/at_unsafe(0i32).count()))
}
)";
  const std::string srcPath = writeTemp("vm_slash_method_vector_access_primitive_count_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_slash_method_vector_access_primitive_count_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects vm wrapper-returned vector access string literals") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[return<i32>]
/std/collections/vector/at_unsafe([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<vector<string>>]
wrapValues() {
  return(vector<string>("hello"raw_utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(/vector/at(wrapValues(), 0i32)),
              wrapValues().at_unsafe(0i32).count()))
}
)";
  const std::string srcPath = writeTemp("vm_wrapper_vector_access_string_count_fallback.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_vector_access_string_count_fallback.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("vm keeps wrapper-returned vector access primitive count diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(/vector/at(wrapValues(), 0i32)),
              wrapValues().at_unsafe(0i32).count()))
}
)";
  const std::string srcPath = writeTemp("vm_wrapper_vector_access_primitive_count_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_vector_access_primitive_count_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("runs vm with user vector count method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(97i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_count_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm canonical slash vector count same-path helper on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(87i32)
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_slash_vector_count_map_same_path_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_slash_vector_count_map_same_path_helper.out.txt")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("rejects vm wrapper-returned canonical vector count slash-method on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_slash_vector_count_map_no_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_slash_vector_count_map_no_helper.out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("rejects vm wrapper-returned canonical vector capacity slash-method on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_slash_vector_capacity_map_no_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_slash_vector_capacity_map_no_helper.out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("capacity requires vector target") !=
        std::string::npos);
}

TEST_CASE("runs vm with user vector capacity method shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_capacity_method_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_user_vector_capacity_method_shadow.err").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("name=capacity") != std::string::npos);
}

TEST_CASE("rejects vm user vector count call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(97i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_count_call_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_user_vector_count_call_shadow_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects vm user vector capacity call shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_capacity_call_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_user_vector_capacity_call_shadow_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("runs vm user array capacity call shadow") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(66i32)
}

  [return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_capacity_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 66);
}

TEST_CASE("runs vm with user array capacity method shadow") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(65i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("vm_user_array_capacity_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 65);
}

TEST_CASE("keeps builtin array access on vm user array at call shadow") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(61i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
  )";
  const std::string srcPath = writeTemp("vm_user_array_at_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 61);
}

TEST_CASE("runs vm with user array at method shadow") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(63i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_at_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 63);
}

TEST_CASE("keeps builtin array access on vm user array at_unsafe call shadow") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(85i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
  )";
  const std::string srcPath = writeTemp("vm_user_array_at_unsafe_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 85);
}

TEST_CASE("runs vm with user array at_unsafe method shadow") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(86i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_at_unsafe_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 86);
}

TEST_CASE("runs vm with user map at_unsafe call shadow") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(62i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_at_unsafe_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user map at_unsafe method shadow") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(64i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_at_unsafe_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user map at call shadow") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(67i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_at_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_SUITE_END();
