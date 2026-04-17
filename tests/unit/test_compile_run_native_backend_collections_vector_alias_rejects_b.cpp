#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native vector helper method expression legacy alias forwarding") {
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
      writeTemp("compile_native_vector_helper_method_expression_canonical_stdlib_forwarding.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_vector_helper_method_expression_canonical_stdlib_forwarding_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_vector_helper_method_expression_canonical_stdlib_forwarding_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects native vector alias named-argument compatibility template forwarding") {
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
      writeTemp("compile_native_vector_alias_named_argument_compatibility_template_forwarding_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_alias_named_argument_compatibility_template_forwarding_reject_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown named argument: marker") != std::string::npos);
}

TEST_CASE("rejects native wrapper temporary templated vector method compatibility template forwarding") {
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
      writeTemp("compile_native_wrapper_temp_templated_vector_method_compatibility_forwarding_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_temp_templated_vector_method_compatibility_forwarding_reject_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects native array alias templated forwarding to canonical vector helper") {
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
  const std::string srcPath =
      writeTemp("compile_native_array_alias_templated_vector_forwarding_rejected.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_alias_templated_vector_forwarding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib templated vector count fallback to array alias") {
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
      writeTemp("compile_native_stdlib_templated_vector_call_array_fallback_rejected.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_templated_vector_call_array_fallback_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native array alias count through same-path helper") {
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
  const std::string srcPath =
      writeTemp("compile_native_array_alias_count_same_path_wrapper_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_alias_count_same_path_wrapper_vector_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 46);
}

TEST_CASE("compiles and runs native array alias capacity through same-path helper") {
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
  const std::string srcPath =
      writeTemp("compile_native_array_alias_capacity_same_path_wrapper_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_alias_capacity_same_path_wrapper_vector_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("compiles and runs native array alias at through same-path helper") {
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
  const std::string srcPath =
      writeTemp("compile_native_array_alias_at_same_path_wrapper_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_alias_at_same_path_wrapper_vector_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("compiles and runs native array alias at_unsafe through same-path helper") {
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
      writeTemp("compile_native_array_alias_at_unsafe_same_path_wrapper_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_alias_at_unsafe_same_path_wrapper_vector_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 50);
}

TEST_CASE("compiles and runs native array alias slash-method helpers through same-path helpers") {
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
      writeTemp("compile_native_array_alias_method_helpers_same_path_vector_receivers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_alias_method_helpers_same_path_vector_receivers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 219);
}

TEST_CASE("rejects native vector alias templated forwarding past non-templated compatibility helper") {
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
      writeTemp("compile_native_vector_templated_alias_forwarding_non_template_compat_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_templated_alias_forwarding_non_template_compat_reject_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native vector namespaced mutator alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(vectorAt<i32>(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_namespaced_mutator_alias.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_namespaced_mutator_alias_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects native vector namespaced count capacity access aliases without alias helpers") {
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
  const std::string srcPath = writeTemp("compile_native_vector_namespaced_count_access_aliases.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_namespaced_count_access_aliases_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("compiles and runs native vector access checks bounds") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32)}
  return(plus(100i32, values[9i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_bounds.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_vector_bounds_exe").string();
  const std::string errPath = (testScratchPath("") / "primec_native_vector_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("compiles and runs native vector access rejects negative index") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32)}
  return(plus(100i32, values[-1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_negative.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_vector_negative_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("runs native vector literal count method without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(vector<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_literal_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_literal_count_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native vector method call") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/vector/first([vector<i32>] items) {
  return(items[0i32])
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  return(values.first())
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_method_call.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_method_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("rejects native bare vector literal unsafe access through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  return(at_unsafe(vector<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_literal_unsafe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_literal_unsafe_err.txt").string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_literal_unsafe_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("only supports at() on numeric/bool/string arrays or vectors") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native bare vector at through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_vector_at_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_bare_vector_at_imported_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("rejects native bare vector at without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_vector_at_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_bare_vector_at_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects native bare vector at method without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_vector_at_method_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_bare_vector_at_method_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects native wrapper temporary vector at method without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().at(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_vector_at_method_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_vector_at_method_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("compiles and runs native bare vector at_unsafe through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_vector_at_unsafe_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_bare_vector_at_unsafe_imported_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("rejects native bare vector at_unsafe without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_vector_at_unsafe_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_bare_vector_at_unsafe_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects native bare vector at_unsafe method without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_bare_vector_at_unsafe_method_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_bare_vector_at_unsafe_method_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects native wrapper temporary vector at_unsafe method without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().at_unsafe(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_vector_at_unsafe_method_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_vector_at_unsafe_method_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("compiles and runs native bare vector count through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(vectorCount<i32>(values), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_count_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("rejects native bare vector count without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_count_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_count_import_requirement_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects native bare vector count method without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_count_method_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_count_method_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("rejects native wrapper temporary vector count method without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_vector_count_method_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_vector_count_method_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("rejects native wrapper vector count slash-method chains before receiver typing") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector()./vector/count().tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_vector_count_slash_chain_unknown_method.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_wrapper_vector_count_slash_chain_unknown_method_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("rejects native wrapper vector capacity slash-method chains before receiver typing") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector()./vector/capacity().tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_vector_capacity_slash_chain_unknown_method.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_vector_capacity_slash_chain_unknown_method_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE("rejects native local alias slash-method vector capacity on string receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_local_alias_slash_vector_capacity_string_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_local_alias_slash_vector_capacity_string_no_helper_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /string/capacity") != std::string::npos);
}

TEST_CASE("rejects native local alias slash-method vector capacity on array receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  return(items./vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_local_alias_slash_vector_capacity_array_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_local_alias_slash_vector_capacity_array_no_helper_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("compiles and runs native stdlib collection shim helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(7i32)}
  [map<i32, i32>] pairs{mapSingle<i32, i32>(3i32, 9i32)}
  [i32 mut] total{plus(vectorCount<i32>(values), mapCount<i32, i32>(pairs))}
  [vector<i32>] emptyValues{vectorNew<i32>()}
  [map<i32, i32>] emptyPairs{mapNew<i32, i32>()}
  assign(total, plus(total, plus(vectorCount<i32>(emptyValues), mapCount<i32, i32>(emptyPairs))))
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shims.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shims_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim multi constructors") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(5i32, 7i32, 9i32)}
  [map<i32, i32>] pairs{mapDouble<i32, i32>(1i32, 11i32, 2i32, 22i32)}
  [i32] vectorTotal{plus(vectorAt<i32>(values, 0i32), vectorAt<i32>(values, 2i32))}
  [i32] mapTotal{plus(mapAt<i32, i32>(pairs, 1i32), mapAtUnsafe<i32, i32>(pairs, 2i32))}
  return(plus(plus(vectorTotal, mapTotal), plus(vectorCount<i32>(values), mapCount<i32, i32>(pairs))))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_multi_ctor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_multi_ctor_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 52);
}

TEST_CASE("compiles and runs native templated stdlib collection return envelopes") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapVector<i32>(9i32)}
  [map<string, i32>] pairs{wrapMap<string, i32>("only"raw_utf8, 4i32)}
  return(plus(plus(vectorCount<i32>(values), mapCount<string, i32>(pairs)), mapAt<string, i32>(pairs, "only"raw_utf8)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_templated_returns.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_returns_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_SUITE_END();
#endif
