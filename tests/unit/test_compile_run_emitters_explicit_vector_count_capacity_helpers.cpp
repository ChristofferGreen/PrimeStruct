#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects wrapper explicit vector capacity calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/capacity(wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_explicit_vector_capacity_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_explicit_vector_capacity_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string err = readFile(errPath);
  CHECK(err.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("compiles and runs wrapper explicit vector count capacity aliases through same-path helpers in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(18i32)
}

[return<int>]
/vector/capacity([vector<i32>] values) {
  return(19i32)
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
      writeTemp("compile_cpp_wrapper_explicit_vector_count_capacity_alias_same_path.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_wrapper_explicit_vector_count_capacity_alias_same_path_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 37);
}

TEST_CASE("compiles and runs local explicit vector count capacity calls through same-path helpers in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(14i32)
}

[return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(15i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values),
              /std/collections/vector/capacity(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_explicit_vector_count_capacity_same_path.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_local_explicit_vector_count_capacity_same_path_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 29);
}

TEST_CASE("C++ emitter rejects local explicit vector count capacity calls without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values),
              /std/collections/vector/capacity(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_explicit_vector_count_capacity_same_path_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_local_explicit_vector_count_capacity_same_path_reject.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects local explicit vector count capacity calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values),
              /std/collections/vector/capacity(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_explicit_vector_count_capacity_same_path_reject_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_explicit_vector_count_capacity_same_path_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects namespaced wrapper vector capacity vector target without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/capacity(wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_wrapper_vector_capacity_vector_target_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_namespaced_wrapper_vector_capacity_vector_target_reject_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("compiles and runs wrapper bare vector capacity through imported stdlib helper in C++ emitter" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_bare_vector_capacity_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_capacity_imported_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("C++ emitter rejects wrapper bare vector capacity calls without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_bare_vector_capacity_call_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_capacity_call_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper bare vector capacity calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_bare_vector_capacity_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_capacity_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects bare vector capacity methods without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_capacity_method_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_bare_vector_capacity_method_same_path_reject.txt").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  const std::string out = readFile(outPath);
  CHECK(out.find("native backend only supports") != std::string::npos);
  CHECK(out.find("name=capacity") != std::string::npos);
}

TEST_CASE("rejects bare vector capacity methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_capacity_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_capacity_method_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string err = readFile(errPath);
  CHECK(err.find("native backend only supports") != std::string::npos);
  CHECK(err.find("name=capacity") != std::string::npos);
}

TEST_CASE("rejects wrapper vector capacity methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_vector_capacity_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_vector_capacity_method_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string err = readFile(errPath);
  CHECK(err.find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE("rejects wrapper vector capacity methods without helper before emission in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_capacity_method_deleted_stub_cpp.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_vector_capacity_method_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  const std::string out = readFile(outPath);
  CHECK(out.find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE(
    "C++ emitter rejects wrapper vector capacity slash-method chains before receiver typing") {
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
  return(wrapVector()./vector/capacity().tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_capacity_slash_chain_unknown_method.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_vector_capacity_slash_chain_unknown_method.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  const std::string out = readFile(outPath);
  CHECK(out.find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE("C++ emitter rejects duplicate local canonical slash-method vector capacity overloads") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([array<i32>] values) {
  return(42i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([string] values) {
  return(43i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  [string] text{"abc"raw_utf8}
  return(plus(plus(values./std/collections/vector/capacity(),
                    items./std/collections/vector/capacity()),
              text./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_capacity_same_path.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_capacity_same_path_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("duplicate definition: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("C++ emitter rejects local canonical slash-method vector capacity on map receiver before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_capacity_map_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_capacity_map_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("capacity requires vector target") !=
        std::string::npos);
}

TEST_CASE("rejects local canonical slash-method vector capacity on string receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_capacity_string_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_capacity_string_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("capacity requires vector target") !=
        std::string::npos);
}

TEST_CASE("rejects local alias slash-method vector capacity on string receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_alias_slash_vector_capacity_string_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_alias_slash_vector_capacity_string_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /string/capacity") != std::string::npos);
}

TEST_CASE("rejects local alias slash-method vector capacity on array receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_alias_slash_vector_capacity_array_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_alias_slash_vector_capacity_array_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("compiles and runs vector alias count capacity slash methods in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count(),
              values./vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_count_capacity_slash_methods_alias_same_path.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_count_capacity_slash_methods_alias_same_path.exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 110);
}

TEST_CASE("compiles and runs stdlib namespaced vector count capacity slash methods in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/count(),
              values./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_count_capacity_slash_methods.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_stdlib_vector_count_capacity_slash_methods_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 110);
}

TEST_CASE("C++ emitter rejects vector namespaced count capacity slash methods without same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count(),
              values./vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_count_capacity_slash_methods_missing_same_path.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_count_capacity_slash_methods_missing_same_path.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string err = readFile(errPath);
  CHECK(err.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("C++ emitter compiles stdlib namespaced vector count capacity slash methods without same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/count(),
              values./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_count_capacity_slash_methods_missing_same_path.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_vector_count_capacity_slash_methods_missing_same_path.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("rejects vector namespaced count capacity slash methods without same-path helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count(),
              values./vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_count_capacity_slash_methods_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_count_capacity_slash_methods_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string err = readFile(errPath);
  CHECK(err.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("compiles and runs stdlib namespaced vector count capacity slash methods without same-path helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/count(),
              values./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_count_capacity_slash_methods_deleted_stub_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_stdlib_vector_count_capacity_slash_methods_deleted_stub.exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("C++ emitter rejects cross-path vector count capacity slash methods before builtin fallback" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count(),
              values./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_cross_path_count_capacity_slash_methods_same_path_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_cross_path_count_capacity_slash_methods_same_path_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("rejects cross-path vector count capacity slash helper routing in C++ emitter" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count(),
              values./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_cross_path_count_capacity_slash_methods_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_cross_path_count_capacity_slash_methods_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("compiles and runs stdlib namespaced vector access slash methods in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(plus(index, 50i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(2i32),
              values./std/collections/vector/at_unsafe(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_access_slash_methods.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_stdlib_vector_access_slash_methods_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 93);
}

TEST_CASE("C++ emitter rejects stdlib namespaced vector access slash methods without helper before lowering") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(1i32),
              values./std/collections/vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_access_slash_methods_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_vector_access_slash_methods_no_helper_cpp.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects stdlib namespaced vector access slash methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(1i32),
              values./std/collections/vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_access_slash_methods_no_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_vector_access_slash_methods_no_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_SUITE_END();
