#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter compiles canonical direct-call map count string receivers") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, string>] values) {
  return(41i32)
}

[return<string>]
/std/collections/map/count([map<i32, string>] values) {
  return("hello"utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(/std/collections/map/count(values).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_direct_call_string_receiver.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_stdlib_namespaced_map_count_direct_call_string_receiver_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_namespaced_map_count_direct_call_string_receiver.err")
          .string();
  const std::string runtimeErrPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_namespaced_map_count_direct_call_string_receiver_runtime.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(runCommand(exePath + " > /dev/null 2> " + runtimeErrPath) == 1);
  CHECK(readFile(runtimeErrPath).find("invalid string index in IR") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical diagnostics on direct-call map count receivers") {
  const std::string source = R"(
[return<string>]
/map/count([map<i32, string>] values) {
  return("hello"utf8)
}

[return<int>]
/std/collections/map/count([map<i32, string>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(/std/collections/map/count(values).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_direct_call_string_receiver_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_namespaced_map_count_direct_call_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("rejects stdlib namespaced vector capacity on map target in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_capacity_map_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_stdlib_namespaced_vector_capacity_map_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("C++ emitter compiles user wrapper temporary count capacity shadow precedence") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/count([map<i32, i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(78i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(79i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(count(wrapMap()), wrapMap().count()),
              plus(plus(count(wrapVector()), wrapVector().count()),
                   plus(capacity(wrapVector()), wrapVector().capacity()))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_user_wrapper_temp_count_capacity_shadow_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_user_wrapper_temp_count_capacity_shadow_precedence_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 163);
}

TEST_CASE("rejects user wrapper temporary count capacity shadow value mismatch in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<bool>]
/map/count([map<i32, i32>] values) {
  return(true)
}

[effects(heap_alloc), return<bool>]
/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
/vector/capacity([vector<i32>] values) {
  return(true)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{count(wrapMap())}
  [i32] mapMethod{wrapMap().count()}
  [i32] vectorCountCall{count(wrapVector())}
  [i32] vectorCountMethod{wrapVector().count()}
  [i32] vectorCapacityCall{capacity(wrapVector())}
  [i32] vectorCapacityMethod{wrapVector().capacity()}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_user_wrapper_temp_count_capacity_shadow_value_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_user_wrapper_temp_count_capacity_shadow_value_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter keeps wrapper count capacity builtin fallback") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(count(wrapMap()), wrapMap().count()),
              plus(capacity(wrapVector()), wrapVector().capacity())))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_count_capacity_builtin_fallback.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_count_capacity_builtin_fallback.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects namespaced wrapper vector count map target in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_wrapper_vector_count_map_target_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_namespaced_wrapper_vector_count_map_target_reject_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects namespaced wrapper vector count vector target without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_wrapper_vector_count_vector_target_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_namespaced_wrapper_vector_count_vector_target_reject_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("C++ emitter rejects array namespaced wrapper count alias") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(/array/count(wrapMap()),
              /std/collections/vector/capacity(wrapVector())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_array_namespaced_wrapper_count_capacity_builtin_fallback.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_wrapper_count_capacity_builtin_fallback_err.txt")
          .string();
  const std::string cppOutPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_wrapper_count_capacity_builtin_fallback.cpp")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + cppOutPath + " --entry /main > " + errPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical direct-call vector count same-path helper on map receiver") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(41i32)
}

[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_count_map_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_count_map_same_path_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 41);
}

TEST_CASE("C++ emitter keeps alias direct-call vector count same-path helper on map receiver") {
  const std::string source = R"(
[return<int>]
/vector/count([map<i32, i32>] values) {
  return(44i32)
}

[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_vector_count_map_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_alias_vector_count_map_same_path_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 44);
}

TEST_CASE("rejects alias direct-call vector count on map receiver without helper in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_vector_count_map_unknown_target.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_alias_vector_count_map_unknown_target_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects array namespaced wrapper capacity builtin fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/capacity(wrapVector()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_wrapper_capacity_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_wrapper_capacity_reject_out.txt")
          .string();
  const std::string cppOutPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_wrapper_capacity_reject.cpp").string();
  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + cppOutPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects namespaced count capacity method chain compatibility fallback in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [string] text{"abc"raw_utf8}
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(plus(/std/collections/vector/count(text).tag(),
              /vector/capacity(values).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_count_capacity_method_chain_compatibility_fallback_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_namespaced_count_capacity_method_chain_compatibility_fallback_reject_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(outPath).empty());
}

TEST_CASE("C++ emitter keeps canonical slash-method vector count same-path helper on string receiver") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
/std/collections/vector/count([string] values) {
  return(91i32)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_count_string_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_count_string_same_path_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 91);
}

TEST_CASE("rejects canonical slash-method vector count on string receiver without helper in C++ emitter") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_count_string_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_count_string_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /string/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical slash-method vector count same-path helper on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(97i32)
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_count_map_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_count_map_same_path_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 97);
}

TEST_CASE("rejects canonical slash-method vector count on map receiver without helper in C++ emitter") {
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
      writeTemp("compile_cpp_canonical_slash_vector_count_map_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_count_map_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical slash-method vector count same-path helper on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(98i32)
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_count_array_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_count_array_same_path_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 98);
}

TEST_CASE("rejects canonical slash-method vector count on array receiver without helper in C++ emitter") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_count_array_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_count_array_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("C++ emitter rejects duplicate local canonical slash-method vector count overloads") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(31i32)
}

[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(32i32)
}

[return<int>]
/std/collections/vector/count([string] values) {
  return(33i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  [string] text{"abc"raw_utf8}
  return(plus(plus(values./std/collections/vector/count(),
                    items./std/collections/vector/count()),
              text./std/collections/vector/count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_count_same_path.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_count_same_path_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("duplicate definition: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("C++ emitter rejects local canonical slash-method vector count on map receiver before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_count_map_no_helper.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_count_map_no_helper.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /map/count") !=
        std::string::npos);
}

TEST_SUITE_END();
