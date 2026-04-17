#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter rejects local canonical slash-method vector capacity same-path helper on string receiver") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/capacity([string] values) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_capacity_string_same_path_helper.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_capacity_string_same_path_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /string/capacity") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects local canonical slash-method vector capacity same-path helper on map receiver") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(92i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_capacity_map_same_path_helper.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_capacity_map_same_path_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /map/capacity") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects local canonical slash-method vector capacity same-path helper on array receiver") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/capacity([array<i32>] values) {
  return(93i32)
}

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_capacity_array_same_path_helper.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_capacity_array_same_path_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /array/capacity") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps alias direct-call vector capacity same-path helper on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32))
}

[return<int>]
/vector/capacity([array<i32>] values) {
  return(95i32)
}

[return<int>]
main() {
  return(/vector/capacity(wrapArray()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_direct_vector_capacity_array_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_alias_direct_vector_capacity_array_same_path_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 95);
}

TEST_CASE("rejects canonical direct-call vector capacity on map receiver without helper in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(/std/collections/vector/capacity(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_vector_capacity_map_unknown_target.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_vector_capacity_map_unknown_target.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("rejects alias direct-call vector capacity on map receiver without helper in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(/vector/capacity(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_direct_vector_capacity_map_unknown_target.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_direct_vector_capacity_map_unknown_target.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("rejects alias direct-call vector capacity on array receiver without helper in C++ emitter") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(/vector/capacity(wrapArray()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_direct_vector_capacity_array_unknown_target.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_direct_vector_capacity_array_unknown_target.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("rejects canonical direct-call vector capacity builtin fallback on map receiver in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(/std/collections/vector/capacity(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_vector_capacity_map_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_vector_capacity_map_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical slash-method vector capacity same-path helper on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(94i32)
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_capacity_map_same_path_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_capacity_map_same_path_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /map/capacity") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical slash-method vector capacity same-path helper on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
/std/collections/vector/capacity([array<i32>] values) {
  return(95i32)
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_capacity_array_same_path_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_capacity_array_same_path_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /array/capacity") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical slash-method vector capacity same-path helper on string receiver") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
/std/collections/vector/capacity([string] values) {
  return(96i32)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_vector_capacity_string_same_path_helper.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_capacity_string_same_path_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /string/capacity") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical slash-method vector capacity on map receiver before emission") {
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
      writeTemp("compile_cpp_canonical_slash_vector_capacity_map_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_capacity_map_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /map/capacity") !=
        std::string::npos);
}

TEST_CASE("rejects canonical slash-method vector capacity on map receiver in C++ emitter") {
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
      writeTemp("compile_cpp_canonical_slash_vector_capacity_map_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_vector_capacity_map_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /map/capacity") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects alias slash-method vector capacity same-path helper on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/vector/capacity([map<i32, i32>] values) {
  return(98i32)
}

[return<int>]
main() {
  return(wrapMap()./vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_slash_vector_capacity_map_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_alias_slash_vector_capacity_map_same_path_helper_exe")
          .string();

  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_slash_vector_capacity_map_same_path_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /map/capacity") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects alias slash-method vector capacity on map receiver before emission") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_slash_vector_capacity_map_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_alias_slash_vector_capacity_map_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /map/capacity") != std::string::npos);
}

TEST_CASE("rejects alias slash-method vector capacity on map receiver in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_slash_vector_capacity_map_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_slash_vector_capacity_map_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /map/capacity") != std::string::npos);
}

TEST_CASE("C++ emitter infers wrapper collection builtin fallback") {
  const std::string source = R"(
wrapMap() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  return(values)
}

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(count(wrapMap()), wrapMap().count()),
              plus(plus(count(wrapVector()), wrapVector().count()),
                   plus(capacity(wrapVector()), wrapVector().capacity()))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_count_capacity_builtin_fallback.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_inferred_wrapper_count_capacity_builtin_fallback.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter keeps bare vector count methods on same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(14i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_count_method_same_path.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_bare_vector_count_method_same_path_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 14);
}

TEST_CASE("C++ emitter keeps bare vector capacity methods on same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/capacity([vector<i32>] values) {
  return(15i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_capacity_method_same_path.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_bare_vector_capacity_method_same_path_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 15);
}

TEST_CASE("C++ emitter keeps bare vector count methods on emitter fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_count_method_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_bare_vector_count_method_deleted_stub.txt").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("keeps bare vector count methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_count_method_deleted_stub_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_bare_vector_count_method_deleted_stub_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("C++ emitter rejects bare vector capacity methods before emitter fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_capacity_method_same_path_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_bare_vector_capacity_method_same_path_reject.txt").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("name=capacity, args=1, method=false") != std::string::npos);
}

TEST_CASE("rejects bare vector capacity methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_capacity_method_same_path_reject_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_capacity_method_same_path_reject.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("name=capacity, args=1, method=false") != std::string::npos);
}

TEST_CASE("rejects wrapper vector count methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_vector_count_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_vector_count_method_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("rejects wrapper vector count methods without helper before emission in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_vector_count_method_deleted_stub_cpp.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_vector_count_method_deleted_stub.txt").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("C++ emitter rejects wrapper vector count slash-method chains before receiver typing") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector()./vector/count().tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_count_slash_chain_unknown_method.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_vector_count_slash_chain_unknown_method.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /vector/count") !=
        std::string::npos);
}

TEST_CASE("compiles and runs wrapper bare vector count through imported stdlib helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapVector()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_bare_vector_count_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_count_imported_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("C++ emitter rejects wrapper bare vector count calls without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapVector()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_bare_vector_count_call_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_count_call_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("C++ emitter rejects namespaced vector count on wrapper map target before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_vector_count_wrapper_map_target_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_namespaced_vector_count_wrapper_map_target_reject.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper explicit vector count alias calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_explicit_vector_count_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_explicit_vector_count_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("call=/vector/count") != std::string::npos);
}

TEST_CASE("C++ emitter rejects wrapper explicit vector count capacity calls without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(/vector/count(wrapVector()),
              /std/collections/vector/capacity(wrapVector())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_explicit_vector_count_capacity_call_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_explicit_vector_count_capacity_call_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper explicit vector count capacity aliases when only canonical helpers exist in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(/vector/count(wrapVector()),
              /vector/capacity(wrapVector())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_explicit_vector_count_capacity_alias_canonical_only_deleted_stub.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_explicit_vector_count_capacity_alias_canonical_only_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("call=/vector/count") != std::string::npos);
}

TEST_CASE("rejects wrapper bare vector count calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_bare_vector_count_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_count_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_SUITE_END();
