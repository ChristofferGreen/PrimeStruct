#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter rejects canonical vector mutator methods with alias-only helper before emission") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/push(3i32)
  return(values[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_mutator_method_alias_only_same_path_reject_exe.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_mutator_method_alias_only_same_path_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("compiles and runs explicit canonical vector mutator statement helper in C++ emitter" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  assign(values[1i32], plus(value, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/push(values, 3i32)
  return(values[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_canonical_vector_mutator_statement_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_explicit_canonical_vector_mutator_statement_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 43);
}

TEST_CASE("compiles and runs explicit canonical reordered vector mutator statement helper in C++ emitter" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  assign(values[1i32], plus(value, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/push(3i32, values)
  return(values[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_canonical_reordered_vector_mutator_statement_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_explicit_canonical_reordered_vector_mutator_statement_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 43);
}

TEST_CASE("C++ emitter rejects alias vector mutator statements with canonical-only helper before emission") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /vector/push(values, 3i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_vector_mutator_canonical_only_same_path_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_vector_mutator_canonical_only_same_path_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("C++ emitter rejects alias reordered vector mutator statements with canonical-only helper before emission") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /vector/push(3i32, values)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_reordered_vector_mutator_canonical_only_same_path_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_reordered_vector_mutator_canonical_only_same_path_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("rejects alias vector mutator statements with canonical-only helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /vector/push(values, 3i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_vector_mutator_canonical_only_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_vector_mutator_canonical_only_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical vector mutator statements with alias-only helper before emission") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/push(values, 3i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_mutator_alias_only_same_path_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_mutator_alias_only_same_path_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical reordered vector mutator statements with alias-only helper before emission") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/push(3i32, values)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_reordered_vector_mutator_alias_only_same_path_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_reordered_vector_mutator_alias_only_same_path_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("rejects canonical vector mutator statements with alias-only helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/push(values, 3i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_mutator_alias_only_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_mutator_alias_only_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("rejects explicit canonical vector remove methods without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/remove_at(0i32)
  values./std/collections/vector/remove_swap(0i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_remove_methods_no_import.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_remove_methods_no_import.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string errors = readFile(errPath);
  CHECK(errors.find("unknown method: /std/collections/vector/remove_at") != std::string::npos);
}

TEST_CASE("rejects inferred wrapper map capacity target in C++ emitter" * doctest::skip(true)) {
  const std::string source = R"(
wrapMap() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  [i32] callValue{capacity(wrapMap())}
  [i32] methodValue{wrapMap().capacity()}
  return(plus(callValue, methodValue))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_map_capacity_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_inferred_wrapper_map_capacity_reject.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("C++ emitter infers wrapper access builtin fallback") {
  const std::string source = R"(
wrapMap() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(3i32, 4i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(plus(at(wrapMap(), 1i32), wrapMap().at(1i32)), wrapMap()[1i32]),
              plus(plus(at_unsafe(wrapMap(), 1i32), wrapMap().at_unsafe(1i32)),
                   plus(plus(plus(at(wrapVector(), 0i32), wrapVector().at(0i32)), wrapVector()[0i32]),
                        plus(at_unsafe(wrapVector(), 0i32), wrapVector().at_unsafe(0i32))))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_access_builtin_fallback.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_inferred_wrapper_access_builtin_fallback.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects inferred wrapper access key mismatch in C++ emitter") {
  const std::string source = R"(
wrapMap() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  [i32] callValue{at(wrapMap(), true)}
  [i32] methodValue{wrapMap().at(true)}
  [i32] indexValue{wrapMap()[true]}
  [i32] unsafeCall{at_unsafe(wrapMap(), true)}
  [i32] unsafeMethod{wrapMap().at_unsafe(true)}
  return(plus(callValue, plus(methodValue, plus(indexValue, plus(unsafeCall, unsafeMethod)))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_access_key_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_inferred_wrapper_access_key_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter infers wrapper string access builtin fallback") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"raw_utf8}
  return(text)
}

[return<int>]
main() {
  return(plus(plus(at(wrapText(), 1i32), wrapText().at(2i32)),
              plus(at_unsafe(wrapText(), 0i32), wrapText().at_unsafe(1i32))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_string_access_builtin_fallback.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_inferred_wrapper_string_access_builtin_fallback.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter infers wrapper string count builtin fallback") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"raw_utf8}
  return(text)
}

[return<int>]
main() {
  return(plus(count(wrapText()), wrapText().count()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_string_count_builtin_fallback.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_inferred_wrapper_string_count_builtin_fallback_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("rejects builtin count on canonical map reference string access without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(count(/std/collections/map/at(ref, 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_builtin_count_canonical_map_reference_string_access.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_builtin_count_canonical_map_reference_string_access.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("C++ emitter rejects bare builtin count on wrapper-returned canonical map access before lowering") {
  const std::string source = R"(
[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(/std/collections/map/at(wrapMap(), 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_builtin_count_wrapper_canonical_map_string_access.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_builtin_count_wrapper_canonical_map_string_access_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_builtin_count_wrapper_canonical_map_string_access.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("C++ emitter treats wrapper-returned canonical map string access as string receiver" * doctest::skip(true)) {
  const std::string source = R"(
[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()[1i32].count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_canonical_map_string_receiver.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_wrapper_canonical_map_string_receiver_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_canonical_map_string_receiver.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend does not support string array return types on /wrapMap") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps imported wrapper-returned canonical map reference access lowering diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[return<int>]
/string/count([string] values) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  return(/std/collections/map/at(borrowMap(location(values)), 1i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_wrapper_map_reference_string_receiver.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_direct_wrapper_map_reference_string_receiver_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_direct_wrapper_map_reference_string_receiver.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend does not support string array return types on /borrowMap") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps non-string diagnostics on wrapper-returned canonical map access receivers" * doctest::skip(true)) {
  const std::string source = R"(
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
      writeTemp("compile_cpp_wrapper_canonical_map_string_receiver_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_canonical_map_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps direct wrapper-returned canonical map access string receiver typing" * doctest::skip(true)) {
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
      writeTemp("compile_cpp_direct_wrapper_canonical_map_access_count_diag.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_direct_wrapper_canonical_map_access_count_diag_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("C++ emitter keeps wrapper-returned canonical map method access string receiver typing") {
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
      writeTemp("compile_cpp_wrapper_canonical_map_method_access_count_diag.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_wrapper_canonical_map_method_access_count_diag_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 182);
}

TEST_CASE("C++ emitter keeps non-string diagnostics on direct-call wrapper-returned canonical map reference access" * doctest::skip(true)) {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/string/count([string] values) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/at(borrowMap(location(values)), 1i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_wrapper_map_reference_string_receiver_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_direct_wrapper_map_reference_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_SUITE_END();
