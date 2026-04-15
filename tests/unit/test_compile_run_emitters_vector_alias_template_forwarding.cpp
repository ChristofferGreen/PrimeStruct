#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects vector alias arity-mismatch compatibility template forwarding in C++ emitter") {
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
  return(plus(/vector/count(values, true), values.count(true)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_arity_mismatch_compatibility_template_forwarding_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_arity_mismatch_compatibility_template_forwarding_reject_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("argument count mismatch") != std::string::npos);
}

TEST_CASE("compiles vector alias compatibility template forwarding on bool->i32 conversion in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [i32] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, true), values.count(true)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_implicit_canonical_forwarding_bool_type_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_implicit_canonical_forwarding_bool_type_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 14);
}

TEST_CASE("rejects vector alias compatibility template forwarding on non-bool type mismatch in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [string] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, 1i32), values.count(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_implicit_canonical_forwarding_non_bool_type_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_implicit_canonical_forwarding_non_bool_type_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on struct type mismatch in C++ emitter") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [MarkerB] marker{MarkerB()}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_implicit_canonical_forwarding_struct_type_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_implicit_canonical_forwarding_struct_type_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on constructor temporary struct mismatch in C++ emitter") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, MarkerB()), values.count(MarkerB())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_constructor_struct_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_canonical_forwarding_constructor_struct_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on method-call temporary struct mismatch in C++ emitter") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
Holder() {}

[return<MarkerB>]
/Holder/fetch([Holder] self) {
  return(MarkerB())
}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [Holder] holder{Holder()}
  return(plus(/vector/count(values, holder.fetch()), values.count(holder.fetch())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_method_struct_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_canonical_forwarding_method_struct_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on chained method-call temporary struct mismatch in C++ emitter") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
Middle() {}
Holder() {}

[return<Middle>]
/Holder/first([Holder] self) {
  return(Middle())
}

[return<MarkerB>]
/Middle/next([Middle] self) {
  return(MarkerB())
}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [Holder] holder{Holder()}
  return(plus(/vector/count(values, holder.first().next()),
              values.count(holder.first().next())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_chained_method_struct_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_canonical_forwarding_chained_method_struct_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on array envelope element mismatch in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [array<i32>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [array<i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [array<i64>] marker{array<i64>(1i64)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_array_envelope_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_canonical_forwarding_array_envelope_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on map envelope mismatch in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [map<i32, string>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [map<i32, i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [map<i32, i64>] marker{map<i32, i64>(1i32, 2i64)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_map_envelope_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_canonical_forwarding_map_envelope_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on map envelope mismatch from call return in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i64>>]
makeMarker() {
  return(map<i32, i64>(1i32, 2i64))
}

[return<int>]
/vector/count([vector<i32>] values, [map<i32, string>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [map<i32, i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_map_call_return_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_canonical_forwarding_map_call_return_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on primitive mismatch from call return in C++ emitter") {
  const std::string source = R"(
[return<i32>]
makeMarker() {
  return(1i32)
}

[return<int>]
/vector/count([vector<i32>] values, [string] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_primitive_call_return_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_canonical_forwarding_primitive_call_return_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding when unknown expected meets primitive call return in C++ emitter") {
  const std::string source = R"(
Marker() {}

[return<i32>]
makeMarker() {
  return(1i32)
}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_unknown_expected_primitive_call_return.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_canonical_forwarding_unknown_expected_primitive_call_return_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding when unknown expected meets primitive binding in C++ emitter") {
  const std::string source = R"(
Marker() {}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [i32] marker{1i32}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_unknown_expected_primitive_binding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_canonical_forwarding_unknown_expected_primitive_binding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding when unknown expected meets vector envelope binding in C++ emitter") {
  const std::string source = R"(
Marker() {}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [vector<i32>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [vector<i32>] marker{vector<i32>(1i32)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_unknown_expected_vector_envelope_binding.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_canonical_forwarding_unknown_expected_vector_envelope_binding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector helper method expression legacy alias forwarding in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(plus(count(values), value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(plus(/vector/push(values, 2i32), values.push(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_helper_method_expression_canonical_stdlib_forwarding.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_helper_method_expression_canonical_stdlib_forwarding_err.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_helper_method_expression_canonical_stdlib_forwarding_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects vector alias named-argument compatibility template forwarding in C++ emitter") {
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
  return(plus(/vector/count([values] values, [marker] true),
              values.count([marker] true)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_named_argument_compatibility_template_forwarding_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_named_argument_compatibility_template_forwarding_reject_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown named argument: marker") != std::string::npos);
}

TEST_CASE("rejects wrapper temporary templated vector method canonical forwarding in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count<i32>(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_temp_templated_vector_method_canonical_forwarding.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_temp_templated_vector_method_canonical_forwarding_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects array alias templated forwarding to canonical vector helper in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/array/count<i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_alias_templated_vector_forwarding_rejected.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_alias_templated_vector_forwarding_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects stdlib templated vector count fallback to array alias in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/array/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count<i32>(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_templated_vector_call_array_fallback_rejected.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_stdlib_templated_vector_call_array_fallback_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs array alias count through same-path helper in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(46i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/count(wrapVector(), true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_alias_count_same_path_wrapper_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_alias_count_same_path_wrapper_vector_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 46);
}

TEST_CASE("compiles and runs array alias capacity through same-path helper in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/capacity([vector<i32>] values, [bool] marker) {
  return(48i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/capacity(wrapVector(), true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_alias_capacity_same_path_wrapper_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_alias_capacity_same_path_wrapper_vector_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("compiles and runs array alias at through same-path helper in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/at([vector<i32>] values, [i32] index, [bool] marker) {
  return(48i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at(wrapVector(), 1i32, true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_alias_at_same_path_wrapper_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_alias_at_same_path_wrapper_vector_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("compiles and runs array alias at_unsafe through same-path helper in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([vector<i32>] values, [i32] index, [bool] marker) {
  return(50i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at_unsafe(wrapVector(), 1i32, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_array_alias_at_unsafe_same_path_wrapper_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_alias_at_unsafe_same_path_wrapper_vector_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 50);
}

TEST_CASE("rejects array alias slash-method helper chains on vector receivers in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/array/count([vector<i32>] values) {
  return(77i32)
}

[return<int>]
/array/capacity([vector<i32>] values) {
  return(78i32)
}

[return<Marker>]
/array/at([vector<i32>] values, [i32] index) {
  return(Marker(plus(index, 10i32)))
}

[return<Marker>]
/array/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(plus(index, 20i32)))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(plus(values./array/count(), values./array/capacity()),
              plus(plus(values./array/at(1i32).tag(), values./array/at_unsafe(2i32).tag()),
                   plus(wrapVector()./array/at(0i32).tag(), wrapVector()./array/at_unsafe(1i32).tag()))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_array_alias_method_helpers_same_path_vector_receivers.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_array_alias_method_helpers_same_path_vector_receivers.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /Marker/tag parameter self") !=
        std::string::npos);
}

TEST_SUITE_END();
