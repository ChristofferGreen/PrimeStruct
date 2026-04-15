#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects vector alias templated forwarding past non-templated compatibility helper in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count<i32>(values, true), values.count<i32>(true)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_templated_alias_forwarding_non_template_compat.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_templated_alias_forwarding_non_template_compat_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects vector namespaced count capacity access aliases without helpers in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  [i32] c{/vector/count(values)}
  [i32] k{/vector/capacity(values)}
  [i32] first{/vector/at(values, 0i32)}
  [i32] second{/vector/at_unsafe(values, 1i32)}
  return(plus(plus(c, k), plus(first, second)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_namespaced_count_access_aliases.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_vector_namespaced_count_access_aliases.err").string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string errors = readFile(errPath);
  CHECK(errors.find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("C++ emitter lowers stdlib namespaced vector mutator statement through imported helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(values, 2i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_mutator_stmt.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_stdlib_namespaced_vector_mutator_stmt.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_push_call_helper") == std::string::npos);
}

TEST_CASE("C++ emitter rejects vector namespaced mutator alias statement without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_namespaced_mutator_stmt.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_vector_namespaced_mutator_stmt.err").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("C++ emitter keeps stdlib namespaced vector access builtin fallback") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(plus(/std/collections/vector/at(values, 0i32),
              /std/collections/vector/at_unsafe(values, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_access_fallback.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_stdlib_namespaced_vector_access_fallback.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t ps_fn_0(") != std::string::npos);
  CHECK(output.find("return ps_fn_0(stack, sp, heapSlots, heapAllocations, argc, argv);") != std::string::npos);
}

TEST_CASE("C++ emitter compiles stdlib namespaced vector at map target without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_at_map_helper.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_stdlib_namespaced_vector_at_map_helper.out")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(outPath).empty());
}

TEST_CASE("C++ emitter compiles stdlib namespaced map access and count helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [i32] c{/std/collections/map/count(values)}
  [i32] first{/std/collections/map/at(values, 1i32)}
  [i32] second{/std/collections/map/at_unsafe(values, 2i32)}
  return(plus(c, plus(first, second)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_map_access_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_stdlib_namespaced_map_access_count_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("C++ emitter compiles stdlib namespaced map helpers on canonical map references") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  [i32] c{/std/collections/map/count(ref)}
  [i32] first{/std/collections/map/at(ref, 1i32)}
  [i32] second{/std/collections/map/at_unsafe(ref, 2i32)}
  return(plus(c, plus(first, second)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_map_reference_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_stdlib_map_reference_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("C++ emitter keeps wrapper-returned canonical map references through reference binding") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [Reference</std/collections/map<i32, i32>>] ref{borrowMap(location(values))}
  [i32] c{ref.count()}
  [i32] first{ref.at(1i32)}
  [i32] second{ref.at_unsafe(2i32)}
  return(plus(c, plus(first, second)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_map_reference_method_sugar.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_wrapper_map_reference_method_sugar_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_map_reference_method_sugar_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("C++ emitter keeps canonical diagnostics on wrapper-returned canonical map reference method sugar") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{borrowMap(location(values))}
  return(ref.at(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_map_reference_method_sugar_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_map_reference_method_sugar_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("C++ emitter runs canonical map reference string access") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(ref[1i32].count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_reference_string_receiver.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_map_reference_string_receiver_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("C++ emitter keeps non-string diagnostics on canonical map reference access receivers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(ref[1i32].count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_reference_string_receiver_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_map_reference_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("rejects map namespaced count compatibility alias in C++ emitter") {
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
  const std::string srcPath = writeTemp("compile_cpp_map_namespaced_count_compatibility_alias_reject.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_map_namespaced_count_compatibility_alias_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_map_namespaced_count_compatibility_alias_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("rejects map namespaced contains compatibility alias in C++ emitter") {
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
      writeTemp("compile_cpp_map_namespaced_contains_compatibility_alias_reject.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_map_namespaced_contains_compatibility_alias_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_map_namespaced_contains_compatibility_alias_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/contains") != std::string::npos);
}

TEST_CASE("rejects map namespaced tryAt compatibility alias in C++ emitter") {
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
      writeTemp("compile_cpp_map_namespaced_try_at_compatibility_alias_reject.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_map_namespaced_try_at_compatibility_alias_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_map_namespaced_try_at_compatibility_alias_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/tryAt") != std::string::npos);
}

TEST_CASE("rejects stdlib namespaced map count alias fallback without import in C++ emitter") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_alias_fallback_without_import.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_namespaced_map_count_alias_fallback_without_import.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("rejects map namespaced at compatibility alias in C++ emitter without explicit alias") {
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
  const std::string srcPath = writeTemp("compile_cpp_map_namespaced_at_compatibility_alias_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_map_namespaced_at_compatibility_alias.err").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("rejects map namespaced at unsafe compatibility alias in C++ emitter without explicit alias") {
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
      writeTemp("compile_cpp_map_namespaced_at_unsafe_compatibility_alias_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_map_namespaced_at_unsafe_compatibility_alias.err")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical direct map access before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(plus(/std/collections/map/at(values, 1i32),
              /std/collections/map/at_unsafe(values, 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_map_access_deleted_stub.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_map_access_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("rejects canonical direct map access without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(plus(/std/collections/map/at(values, 1i32),
              /std/collections/map/at_unsafe(values, 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_map_access_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_map_access_deleted_stub_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("C++ emitter rejects direct builtin count on canonical map access without helper before lowering") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "hello"utf8, 2i32, "bye"utf8)}
  return(plus(count(/std/collections/map/at(values, 1i32)),
              count(/std/collections/map/at_unsafe(values, 2i32))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_map_access_count_deleted_stub.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_map_access_count_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("C++ emitter rejects direct builtin contains on canonical map access before deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, map<i32, i32>>] values{map<i32, map<i32, i32>>()}
  return(plus(contains(/std/collections/map/at(values, 1i32), 2i32),
              contains(/std/collections/map/at_unsafe(values, 2i32), 3i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_map_access_contains_deleted_stub.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_map_access_contains_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: contains") != std::string::npos);
}

TEST_CASE("rejects direct builtin contains on canonical map access without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, map<i32, i32>>] values{map<i32, map<i32, i32>>()}
  return(plus(contains(/std/collections/map/at(values, 1i32), 2i32),
              contains(/std/collections/map/at_unsafe(values, 2i32), 3i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_map_access_contains_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_map_access_contains_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: contains") != std::string::npos);
}

TEST_CASE("C++ emitter rejects wrapper-returned map access contains receivers before deleted stubs") {
  const std::string source = R"(
[return</std/collections/map<i32, map<i32, i32>>>]
wrapMap() {
  return(map<i32, map<i32, i32>>())
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(contains(wrapMap()./std/collections/map/at(1i32), 2i32),
              contains(wrapMap()./std/collections/map/at_unsafe(2i32), 3i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_slash_method_map_access_contains_deleted_stub.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_slash_method_map_access_contains_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: contains") != std::string::npos);
}

TEST_CASE("rejects wrapper-returned slash-method map access contains without helper in C++ emitter") {
  const std::string source = R"(
[return</std/collections/map<i32, map<i32, i32>>>]
wrapMap() {
  return(map<i32, map<i32, i32>>())
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(contains(wrapMap()./std/collections/map/at(1i32), 2i32),
              contains(wrapMap()./std/collections/map/at_unsafe(2i32), 3i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_slash_method_map_access_contains_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_slash_method_map_access_contains_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: contains") != std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical slash-method map access before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(values./std/collections/map/at(1i32),
              values./std/collections/map/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_method_map_access_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_method_map_access_deleted_stub.cpp")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_method_map_access_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /map/at") != std::string::npos);
}

TEST_SUITE_END();
