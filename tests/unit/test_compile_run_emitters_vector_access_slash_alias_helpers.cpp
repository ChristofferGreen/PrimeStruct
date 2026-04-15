#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("compiles and runs vector namespaced access slash methods through explicit alias helpers in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(plus(index, 50i32))
}

[effects(heap_alloc), return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/at(2i32),
              values./vector/at_unsafe(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_access_slash_methods_alias_same_path.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_access_slash_methods_alias_same_path_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 93);
}

TEST_CASE("C++ emitter rejects vector namespaced access slash methods without alias helper before lowering") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/at(1i32),
              values./vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_access_slash_methods_alias_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_access_slash_methods_alias_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects vector namespaced access slash methods without alias helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/at(1i32),
              values./vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_access_slash_methods_alias_no_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_access_slash_methods_alias_no_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects stdlib vector namespaced access slash methods without canonical helper in C++ emitter") {
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

TEST_CASE("rejects array compatibility access slash methods on vector receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./array/at(1i32),
              values./array/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_array_access_slash_methods_vector_no_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_array_access_slash_methods_vector_no_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /array/at") != std::string::npos);
}

TEST_CASE("rejects array compatibility access slash method chain before receiver typing in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./array/at_unsafe(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_array_access_slash_method_chain_vector_no_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_array_access_slash_method_chain_vector_no_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects wrapper-returned array compatibility access slash method chains before receiver typing in C++ emitter") {
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
  return(plus(wrapVector()./array/at(1i32).tag(),
              wrapVector()./array/at_unsafe(2i32).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_array_access_slash_method_chain_vector_no_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_array_access_slash_method_chain_vector_no_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /array/at") != std::string::npos);
}

TEST_CASE("C++ emitter rejects bare vector at methods without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_at_method_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_method_deleted_stub.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_method_deleted_stub_cpp.err").string();
  CHECK(runCommand(compileCmd + " 2> " + errPath) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects bare vector at methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_at_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_method_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("C++ emitter rejects bare vector at_unsafe methods without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_at_unsafe_method_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_unsafe_method_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_unsafe_method_deleted_stub_cpp.err")
          .string();
  CHECK(runCommand(compileCmd + " 2> " + errPath) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects wrapper vector at_unsafe methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().at_unsafe(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_at_unsafe_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_vector_at_unsafe_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("C++ emitter rejects wrapper bare vector at calls before deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at(wrapVector(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_bare_vector_at_call_deleted_stub.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_at_call_deleted_stub_cpp.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper bare vector at calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at(wrapVector(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_bare_vector_at_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_at_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects wrapper bare vector at_unsafe calls before deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at_unsafe(wrapVector(), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_bare_vector_at_unsafe_call_deleted_stub.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_at_unsafe_call_deleted_stub_cpp.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at_unsafe") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects wrapper explicit vector access calls before deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(/vector/at(wrapVector(), 1i32),
              /std/collections/vector/at_unsafe(wrapVector(), 2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_explicit_vector_access_call_deleted_stub.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_explicit_vector_access_call_deleted_stub_cpp.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper bare vector at_unsafe calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at_unsafe(wrapVector(), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_bare_vector_at_unsafe_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_at_unsafe_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at_unsafe") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper explicit vector access calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/at(wrapVector(), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_explicit_vector_access_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_explicit_vector_access_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects bare vector mutator calls without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  push(values, 4i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_mutator_call_nohelper_cpp.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_mutator_call_nohelper_cpp.err").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects bare vector mutator methods without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values.push(5i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_vector_mutator_method_nohelper_cpp.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_mutator_method_nohelper_cpp.err").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects explicit vector mutator alias methods without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./vector/push(4i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_mutator_alias_method_nohelper_cpp.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_explicit_vector_mutator_alias_method_nohelper_cpp.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/push") != std::string::npos);
}

TEST_CASE("rejects bare vector mutator call statements without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  push(values, 4i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_mutator_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_mutator_call_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("rejects bare vector mutator method statements without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values.push(4i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_mutator_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_mutator_method_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("rejects explicit vector mutator alias method statements without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./vector/push(4i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_mutator_alias_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_vector_mutator_alias_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/push") != std::string::npos);
}

TEST_CASE("rejects explicit canonical vector clear method without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/clear()
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_mutator_canonical_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_vector_mutator_canonical_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/clear") != std::string::npos);
}

TEST_CASE("rejects explicit canonical vector push method without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/push(4i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_push_canonical_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_vector_push_canonical_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/push") != std::string::npos);
}

TEST_CASE("rejects explicit canonical vector pop method without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/pop()
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_pop_canonical_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_vector_pop_canonical_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/pop") != std::string::npos);
}

TEST_CASE("rejects explicit canonical vector reserve method without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/reserve(9i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_reserve_canonical_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_vector_reserve_canonical_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/reserve") != std::string::npos);
}

TEST_CASE("rejects explicit canonical vector mutator method helper without access helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  assign(values[1i32], plus(value, 70i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/push(3i32)
  return(values[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_canonical_vector_mutator_method_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_canonical_vector_mutator_method_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects alias vector mutator methods with canonical-only helper before emission") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./vector/push(3i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_vector_mutator_method_canonical_only_same_path_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_vector_mutator_method_canonical_only_same_path_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/push") != std::string::npos);
}

TEST_CASE("rejects alias vector mutator methods with canonical-only helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./vector/push(3i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_vector_mutator_method_canonical_only_same_path_reject_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_vector_mutator_method_canonical_only_same_path_reject_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/push") != std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical vector mutator methods with alias-only helper before emission") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/push(3i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_mutator_method_alias_only_same_path_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_mutator_method_alias_only_same_path_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/push") != std::string::npos);
}

TEST_SUITE_END();
