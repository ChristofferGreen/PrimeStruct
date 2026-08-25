#include "../test_compile_run_helpers.h"
#include "../test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects local canonical slash-method vector count on string receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_count_string_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_count_string_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps local canonical slash-method vector count same-path helper on string receiver") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([string] values) {
  return(81i32)
}

[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_count_string_same_path_helper.prime",
                source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 81);
}

TEST_CASE("C++ emitter keeps local canonical slash-method vector count same-path helper on map receiver") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(82i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_count_map_same_path_helper.prime",
                source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 82);
}

TEST_CASE("C++ emitter keeps local canonical slash-method vector count same-path helper on array receiver") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(83i32)
}

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_count_array_same_path_helper.prime",
                source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 83);
}

TEST_CASE("rejects local alias slash-method vector count on string receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_alias_slash_vector_count_string_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_alias_slash_vector_count_string_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects local alias slash-method vector count on array receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_alias_slash_vector_count_array_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_alias_slash_vector_count_array_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter keeps rooted vector count same-path helper on string receiver") {
  const std::string source = R"(
[return<int>]
/vector/count([string] values) {
  return(96i32)
}

[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_alias_slash_vector_count_string_same_path_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 96);
}

TEST_CASE("C++ emitter rejects local alias slash-method vector count same-path helper on map receiver") {
  const std::string source = R"(
[return<int>]
/vector/count([map<i32, i32>] values) {
  return(97i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_alias_slash_vector_count_map_same_path_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_alias_slash_vector_count_map_same_path_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter keeps rooted vector count same-path helper on array receiver") {
  const std::string source = R"(
[return<int>]
/vector/count([array<i32>] values) {
  return(98i32)
}

[return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_alias_slash_vector_count_array_same_path_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 98);
}

TEST_CASE("C++ emitter keeps rooted wrapper vector count same-path helper on string receiver") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
/vector/count([string] values) {
  return(96i32)
}

[return<int>]
main() {
  return(wrapText()./vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_slash_vector_count_string_same_path_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 96);
}

TEST_CASE("C++ emitter rejects alias slash-method vector count same-path helper on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/vector/count([map<i32, i32>] values) {
  return(97i32)
}

[return<int>]
main() {
  return(wrapMap()./vector/count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_alias_slash_vector_count_map_same_path_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_compile_cpp_alias_slash_vector_count_map_same_path_helper_repin.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical slash-method vector count same-path helper on map receiver") {
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
      writeTemp("compile_cpp_canonical_slash_vector_count_map_same_path_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_count_map_same_path_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper-returned canonical vector capacity slash-method on map receiver in C++ emitter") {
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
      writeTemp("compile_cpp_canonical_slash_vector_capacity_map_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_capacity_map_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") !=
        std::string::npos);
}

TEST_CASE("rejects alias slash-method vector count on map receiver in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./vector/count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_alias_slash_vector_count_map_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_compile_cpp_alias_slash_vector_count_map_deleted_stub_exe_repin.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps rooted wrapper vector count same-path helper on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
/vector/count([array<i32>] values) {
  return(98i32)
}

[return<int>]
main() {
  return(wrapArray()./vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_slash_vector_count_array_same_path_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 98);
}

TEST_CASE("C++ emitter keeps canonical slash-method vector count same-path helper on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(88i32)
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_count_array_same_path_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 88);
}

TEST_CASE("C++ emitter keeps canonical slash-method vector count same-path helper on string receiver") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
/std/collections/vector/count([string] values) {
  return(89i32)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_count_string_same_path_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 89);
}

TEST_CASE("rejects wrapper-returned canonical vector capacity slash-method on array receiver in C++ emitter") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_capacity_array_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_capacity_array_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") !=
        std::string::npos);
}

TEST_CASE("rejects alias slash-method vector count on array receiver with rooted target in C++ emitter") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./vector/count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_alias_slash_vector_count_array_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_compile_cpp_alias_slash_vector_count_array_deleted_stub_exe_repin.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical direct-call vector count same-path helper on string receiver") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
/std/collections/vector/count([string] values) {
  return(92i32)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapText()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_vector_count_string_same_path_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 92);
}

TEST_CASE("rejects canonical direct-call vector count on string receiver in C++ emitter") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapText()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_vector_count_string_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_vector_count_string_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps alias direct-call vector count same-path helper on string receiver") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
/vector/count([string] values) {
  return(95i32)
}

[return<int>]
main() {
  return(/vector/count(wrapText()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_direct_vector_count_string_same_path_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 95);
}

TEST_CASE("rejects alias direct-call vector count on string receiver in C++ emitter") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(/vector/count(wrapText()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_direct_vector_count_string_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_direct_vector_count_string_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps alias direct-call vector count same-path helper on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
/vector/count([array<i32>] values) {
  return(99i32)
}

[return<int>]
main() {
  return(/vector/count(wrapArray()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_direct_vector_count_array_same_path_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 99);
}

TEST_CASE("rejects alias direct-call vector count on array receiver in C++ emitter") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(/vector/count(wrapArray()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_direct_vector_count_array_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_direct_vector_count_array_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical direct-call vector count same-path helper on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(93i32)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapArray()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_vector_count_array_same_path_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  // TODO-4805 (fixed): the direct call to the user's same-path
  // /std/collections/vector/count([array<i32>]) shadow now correctly
  // dispatches on an array<i32> helper-return receiver, matching the
  // sibling string-receiver case above.
  CHECK(runCommand(compileCmd) == 93);
}

TEST_CASE("rejects canonical direct-call vector count on array receiver in C++ emitter") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapArray()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_vector_count_array_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_vector_count_array_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects namespaced access method chain compatibility fallback in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(7i32, 8i32)}
  return(plus(/std/collections/vector/at(values, 0i32).tag(),
              /vector/at_unsafe(values, 1i32).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_access_method_chain_compatibility_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_namespaced_access_method_chain_compatibility_fallback_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects namespaced wrapper string access method chain compatibility fallback in C++ emitter") {
  const std::string source = R"(
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
      writeTemp("compile_cpp_namespaced_wrapper_string_access_method_chain_compatibility_fallback.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_namespaced_wrapper_string_access_method_chain_compatibility_fallback_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at_unsafe") != std::string::npos);
}

TEST_SUITE_END();
