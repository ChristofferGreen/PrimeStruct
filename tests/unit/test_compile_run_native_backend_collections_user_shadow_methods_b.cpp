#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles and runs native vector push helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  pop(values)
  reserve(values, 3i32)
  push(values, 9i32)
  return(plus(values[2i32], capacity(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_push.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_vector_push_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs native vector mutator method calls") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values.pop()
  values.reserve(3i32)
  values.push(9i32)
  values.remove_at(1i32)
  values.remove_swap(0i32)
  values.clear()
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_mutator_methods.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_mutator_methods_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native user push helper shadow") {
  const std::string source = R"(
[return<int>]
push([i32] left, [i32] right) {
  return(plus(left, right))
}

[return<int>]
main() {
  push(1i32, 2i32)
  return(push(4i32, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_push_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_push_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native user vector constructor shadow") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(vector([value] 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_constructor_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_constructor_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native user array constructor shadow") {
  const std::string source = R"(
[return<int>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(array([value] 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_array_constructor_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_array_constructor_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native user map constructor shadow") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  return(map([key] 4i32, [value] 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_map_constructor_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_map_constructor_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("rejects native builtin vector constructor named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(vector([value] 1i32))
}
)";
  const std::string srcPath = writeTemp("native_builtin_vector_constructor_named_args.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_builtin_vector_constructor_named_args_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("rejects native builtin array constructor named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(array([value] 1i32))
}
)";
  const std::string srcPath = writeTemp("native_builtin_array_constructor_named_args.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_builtin_array_constructor_named_args_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("rejects native builtin map constructor named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(map([key] 1i32, [value] 2i32))
}
)";
  const std::string srcPath = writeTemp("native_builtin_map_constructor_named_args.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_builtin_map_constructor_named_args_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("compiles and runs native namespaced vector count named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count([values] values))
}
)";
  const std::string srcPath = writeTemp("native_namespaced_vector_count_named_args.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_namespaced_vector_count_named_args_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native namespaced vector capacity named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity([values] values))
}
)";
  const std::string srcPath = writeTemp("native_namespaced_vector_capacity_named_args.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_namespaced_vector_capacity_named_args_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects native removed vector access alias named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/at([values] values, [index] 0i32))
}
)";
  const std::string srcPath = writeTemp("native_removed_vector_access_alias_named_args.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_removed_vector_access_alias_named_args_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("rejects native removed vector access alias at_unsafe named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/at_unsafe([values] values, [index] 0i32))
}
)";
  const std::string srcPath = writeTemp("native_removed_vector_access_alias_at_unsafe_named_args.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_removed_vector_access_alias_at_unsafe_named_args_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("rejects native namespaced vector count with soa_vector literal target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(/std/collections/vector/count(values))
}
)";
  const std::string srcPath = writeTemp("native_namespaced_vector_count_soa_vector_target.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_namespaced_vector_count_soa_vector_target_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("compiles and runs native user map constructor block shadow") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  map(4i32, 6i32) {
    assign(result, 7i32)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("compile_native_user_map_constructor_block_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_map_constructor_block_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native user vector constructor block shadow") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  vector(9i32) {
    assign(result, 4i32)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_constructor_block_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_constructor_block_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native user array constructor block shadow") {
  const std::string source = R"(
[return<int>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  array(9i32) {
    assign(result, 5i32)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("compile_native_user_array_constructor_block_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_array_constructor_block_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native user vector push call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(values, 3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_push_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_push_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects native reordered namespaced vector push call compatibility alias") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /vector/push(3i32, values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_reordered_namespaced_vector_push_call_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_reordered_namespaced_vector_push_call_shadow_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("push requires vector binding") != std::string::npos);
}

TEST_CASE("compiles and runs native std namespaced reordered mutator compatibility helper shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/push(3i32, values)
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_reordered_mutator_compat_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_std_namespaced_reordered_mutator_compat_shadow_exe")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user vector push bool positional call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [bool] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(true, values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_push_call_bool_positional_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_push_call_bool_positional_shadow_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user vector push call named shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push([value] 3i32, [values] values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_push_call_named_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_push_call_named_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user vector push method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.push(3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_push_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_push_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user vector push call expression shadow") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  return(push(values, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_push_call_expr_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_push_call_expr_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("rejects native reordered namespaced vector push call expression compatibility alias") {
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
      writeTemp("compile_native_reordered_namespaced_vector_push_call_expr_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_reordered_namespaced_vector_push_call_expr_shadow_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("push is only supported as a statement") != std::string::npos);
}


TEST_SUITE_END();
#endif
