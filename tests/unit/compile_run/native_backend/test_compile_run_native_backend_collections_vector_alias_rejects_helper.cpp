#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native vector helper method expression legacy alias forwarding") {
  const std::string source = R"(
[public return<int>]
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
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects native vector alias named-argument compatibility template forwarding") {
  const std::string source = R"(
[return<int> effects(none)]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[public return<int> effects(none)]
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
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown named argument: marker") != std::string::npos);
}

TEST_CASE("rejects native wrapper temporary templated vector method compatibility template forwarding") {
  const std::string source = R"(
[return<int> effects(none)]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[public return<int> effects(none)]
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

TEST_CASE("native array alias count through same-path helper") {
  const std::string source = R"(
[return<int> effects(none)]
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

TEST_CASE("rejects native vector alias templated forwarding past non-templated compatibility helper") {
  const std::string source = R"(
[return<int> effects(none)]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[public return<int> effects(none)]
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

TEST_CASE("native vector access checks bounds") {
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

TEST_CASE("native vector access rejects negative index") {
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

TEST_CASE("native vector method call") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(none)]
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

TEST_CASE("native bare vector at through imported stdlib helper") {
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
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
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
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("native bare vector count through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(/std/collections/vector/count<i32>(values), /std/collections/vector/count<i32>(values)))
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

TEST_SUITE_END();
#endif
