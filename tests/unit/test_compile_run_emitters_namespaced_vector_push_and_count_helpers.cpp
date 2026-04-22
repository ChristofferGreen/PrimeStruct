#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects reordered namespaced vector push call expression compatibility alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  return(/vector/push(3i32, values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_reordered_namespaced_vector_push_call_expr_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_reordered_namespaced_vector_push_call_expr_shadow_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("compiles and runs auto-inferred std namespaced vector push canonical precedence in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push([value] payload, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_push_expr_named_receiver_precedence_auto.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_push_expr_named_receiver_precedence_auto_exe")
          .string();
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_std_namespaced_vector_push_expr_named_receiver_precedence_auto_err.txt")
                                  .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs auto-inferred std namespaced vector push canonical definition in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push([value] payload, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_push_expr_named_receiver_canonical_definition_auto.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_push_expr_named_receiver_canonical_definition_auto_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects auto-inferred std namespaced count helper return mismatch in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

  [effects(heap_alloc), return<int>]
  main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_namespaced_vector_count_receiver_precedence_auto.prime",
                                        source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_count_receiver_precedence_auto_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("return type mismatch: expected i32") != std::string::npos);
}

TEST_CASE("compiles std namespaced count helper canonical fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_namespaced_vector_count_canonical_fallback_auto.prime",
                                        source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_count_canonical_fallback_auto_exe")
          .string();
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_std_namespaced_vector_count_canonical_fallback_auto_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs std namespaced count expression canonical precedence in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_namespaced_vector_count_expr_receiver_precedence.prime",
                                        source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_count_expr_receiver_precedence_exe")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles std namespaced count expression canonical fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_namespaced_vector_count_expr_canonical_fallback.prime",
                                        source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_count_expr_canonical_fallback_exe")
          .string();
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_std_namespaced_vector_count_expr_canonical_fallback_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects std namespaced count non-builtin compatibility fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_namespaced_count_non_builtin_compat_fallback.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_std_namespaced_count_non_builtin_compat_fallback_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_std_namespaced_count_non_builtin_compat_fallback_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects std namespaced count non-builtin compatibility fallback type mismatch in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_count_non_builtin_compat_fallback_mismatch.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_cpp_std_namespaced_count_non_builtin_compat_fallback_mismatch_exe")
                                  .string();
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_std_namespaced_count_non_builtin_compat_fallback_mismatch_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects vector namespaced count non-builtin array fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(31i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_namespaced_count_non_builtin_array_fallback_rejected.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_vector_namespaced_count_non_builtin_array_fallback_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs std namespaced capacity expression canonical precedence in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_capacity_expr_receiver_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_capacity_expr_receiver_precedence_exe")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs std namespaced capacity expression canonical fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_capacity_expr_canonical_fallback.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_capacity_expr_canonical_fallback_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects user vector mutator bool positional call shadow in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(true, values)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_bool_positional_call_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_vector_mutator_bool_positional_call_shadow_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("C++ emitter mutator rewrite keeps known vector receiver leading names") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
/i32/push([i32] value, [vector<i32> mut] values) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [i32] index{7i32}
  push(values, index)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_mutator_known_receiver_no_reorder.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_mutator_known_receiver_no_reorder_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects user vector access named call shadow in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at([index] 1i32, [values] values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_access_named_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_vector_access_named_call_shadow_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_vector_access_named_call_shadow_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("compiles and runs auto-inferred std namespaced access helper canonical precedence in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_access_expr_named_receiver_precedence_auto.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_access_expr_named_receiver_precedence_auto_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs auto-inferred std namespaced access helper canonical definition in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_access_expr_named_receiver_canonical_fallback_auto.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_access_expr_named_receiver_canonical_fallback_auto_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs wrapper std namespaced access helper named receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(plus(index, 30i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/at([index] 2i32, [values] wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_std_namespaced_vector_access_named_receiver.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_wrapper_std_namespaced_vector_access_named_receiver_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 32);
}

TEST_CASE("compiles and runs std collections vectorAt wrapper in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32, 6i32)}
  return(vectorAt<i32>(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_collections_vectorat_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_std_collections_vectorat_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("rejects wrapper std namespaced access helper named receiver without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/at([index] 1i32, [values] wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_std_namespaced_vector_access_named_receiver_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_std_namespaced_vector_access_named_receiver_reject_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("rejects removed vector access alias named arguments in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/at([values] values, [index] 0i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_removed_vector_access_alias_named_args.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_removed_vector_access_alias_named_args_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_removed_vector_access_alias_named_args_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") !=
        std::string::npos);
}

TEST_CASE("rejects removed vector access alias at_unsafe named arguments in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/at_unsafe([values] values, [index] 0i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_removed_vector_access_alias_at_unsafe_named_args.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_removed_vector_access_alias_at_unsafe_named_args_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_removed_vector_access_alias_at_unsafe_named_args_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /vector/at_unsafe") !=
        std::string::npos);
}

TEST_CASE("rejects user vector access positional call shadow in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(73i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at(1i32, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_access_positional_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_vector_access_positional_call_shadow_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_vector_access_positional_call_shadow_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("rejects user map access string positional call shadow in C++ emitter") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(84i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_access_string_positional_call_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_map_access_string_positional_call_shadow_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend requires integer indices for at") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects later map receiver positional shadow without canonical reorder") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(86i32)
}

[return<int>]
/string/at([string] values, [map<string, i32>] key) {
  return(87i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_access_later_receiver_precedence.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_map_access_later_receiver_precedence_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}


TEST_SUITE_END();
