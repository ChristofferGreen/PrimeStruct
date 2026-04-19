#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native auto-inferred named access helper receiver precedence in lowering") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/get([vector<i32>] values, [string] index) {
  return(12i32)
}

[effects(heap_alloc), return<int>]
/string/get([string] values, [vector<i32>] index) {
  return(98i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [string] index{"tag"raw_utf8}
  [auto] inferred{get([index] index, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_access_expr_named_receiver_precedence_auto.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_user_access_expr_named_receiver_precedence_auto_exe")
          .string();
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_user_access_expr_named_receiver_precedence_auto_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/"
                               "saturate/convert/pointer/assign/increment/decrement calls in expressions "
                               "(call=/get") != std::string::npos);
}

TEST_CASE("rejects native auto-inferred std namespaced access helper compatibility alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_namespaced_vector_access_expr_named_receiver_precedence_auto.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_access_expr_named_receiver_precedence_auto_exe")
          .string();

  const std::string outPath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_access_expr_named_receiver_precedence_auto_out.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  const std::string diagnostics = readFile(outPath);
  CHECK(diagnostics.find("return type mismatch") != std::string::npos);
  CHECK((diagnostics.find("expected int") != std::string::npos ||
         diagnostics.find("expected i32") != std::string::npos));
}

TEST_CASE("compiles and runs native auto-inferred std namespaced access helper canonical definition") {
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
      writeTemp("compile_native_std_namespaced_vector_access_expr_named_receiver_canonical_fallback_auto.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_std_namespaced_vector_access_expr_named_receiver_canonical_fallback_auto_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native user vector pop call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  pop(values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_pop_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_pop_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native user vector pop method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.pop()
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_pop_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_pop_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user vector reserve call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reserve(values, 3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_reserve_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_reserve_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user vector reserve method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.reserve(3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_reserve_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_reserve_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user vector clear call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  clear(values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_clear_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_clear_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native user vector clear method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.clear()
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_clear_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_clear_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user vector remove_at call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_at(values, 1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_remove_at_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_remove_at_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native user vector remove_at method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_at(1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_remove_at_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_remove_at_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user vector remove_swap call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_remove_swap_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_remove_swap_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native indexed vector assign") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [i32] index{1i32}
  assign(values[index], 7i32)
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_native_indexed_vector_assign.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_indexed_vector_assign_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native user vector remove_swap method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_swap(1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_vector_remove_swap_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_remove_swap_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("grows native vector reserve beyond initial capacity") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reserve(values, 3i32)
  push(values, 9i32)
  return(plus(count(values), capacity(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_grows.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_reserve_grows_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("preserves native vector values across reserve growth") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 8i32)}
  reserve(values, 4i32)
  push(values, 16i32)
  return(plus(plus(values[0i32], values[1i32]), values[2i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_growth_preserves_values.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_reserve_growth_preserves_values_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 28);
}

TEST_CASE("grows native vector push beyond initial capacity") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(plus(count(values), capacity(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_push_grows.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_push_grows_exe").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("preserves native vector values across push growth") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(5i32)}
  push(values, 7i32)
  return(plus(values[0i32], values[1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_push_growth_preserves_values.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_push_growth_preserves_values_exe").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs native vector literal at local dynamic limit") {
  auto buildVectorLiteralArgs = [](int count) {
    std::string args;
    args.reserve(static_cast<size_t>(count) * 6);
    for (int i = 0; i < count; ++i) {
      if (i > 0) {
        args += ", ";
      }
      args += "1i32";
    }
    return args;
  };

  const std::string source = std::string(
      "import /std/collections/*\n\n"
      "[effects(heap_alloc), return<int>]\n"
      "main() {\n"
      "  [vector<i32> mut] values{vector<i32>(") +
                             buildVectorLiteralArgs(256) +
                             ")}\n"
                             "  return(convert<i32>(and(equal(count(values), 256i32), equal(capacity(values), "
                             "256i32))))\n"
                             "}\n";
  const std::string srcPath = writeTemp("compile_native_vector_literal_local_limit.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_literal_local_limit_exe").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}


TEST_SUITE_END();
#endif
